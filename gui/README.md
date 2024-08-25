# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Views

- **Library**: upload a file (`.mid`, `.midi`, `.musicxml`, `.xml`, `.mxl`),
  list the stored pieces, rename, delete, or run a piece's analysis again
  (the round arrow). Clicking a filename opens the Analysis view.
- **Analysis**: one piece. The top-level Analysis nav entry reopens the piece
  viewed last this session; before any piece has been opened it points at
  the Library.
- **Collections**: named groups of piece ids (the Corpus tool, below).
- **Generator**: not built yet.

### Analysis view

Above the tabs: the filename, the format, the analyzer version, the first 7
characters of the Ursatz commit, the note count and the measure count
(`meter.measure_count`). A "Re-run analysis" button runs the Analyzer again
on the stored source and reloads the view. The sidebar's Provenance panel
lists the document's metadata (source file, analyzer version, Ursatz commit,
document schema, input format, source type, meter, key signature, spelling,
importer warnings).

There is one tab per group of document sections, in this order:

| Tab | Sections |
| --- | --- |
| Keys | `keys`, `tonicizations`, `modulations`, `pivot_chords` |
| Chords | `chords`, `chord_vocabulary`, `non_chord_tones`, `prolongations` |
| Cadences | `cadences` |
| Progressions | `progressions`, `harmonic_rhythm` |
| Phrases | `phrases` |
| Themes and form | `themes`, `form` |
| Motives | `motives` |
| Melody | `melody`, `outer_voices` |
| Rhythm and meter | `meter`, `rhythm` |
| Texture | `texture`, `voice_leading` |
| Schemas | `schemas` |
| Markings | `markings` |
| Counterpoint | `counterpoint_check` |
| Notes | `notes` |

A tab shows the number of rows it holds. A tab whose sections are all empty
arrays or null is disabled and reads "(none)". A Score tab follows the
others.

Each item of an array section is one row: its name (the item's `label`, else
its `numeral`, `type` or `key`), where it is (`m<measure> b<beat>` for the
start and the end, `-` when the location is null), and a Fields toggle that
shows the item's remaining fields as formatted JSON. An object section
(`keys`, `form`, `meter`, `harmonic_rhythm`, `outer_voices`, `markings`,
`counterpoint_check`) is shown as one JSON block, and the arrays of claims
inside it (the key regions, the form's sections and returns, the soprano and
bass lines) are rows. Selecting a row with `note_ids` highlights those notes
in the Score tab.

The tab logic is in `frontend/public/analysis-tabs.js` (no DOM, loaded by
`index.html` before `app.js` and by the frontend tests); `app.js` renders it.

### Score tab

Fetches `GET /api/analyzer/pieces/<id>/musicxml` and renders it with
[OpenSheetMusicDisplay](https://opensheetmusicdisplay.org/), vendored as a
static UMD build under `frontend/public/vendor/opensheetmusicdisplay/` and
loaded by a plain `<script>` tag (`frontend/` has no `package.json` or
bundler). A caption below the score ("Barlines are approximate, not authored
meter.") shows when `GET .../musicxml/meta` reports `meter_defaulted: true`,
that is, when the source file gave no meter.

Selecting a row colors its notes' noteheads (OSMD's `Note.NoteheadColor`). A
row's `note_ids` are mapped to `<note xml:id>` values through the piece's
`note_xml_ids`. OSMD does not keep `xml:id`, so the score tab finds each
note by correlating the order of the MusicXML notes with OSMD's own note
order, which holds for a single voice only. Multi-voice scores need matching
by position and pitch.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`): the only code allowed to
  call into Ursatz's persistence and query functions or the Analyzer
  library. Every tool goes through this layer; nothing calls Ursatz directly
  from route code. This boundary is the one thing the scaffold is strict
  about, because it is expensive to untangle later.
- **Tool modules** (`backend/src/routes/`): independent GUI features
  (Analysis and Corpus now, Generator later). Each tool registers its own
  HTTP routes under an `/api/<tool_name>/...` prefix and never calls another
  tool directly. See `backend/include/ursatz_gui/tool.h` for the
  registration convention and `backend/src/routes/health.c` as the reference
  tool.
- **Host/shell** (`backend/src/server.c`, `frontend/public/`): starts the
  HTTP server, wires tool routes together, and serves the static frontend
  shell (top bar, tool sidebar, content area) that tool UIs render into.

New tool code goes in `backend/src/routes/`. New shared persistence or query
logic goes in `backend/src/service/`. If you find yourself calling an Ursatz
function from inside `routes/`, it belongs in `service/` instead.

The Analyzer library (`external/ursatz-analyzer`, linked as
`ursatz_analyzer`) is the only analysis path: `analysis_service.c` calls
`input_pipeline_import_file`, `run_analysis_pipeline` and
`analysis_document_build`, and the GUI keeps no analysis code of its own.
Analysis therefore changes only when the Analyzer submodule is bumped.

## Backend

C HTTP server using [civetweb](https://github.com/civetweb/civetweb) (MIT),
fetched by CMake `FetchContent`. Civetweb is small, permissively licensed
and has a simple C API, with no GPL or commercial dual-licensing.

### Piece store

Each piece is a directory `pieces/<id>/`. Ids are `piece-<16 lowercase hex>`
from `ursatz_gui_new_id` in `service/id_service.c` (8 bytes of
`/dev/urandom`, with a hash fallback), redrawn if the directory exists, at
most 8 tries.

- `source.mid` or `source.musicxml`: the upload's bytes, written unchanged. A
  `.xml` or `.mxl` upload keeps its extension (`source.xml`, `source.mxl`).
- `analysis.json`: the Analyzer's analysis document (schema v9).
- `meta.json`: `{"filename", "format": "midi"|"musicxml", "uploaded_at"
  (ISO 8601 UTC), "analyzer_version", "document_schema_version"}`.

Every file is written to `<name>.tmp` and renamed, and a failed analysis
removes the directory. Re-running the analysis rewrites `analysis.json` and
the two version fields of `meta.json`; the filename and `uploaded_at` stay.
Pieces saved by earlier versions (`pieces/<id>.db`) are ignored: they are
not listed and cannot be opened, so upload those files again.

### Analyzer routes

- `POST /api/analyzer/pieces`: the file as the raw request body, its name
  URL-encoded in the `X-Filename` header. The format is detected from the
  content (`input_format_detect`), not from the filename. Returns
  `{"ok":true,"id":...,"filename":...}`. A body that is neither MIDI nor
  MusicXML gives `415` with `{"error": "unsupported format"}`; a file the
  Analyzer cannot import or analyze gives `422` with
  `{"error": "import failed"}`; nothing is stored in either case.
- `GET /api/analyzer/pieces`: the stored pieces, newest upload first, then by
  id: `{"ok":true,"pieces":[{"id","filename","format","uploaded_at"}]}`.
- `GET /api/analyzer/pieces/<id>`:
  `{"ok":true,"id","filename","format","uploaded_at","note_xml_ids":[...],"document":{...}}`.
  `document` is the analysis document as stored. `note_xml_ids[i]` is the
  `<note xml:id>` the score export writes for note `i` of the source; a
  claim's `note_ids` are indices into this array. An unknown id returns
  `{"ok":false,"message":"piece not found"}`.
- `PATCH /api/analyzer/pieces/<id>`: body `{"filename":"<name>"}`. Rewrites the
  filename in `meta.json`. Returns `{"ok":true,"id":...,"filename":...}`,
  `404` for an unknown id, or `{"ok":false,...}` for an empty filename.
- `POST /api/analyzer/pieces/<id>/reanalyze`: runs the Analyzer again on the
  stored source. Returns `{"ok":true,"id":...}`, `404` for an unknown id, or
  `422` with `{"error": "import failed"}` when the source no longer imports.
  The piece is unchanged unless the new analysis succeeded.
- `DELETE /api/analyzer/pieces/<id>`: removes the piece's directory.
- `GET /api/analyzer/pieces/<id>/musicxml`: the piece's score as MusicXML
  (`application/xml`), written by Ursatz's `musicxml_export_write_score` from
  the stored source: its parts and staves, its real meter and its key
  signatures. Tempo markings and dynamics are left out. Unknown id returns
  `404`. If the export fails the response is
  `{"ok":false,"message":"failed to export piece","code":"<MusicXmlExportResult name>"}`.
- `GET /api/analyzer/pieces/<id>/musicxml/meta`:
  `{"ok":true,"meter_defaulted":<bool>}`, true when the source gave no meter
  and the barlines come from a default 4/4 (the export then carries
  `ursatz:meter-defaulted`). Same errors as the export route.

### Corpus routes

The Corpus tool wraps Ursatz's `corpus_api.h`:

- `POST /api/corpus`: body `{"id":"<corpus-id>","name":"<name>"}`. Creates a
  Corpus. Returns `{"ok":true}` or `{"ok":false,"message":...}`.
- `GET /api/corpus`: lists all Corpora:
  `{"ok":true,"corpora":[{"id":...,"name":...},...]}`.
- `POST /api/corpus/<id>/pieces`: body `{"piece_id":"<EntityID>"}`. Adds a
  piece ref to the named Corpus. Adding a ref already present fails, same as
  `corpus_api` itself.
- `DELETE /api/corpus/<id>/pieces/<piece_id>`: removes a piece ref. Removing
  a ref that is not a member fails the same way.
- `GET /api/corpus/<id>/pieces`: lists piece refs in a Corpus:
  `{"ok":true,"pieces":[{"id":...},...]}`.

All Corpus routes return HTTP 200 with `{"ok":false,...}` on failure.
`<id>` and `<piece_id>` path segments are validated against the same
alnum, `-`, `_` whitelist the Analyzer uses for piece ids (its own copy,
kept in `corpus_service.c`).

Corpus uses one shared store, `data/corpus.db`, opened once when the server
starts and kept open for the life of the process. It does not check that a
piece ref exists in the piece store when added; `corpus_api` does not either.

`GET /api/health/status` calls the persistence service to confirm
`persistence_open` succeeds against a test database path, and returns JSON.

## Frontend

Plain static HTML, JS and CSS served by the backend from `frontend/public/`.
No framework or build pipeline. Theming lives entirely in `style.css` as CSS
custom properties on `:root`, so a theme picker can swap colors without
touching markup or `app.js`. Frontend logic makes no styling decisions.

The Corpus tool is shown as "Collections" in the sidebar and view title; the
API, module and route names stay `corpus`. It renders a grid of Collection
cards (name only; the API has no piece count or last-updated field). Clicking
a card opens a detail view listing that Collection's piece refs with the same
row component as the Library: each row's filename is found by joining the
piece ref's id against `/api/analyzer/pieces`, falling back to the bare id.
The only action on a Collection's piece row is removing it from the
Collection. New piece refs are added as plain id strings from the id input
below the list, and a piece may belong to any number of Collections.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires CMake 3.20 or newer, a C11 compiler, SQLite development headers and
pkg-config. Ursatz is a git submodule at `external/ursatz`, pinned at PR #142
(`ffd8a19`), the commit the Analyzer and Composer pin. The Analyzer is a
second submodule, `external/ursatz-analyzer`, pinned at PR #36 (`11263b7`).
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF).

## Tests

`ctest` runs the backend tests (health, analyzer, corpus and id routes) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node CMake prints one status line
and skips them. The frontend tests are plain Node scripts that can also be
run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against
`frontend/tests/fixtures/mozart545.analysis.json`, the Analyzer CLI's
document for Mozart 545 cut to its first 6 items per array.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, compares the
stored analysis with the Analyzer CLI's document for the same file, and
covers list, detail, rename, re-run and delete.

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.

## Out of scope

Native webview packaging, Generator tool code, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists).
