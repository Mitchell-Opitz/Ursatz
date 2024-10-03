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
- **Generator**: not built yet. Its backend is the Composer tool (see Composer
  routes below); the view comes after it.

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
bundler). A caption below the score ("This file declares no time signature;
shown in 4/4") shows when `GET .../musicxml/meta` reports
`meter_defaulted: true`, that is, when the source file gave no meter.

Selecting a row colors its notes' noteheads (OSMD's `Note.NoteheadColor`). A
row's `note_ids` are mapped to `<note xml:id>` values through the piece's
`note_xml_ids`. OSMD does not keep `xml:id`, so the score tab finds each
note by its position and pitch (`frontend/public/score-notes.js`, no DOM,
loaded by `index.html` before `app.js` and by the frontend tests):

- The key is `<part index>|<measure number>|<staff>|<voice>|<onset>|<pitch>`.
  Onset is the position in the measure as a reduced fraction of a whole note
  (`1/4`); pitch is step, alter and octave (`E-1/5`). Rests have no key.
- The key is built from the MusicXML text (`<divisions>`, `<duration>`,
  `<backup>`, `<forward>`, `<chord/>`) and from each OSMD `GraphicalNote`
  (`ParentStaff` for part and staff, measure number, voice id, `Timestamp`,
  `Pitch`). Equal keys are the same note, so multi-part, multi-voice scores
  highlight exactly the claim's notes.
- OSMD 3.10.1's voice ids equal the MusicXML `<voice>` numbers, so the voice
  is part of the key and there is no voice-less fallback.
- A claim's note id also matches its barline fragments (`<id>_a`, `<id>_2`).
- Rendered notes whose key matches no `xml:id` are counted; the count is
  logged once per score with `console.warn`.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`): the only code allowed to
  call into Ursatz's persistence and query functions or the Analyzer
  library. Every tool goes through this layer; nothing calls Ursatz directly
  from route code. This boundary is the one thing the scaffold is strict
  about, because it is expensive to untangle later.
- **Tool modules** (`backend/src/routes/`): independent GUI features
  (Analysis, Corpus and Composer). Each tool registers its own
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

### Composer routes

The Composer tool grows themes, plans and fills pieces, and makes variations.
It does not link the Composer library: `composer_service.c` is the only code
that starts the Composer, and it runs the `ursatz-composer` command line with
`posix_spawn` and an argument array (never a shell), capturing stdout and
stderr. The Composer brings its own copies of Ursatz and the Analyzer, so
linking it would put two versions of each into one build; as a command it is
built in its own tree (`external/ursatz-composer`, an `ExternalProject` named
`ursatz_composer_cli`) and its targets never meet the GUI's (R-086). Every
path in the arguments is absolute and the working directory is never changed.
A run is stopped with `SIGKILL` after 120 seconds.

- `URSATZ_GUI_COMPOSER`: the Composer executable to run instead of the one
  built beside the backend (`build/composer/cli/ursatz-composer`).
- `URSATZ_GUI_COMPOSER_TIMEOUT`: the time limit in seconds (default 120).

Every request field is checked against a fixed pattern or range before it
reaches an argument list: keys (`D`, `F#`, `Bb`, `D minor`), meters
(`2/4`, `3/4`, `4/4`, `6/8`), numbers in range, form letters
(`^[A-Z]'*$`), lanes, node ids, tempos (20 to 400, whole or `n/d`) and
note-list text. Anything else is 400 and starts nothing.

Compositions are stored in `compositions/<id>/` (`comp-<16 hex>`):

- `meta.json`: `{"name", "created_at", "kind": "grow"|"plan", "request",
  "versions": [{"n", "action", "args", "created_at", "seed", "summary"}]}`.
  `request` is the body that made the composition; `summary` is the command's
  JSON output (for `plan`, `{"plan": ..., "fill": ...}`, with the `fill`
  command's arguments in `fill_args`). `args` paths are relative to the
  composition.
- `v<n>/plan.json`, `v<n>/piece.mid`, `v<n>/piece.musicxml`: one directory per
  version. Every step is a new version; earlier versions are never changed.
- `motive.txt`, `given-melody.txt`: the note lists the request gave.
- `v<n>/variations-<k>/`: a set of variations (`variation-<i>.json`, `.mid`,
  `.musicxml`, and `result.json`, the command's output).

A command that fails adds no version and leaves nothing behind. A new version
or composition is written with the tempo the composition was made with.

Routes (bodies are JSON; success bodies have `"ok":true`):

- `GET /api/composer/status`: `{"ok":true,"available":true,"version":"..."}`,
  or 503 when the binary is missing.
- `POST /api/composer/grow`: `{name, motive, into, hybrid?, key, meter,
  bars?, cadence?, first_cadence?, accompaniment?, texture?, check?, seed?,
  tempo?}`. `motive` is a note list (`D4:q F4:e A4:e D5:h`); `hybrid` is
  required when `into` is `hybrid`. Makes version 1. Returns
  `{"ok":true,"id","version":1,"summary"}`.
- `POST /api/composer/plan`: `{name, form, key, meter, bars?, count?,
  modulate?, motive?, given_melody? {notes, at}, accompaniment?, texture?,
  seed?, check?, tempo?}`. Runs `plan`, then `fill` with the same seed, into
  version 1.
- `GET /api/composer/compositions`: `{"ok":true,"compositions":[{id, name,
  kind, created_at, versions}]}`, newest first.
- `GET /api/composer/compositions/<id>`: `meta.json` with `ok` and `id`.
- `PATCH /api/composer/compositions/<id>`: `{name}`. `DELETE` removes the
  composition.
- `GET /api/composer/compositions/<id>/versions/<n>/plan | musicxml | midi`:
  the file (`application/json`, `application/xml`, `audio/midi` as an
  attachment).
- `POST .../<id>/lock`: `{version, node | section, lane?}`. Writes the next
  version; only the plan changes, so it copies the piece files of `version`.
- `POST .../<id>/regenerate`: `{version, node | section, lane?, seed?}`.
  Writes the next version with its piece files. A grow plan has no form
  letters; address its nodes by `node` (the `composer.id` in `plan.json`).
- `POST .../<id>/variations`: `{version, node | section, lane?, count (1 to
  16), seed?, harmonize?}`. Writes `v<version>/variations-<k>/` and returns
  `{"ok":true,"id","version","set":<k>,"variations":[{index, seed,
  sections}],"kept","requested","seeds_tried"}`. `harmonize` runs
  `harmonize` and takes a `section` only, with a section whose melody is all
  Given. `k` is counted over the whole composition.
- `POST .../<id>/variations/<k>/<i>/choose`: makes variation `i` of set `k`
  the next version.
- `GET .../<id>/variations/<k>/<i>/plan | musicxml | midi`: the files of a
  variation, for previews.

Errors are `{"error": "..."}`: 400 for a bad field or a Composer usage error
(with `stderr`), 404 for an unknown composition, version or variation, 422
`{"error":"stuck","stderr"}` when the Composer cannot do it (exit status 2),
503 when the binary is missing, 504 on a timeout, 500 otherwise. The Composer
needs POSIX (`posix_spawn`), so this tool does not build on Windows.

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
built (`URSATZ_BUILD_TESTS` is OFF). The Composer is a third submodule,
`external/ursatz-composer`, pinned at PR #34 (`1786b33`). It is built by an
`ExternalProject` in `build/composer` (its tests off), so it brings its own
Ursatz and Analyzer; the build runs it as `ursatz-composer` and does not link
it.

## Tests

`ctest` runs the backend tests (health, analyzer, corpus, composer and id routes) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node CMake prints one status line
and skips them. The frontend tests are plain Node scripts that can also be
run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against
`frontend/tests/fixtures/mozart545.analysis.json`, the Analyzer CLI's
document for Mozart 545 cut to its first 6 items per array.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, compares the
stored analysis with the Analyzer CLI's document for the same file, and
covers list, detail, rename, re-run and delete. `test_composer_route` runs
the real Composer: grow, regenerate, lock, variations, choose and plan make
versions and leave the earlier ones alone; bad requests start nothing and a
fake Composer (`URSATZ_GUI_COMPOSER`) that records its arguments or sleeps
shows the argument lists and the 504.

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.

## Out of scope

Native webview packaging, Generator tool code, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists).
