# Ursatz GUI

A local web UI for the Ursatz platform. This repo is a host/shell around
independent tool modules, not a monolithic application.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`) — the only code
  allowed to call into Ursatz's persistence/query functions or the
  Analyzer library. Every tool
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
- `POST /api/analyzer/pieces`: accepts a MIDI or MusicXML file (`.mid`,
  `.midi`, `.musicxml`, `.xml`, `.mxl`) as the raw request body, the
  filename URL-encoded in the `X-Filename` header. The format is detected
  from the content (`input_format_detect`), not from the filename. The
  file is analyzed by the Analyzer library and stored as a piece (below).
  Returns `{"ok":true,"id":...,"filename":...}`. A body that is neither
  MIDI nor MusicXML gives `415` with `{"error": "unsupported format"}`; a
  file the Analyzer cannot import or analyze gives `422` with
  `{"error": "import failed"}`; nothing is stored in either case.
- `GET /api/analyzer/pieces`: lists the stored pieces, newest upload
  first, then by id:
  `{"ok":true,"pieces":[{"id","filename","format","uploaded_at"}]}`.
- `GET /api/analyzer/pieces/<id>`: one piece:
  `{"ok":true,"id","filename","format","uploaded_at","note_xml_ids":[...],"document":{...}}`.
  `document` is the Analyzer's analysis document (schema v9), inlined as
  stored. `note_xml_ids[i]` is the `<note xml:id>` the score export writes
  for note `i` of the source, in the order of the document's `note_ids`
  (a claim's `note_ids` are indices into this array). Unknown id returns
  `{"ok":false,"message":"piece not found"}`.
- `PATCH /api/analyzer/pieces/<id>`: body `{"filename":"<name>"}`. Rewrites
  the filename in the piece's `meta.json`; the source and the analysis are
  untouched. Returns `{"ok":true,"id":...,"filename":...}`, `404` for an
  unknown id, or `{"ok":false,...}` for an empty filename.
- `DELETE /api/analyzer/pieces/<id>`: removes the piece's directory.
- `GET /api/analyzer/pieces/<id>/musicxml`: the piece's score as MusicXML
  (`Content-Type: application/xml`), written by Ursatz's
  `musicxml_export_write_score` from the stored source: its parts and
  staves, its real meter (the MIDI time signatures or the score's own) and
  its key signatures. Tempo markings and dynamics are left out. Unknown id
  returns `404`. If the export fails the response is
  `{"ok":false,"message":"failed to export piece","code":"<MusicXmlExportResult name>"}`.
- `GET /api/analyzer/pieces/<id>/musicxml/meta`:
  `{"ok":true,"meter_defaulted":<bool>}`, true when the source gave no
  meter and the barlines come from a default 4/4 (the export then carries
  `ursatz:meter-defaulted`). Same errors as the export route.

The Analyzer library (`external/ursatz-analyzer`, linked as
`ursatz_analyzer`) is the only analysis path: `analysis_service.c` calls
`input_pipeline_import_file`, `run_analysis_pipeline` and
`analysis_document_build`, and the GUI keeps no analysis code of its own.
Analysis therefore changes only when the Analyzer submodule is bumped.

Each piece is a directory `pieces/<id>/` (ids are `piece-<16 lowercase hex>`
from `ursatz_gui_new_id` in `service/id_service.c`, redrawn if the
directory exists, at most 8 tries):

- `source.mid` or `source.musicxml`: the upload's bytes, written
  unchanged. A `.xml` or `.mxl` upload keeps its extension
  (`source.xml`, `source.mxl`).
- `analysis.json`: the Analyzer's analysis document.
- `meta.json`: `{"filename", "format": "midi"|"musicxml", "uploaded_at"
  (ISO 8601 UTC), "analyzer_version", "document_schema_version"}`.

Every file is written to `<name>.tmp` and renamed, and a failed analysis
removes the directory. Pieces saved by earlier versions (`pieces/<id>.db`)
are ignored: they are not listed and cannot be opened, so upload those files
again.

The Corpus tool wraps Ursatz's `corpus_api.h` (vendored via
`external/ursatz`):

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
Periods/Sentences, each with a count badge. Until the tabs follow the
analysis document's own sections, only Chords and Keys have content: they
list the document's `chords` and `keys.regions` (label and location, with
an "Analyzed" flag); the other tabs are empty. Selecting a tab renders a
plain-text list of that category's claims. Clicking a claim opens a detail
panel: a claim summary, an Evidence section (always rendered as an
explicit empty state), a Provenance section, and disabled Accept/Dispute
buttons (no Promotion endpoint exists yet).

The tab bar also includes a Score tab, alongside the claim-list tabs
rather than replacing any of them. Selecting it fetches
`GET /api/analyzer/pieces/<id>/musicxml` and renders it with
[OpenSheetMusicDisplay](https://opensheetmusicdisplay.org/), vendored as
a static UMD build under `frontend/public/vendor/opensheetmusicdisplay/`
and loaded via a plain `<script>` tag in `index.html` -- `frontend/`
has no `package.json`/bundler, so this matches the existing no-build-tool
pattern rather than introducing one. A caption below the score
("Barlines are approximate, not authored meter.") is shown whenever
`GET .../musicxml/meta` reports `meter_defaulted: true`, that is, when
the source file gave no meter.

Clicking a claim in a claim-list tab also highlights its target note(s)
in the Score tab (notehead color, via OSMD's `Note.NoteheadColor`) --
selection-from-claim-list only, not the reverse. OSMD's own graphical
model does not retain MusicXML `xml:id` (checked against the vendored
build), so matching correlates document order between the fetched
MusicXML and OSMD's own note traversal instead (see
`buildScoreNoteIndex`/`collectGraphicalNotesInOrder` in `app.js`) --
valid only while document order matches between the two. The export is
single-part, but it can hold several voices per measure since Ursatz
PR #43, so this should be rechecked on multi-voice pieces and made
id-based if it drifts. A claim
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

Requires CMake 3.20 or newer, a C11 compiler, SQLite development headers
and pkg-config. Ursatz is vendored as a git submodule at `external/ursatz`,
pinned at PR #142 (`ffd8a19`), the commit the Analyzer and Composer pin. The
Analyzer is a second submodule, `external/ursatz-analyzer`, pinned at PR #36
(`11263b7`).
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF).

`ctest` runs the backend tests and, when `node` is installed, the frontend
tests too (`frontend/tests/*.test.js`, registered as `frontend_<name>`).
Without node CMake prints one status line and skips them.

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.

## Out of scope

Native webview packaging, Generator tool code, and authentication.
Re-running analysis on an already-saved piece is not built yet (the
Library's "Re-run analysis" button is disabled).
