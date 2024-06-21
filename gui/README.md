# Ursatz GUI

A local web UI for the Ursatz platform. This repo is a host/shell around
independent tool modules, not a monolithic application.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`) — the only code
  allowed to call into Ursatz's persistence/query functions. Every tool
  goes through this layer; nothing calls Ursatz directly from route code.
  This boundary is the one thing this scaffold is strict about, because
  it is expensive to untangle later.
- **Tool modules** (`backend/src/routes/`) — independent GUI features
  (Analysis/Corpus now, Generator later). Each tool registers its own
  HTTP routes under an `/api/<tool_name>/...` prefix and never calls
  another tool directly. See `backend/include/ursatz_gui/tool.h` for the
  registration convention, and `backend/src/routes/health.c` as the
  reference tool.
- **Host/shell** (`backend/src/server.c`, `frontend/public/`) — starts
  the HTTP server, wires tool routes together, and serves the static
  frontend shell (top bar, tool sidebar, content area) that tool UIs
  render into.

New tool code goes in `backend/src/routes/`. New shared persistence or
query logic goes in `backend/src/service/`. If you find yourself calling
an Ursatz function from inside `routes/`, it belongs in `service/`
instead.

## Backend

C HTTP/JSON-RPC-style server using [civetweb](https://github.com/civetweb/civetweb)
(MIT licensed), fetched via CMake `FetchContent`. Civetweb was chosen over
hand-rolled sockets or heavier alternatives (e.g. libmicrohttpd, mongoose)
because it's a small, permissively licensed, single-purpose embedded HTTP
library with a simple C API and no GPL/commercial dual-licensing
complications.

The backend exposes:

- `GET /api/health/status`: calls through the persistence service to
  confirm `persistence_open` succeeds against a test database path,
  and returns JSON.
- `POST /api/analyzer/pieces`: accepts a raw MIDI file as the request
  body, runs it through Ursatz's `MIDIImporter`, `ChordIdentification`,
  and `KeyEstimation` (against the `common-practice-minimal`
  framework) via the analysis service, saves the resulting NoteEvent
  sequence and Interpretations, and returns the new piece's id.
- `GET /api/analyzer/pieces`: lists saved piece ids.
- `GET /api/analyzer/pieces/<id>`: returns one saved piece's detail --
  its NoteEvent count and every Chord/Key/Cadence/Motif/Phrase/Period/
  Sentence/Form/Thematic Return Interpretation saved for it, each with a
  `produced_by` field (`author`/`method`). `produced_by` is
  analyzer/method identity only, not a full Provenance-object reference:
  Ursatz's Interpretation carries a `ProvenanceReference`, but
  `KeyEstimation`/`ChordIdentification` always construct it as
  `PROVENANCE_REFERENCE_ABSENT`, and no resolvable Provenance record
  type exists upstream in Ursatz yet to point it at even if it were
  present. `author`/`method` are the only importer/analyzer identity
  Ursatz's Interpretation actually persists today. Unknown id returns
  `{"ok":false,...}`, same convention as the other Analyzer routes.

  Each claim also carries `highlighted_note_ids`: the MusicXML `xml:id`
  (see the `musicxml` route below) of every NoteEvent its target
  resolves to, used by the Analysis view's Score tab to highlight the
  claim's notes. This resolves two different ways depending on type:
  - `phrase`: the Interpretation's `target_id` *is* a NoteEvent id
    directly (Phrase Segmentation/Boundary Detection target the event
    itself), so this is derived on the fly from `target_id` -- no extra
    storage needed.
  - `cadence`/`motif`: the target is a `Structure`/`MotifOccurrence`
    that Ursatz never persists -- it is transient, created and destroyed
    within a single analysis run (confirmed: no
    `persistence_load_structure`/`persistence_load_motif` exists
    anywhere in `external/ursatz`). Its event ids are captured while
    that object is still alive, at analysis time, into a small
    ursatz-gui-owned table (`ursatz_gui_interpretation_note_targets`,
    keyed by `interpretation_id`, alongside the existing
    `ursatz_gui_interpretations` index table below) and served back from
    there. **Pieces analyzed before this table existed have no
    `highlighted_note_ids` for their cadence/motif claims** -- empty,
    not an error -- until reanalyzed (no backfill).
  - `key`/`chord`/`period`/`sentence`/`form`/`thematic_return`:
    `highlighted_note_ids` is always empty this pass.
    Period/Sentence/Form/Thematic Return claims are Relationship-shaped
    (their target chains 2-3 hops through further occurrence/section ids
    before reaching any NoteEvent, with no single note-range object at
    any point) -- excluded from highlighting for now, tracked as a
    follow-up; they still list normally in their claim-list tab.
- `PATCH /api/analyzer/pieces/<id>`: body `{"filename":"<name>"}`. Updates
  the piece's persisted original filename (the same field `GET` returns
  and the Library view displays) -- does not touch NoteEvents,
  Interpretations, or re-run any analysis. Returns
  `{"ok":true,"id":...,"filename":...}`, or `{"ok":false,...}` for an
  unknown id (404) or an empty filename.
- `GET /api/analyzer/pieces/<id>/musicxml`: opens the piece's
  `PersistenceHandle` via the same `pieces/<id>.db` lookup the other
  piece routes use, and returns Ursatz's `musicxml_export_write` output
  as the response body, `Content-Type: application/xml` (plain XML, not
  `application/vnd.recordare.musicxml+xml` -- this is an uncompressed
  single document, not a zipped `.mxl` package). Unknown id returns 404.
  If `musicxml_export_write` itself fails (e.g. a NoteEvent missing pitch
  spelling, or overlapping notes in the same voice), the response is
  `{"ok":false,"message":"failed to export piece","code":"<name>"}`,
  where `code` is the specific `MusicXmlExportResult` enum member name
  (`MUSICXML_EXPORT_ERR_NULL_ARG`, `MUSICXML_EXPORT_ERR_LOAD_FAILED`,
  `MUSICXML_EXPORT_ERR_MISSING_SPELLING`,
  `MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES`, or `MUSICXML_EXPORT_ERR_ALLOC`
  -- see `external/ursatz/src/export/musicxml_export.h`) that
  `musicxml_export_write` returned, also logged server-side.
  `musicxml_export_write` lays barlines on a synthetic 4/4 grid (Ursatz
  has no Meter/MeterMap data yet) and flags this via an
  `ursatz:synthetic-meter` `<miscellaneous-field>` -- see
  `external/ursatz/src/export/musicxml_export.h`. Every exported `<note>`
  carries an `xml:id` derived from its NoteEvent's EntityID (a NoteEvent
  split across the synthetic barline grid exports as several `<note>`
  elements, one base id plus `_a`/`_b`/... suffixed fragment ids) -- this
  is what `highlighted_note_ids` above and the Score tab's highlighting
  match against.
- `GET /api/analyzer/pieces/<id>/musicxml/meta`: same export, but returns
  `{"ok":true,"cosmetic_meter":<bool>}` instead of the XML body, so the
  frontend can read the synthetic-meter flag without a second client-side
  XML parse (OSMD already parses the full document to render it). Both
  endpoints call into the same `musicxml_export_write`, and report the
  same `code` field on export failure described above.

Ursatz's persistence layer has no "piece" concept (one `note_events`
table, one `interpretations` table, no grouping/filename metadata): the
Analyzer tool works around this by saving each piece to its own SQLite
file under `pieces/`, using the filename as the piece id. Listing scans
that directory rather than querying a tracking table.

Ursatz's persistence also has no way to enumerate the Interpretations
stored for a piece (`persistence_load_interpretation` only loads by a
known id). The analysis service works around this with its own small
`ursatz_gui_interpretations` index table, added to the same per-piece
SQLite file at analysis time -- ursatz-gui-owned schema, entirely
outside Ursatz's own schema/contract, recording which Interpretation ids
and types (`key`/`chord`/`cadence`/`motif`/`phrase`/`period`/`sentence`/
`form`/`thematic_return`) belong to that piece so the detail route knows
what to load. A sibling table,
`ursatz_gui_interpretation_note_targets`, records the captured
NoteEvent xml:ids behind `cadence`/`motif` claims' `highlighted_note_ids`
(see the `GET /api/analyzer/pieces/<id>` description above) -- the same
kind of ursatz-gui-owned workaround for a gap in what Ursatz itself
persists.

The Corpus tool wraps Ursatz's `corpus_api.h` (vendored via
`external/ursatz`, same as Analyzer's Persistence/MIDIImporter usage):

- `POST /api/corpus`: body `{"id":"<corpus-id>","name":"<name>"}`. Creates
  a Corpus. Returns `{"ok":true}` or `{"ok":false,"message":...}`.
- `GET /api/corpus`: lists all Corpora:
  `{"ok":true,"corpora":[{"id":...,"name":...},...]}`.
- `POST /api/corpus/<id>/pieces`: body `{"piece_id":"<EntityID>"}`. Adds a
  piece ref to the named Corpus. Adding a ref already present fails, same
  as corpus_api itself (fail-fast, not reinterpreted).
- `DELETE /api/corpus/<id>/pieces/<piece_id>`: removes a piece ref.
  Removing a ref that isn't a member fails the same way.
- `GET /api/corpus/<id>/pieces`: lists piece refs in a Corpus:
  `{"ok":true,"pieces":[{"id":...},...]}`.

All Corpus routes return HTTP 200 with `{"ok":false,...}` on failure, same
convention as Analyzer. `<id>`/`<piece_id>` path segments are validated
against the same alnum/`-`/`_` whitelist Analyzer uses for piece ids
(its own copy, kept local to `corpus_service.c` -- promoting it to a
shared validator is a reasonable follow-up, out of scope here).

Unlike Analyzer's per-piece SQLite files, Corpus uses one shared store,
`data/corpus.db`, opened once when the server starts (`server.c`) and
kept open for the life of the process rather than per-request. Corpus
does not validate that a piece ref actually exists in Persistence when
added -- corpus_api doesn't do this today, by design, and this route
layer doesn't add it.

## Frontend

Plain static HTML/JS/CSS served by the backend from `frontend/public/`.
No framework or build pipeline at this stage. Theming lives entirely in
`style.css` as CSS custom properties on `:root`, so a future theme
picker can swap colors without touching markup or `app.js`. Frontend
logic (`app.js`) makes no styling decisions.

The Corpus tool page is displayed as "Collections" in the sidebar and view
title -- the API/module/route naming stays `corpus`, only the label shown
to the user changed. It renders a grid of Collection cards (name only; the
API has no piece-count or last-updated field, so neither is shown on the
card). Clicking a card opens a detail view listing that Collection's piece
refs, reusing the same row component/styling as the Library view: each
row's filename is looked up by joining the piece ref's id against
`/api/analyzer/pieces`, falling back to the bare id when no match is
found. The only action on a Collection's piece row is removing it from
the Collection (`DELETE /api/corpus/<id>/pieces/<piece_id>`) -- Library's
piece-management actions (rename, delete, etc.) are not carried over,
since a Collection row represents membership, not the piece itself. New
piece refs are still added as plain id strings via the id input below the
list, and a piece may belong to any number of Collections.

Clicking a piece's filename in the Library view opens the Analysis view
for that piece. The top-level "Analysis" nav tab reopens whichever piece
was last viewed this session; before any piece has been opened it shows
a prompt pointing at Library instead of guessing a piece. The old flat
Piece Detail view this replaces has been removed. The Analysis view
shows: a header
(title/filename only -- no status, composer, opus, or meter, none of
which the API returns); a sidebar of analyzer name/version pairs (static
placeholder data, since no analyzer-registry endpoint exists); and a tab
bar over Chords, Keys, Cadences, Motifs, Phrases, Form, and
Periods/Sentences, each with a count badge. Tabs group real
`interpretations` by their `type` field (`GET /api/analyzer/pieces/<id>`)
-- Form's badge includes both `form` and `thematic_return` claims, and
Periods/Sentences' includes both `period` and `sentence`, since each pair
shares one tab. Selecting a tab renders a plain-text list of that
category's claims (label + provenance flag only -- the API returns no
span, confidence, or uncertainty fields to show). Clicking a claim opens
a detail panel: a claim summary, an Evidence section (always rendered as
an explicit empty state, since no Evidence data exists behind this
endpoint -- Registry marks Evidence Not Started), a Provenance section
(`produced_by.author`/`produced_by.method`/`provenance_state`, the only
provenance data an Interpretation actually carries -- not a full
Provenance record), and disabled Accept/Dispute buttons (no Promotion
endpoint exists yet).

The tab bar also includes a Score tab, alongside the claim-list tabs
rather than replacing any of them. Selecting it fetches
`GET /api/analyzer/pieces/<id>/musicxml` and renders it with
[OpenSheetMusicDisplay](https://opensheetmusicdisplay.org/), vendored as
a static UMD build under `frontend/public/vendor/opensheetmusicdisplay/`
and loaded via a plain `<script>` tag in `index.html` -- `frontend/`
has no `package.json`/bundler, so this matches the existing no-build-tool
pattern rather than introducing one. A caption below the score
("Barlines are approximate, not authored meter.") is shown whenever
`GET .../musicxml/meta` reports `cosmetic_meter: true`, which is every
piece today (see the route description above).

Clicking a claim in a claim-list tab also highlights its target note(s)
in the Score tab (notehead color, via OSMD's `Note.NoteheadColor`) --
selection-from-claim-list only, not the reverse. OSMD's own graphical
model does not retain MusicXML `xml:id` (checked against the vendored
build), so matching correlates document order between the fetched
MusicXML and OSMD's own note traversal instead (see
`buildScoreNoteIndex`/`collectGraphicalNotesInOrder` in `app.js`) --
valid only because this app's export is single-voice/single-part; it
would need to become genuinely id-based if that ever changes. A claim
with no `highlighted_note_ids` (`key`/`chord`/`period`/`sentence`/
`form`/`thematic_return`) clears any previous highlight and otherwise
does nothing -- it still appears normally in its tab, just with no
highlight action.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Ursatz is vendored as a git submodule at `external/ursatz`, same pattern
as `ursatz-analyzer`. `git submodule update --init --recursive` (or
`git clone --recurse-submodules` up front) is required before the first
build.

## Out of scope for this scaffold

Native webview packaging, Generator tool code, re-running analysis on
an already-saved piece, and authentication are explicitly not part of
this task.
