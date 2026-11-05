# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`): the only code that
  calls the Analyzer and Composer libraries. Every tool goes through this
  layer; nothing calls them from route code. The GUI is a client of the
  apps (R-218, R-219): it displays and takes input, and every music answer
  comes from the Analyzer or the Composer. The one library edge left is
  storage: `persistence_service.c` includes `persistence/persistence.h` and
  `collection_service.c` includes `corpus/corpus_api.h`, both from Ursatz.
  No other backend file includes an Ursatz header.
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
`analysis_document_build`; the piece detail's note ids, the score
(`analyzer_score_export_musicxml`, `analyzer_imported_meter_defaulted`) and the
playback (`analyzer_playback_build`) also come from the Analyzer. The file
includes no Ursatz header, and the GUI keeps no analysis code of its own.
Analysis therefore changes only when the Analyzer submodule is bumped.

### Backend helper modules

Everything but `main.c` is built as the static library `ursatz_gui_core` (`backend/CMakeLists.txt`); the server
(`ursatz-gui-backend`) is `main.c` linked to it, and each test is built with `ursatz_gui_add_test(name source workdir)`
(`backend/tests/CMakeLists.txt`), which links the same library. A new source file is added to the library once.

- `util/fs_util.c`: `ursatz_gui_dup_string`, `ursatz_gui_copy_message`, `ursatz_gui_iso_utc_now`, `ursatz_gui_path_exists`,
  `ursatz_gui_path_is_dir`, `ursatz_gui_make_dir`, `ursatz_gui_ensure_dir`, `ursatz_gui_write_file_atomic` (writes `<path>.tmp`, then renames), `ursatz_gui_read_file`
  and `ursatz_gui_read_json_object`. Every store and the routes use these; none keeps a copy. `link_service.c` keeps
  its own `iso_utc_now`, which adds milliseconds so two runs in one second still order.
- `util/json_util.c`: the cJSON helpers the services share: `ursatz_gui_json_string` (the string member, `""` when
  absent) and `_string_copy`, `_set_string`, `_attach`, `_add_copy_or_null` and `_add_copy_if`.
- `util/store_lock.h`: `UrsatzGuiLock` (a pthread mutex, an SRWLOCK on Windows) with `URSATZ_GUI_LOCK_INIT` and
  `ursatz_gui_lock_acquire/release`. Each store wraps it in `LOCK()` and `UNLOCK()`.
- `service/piece_lock.c`: `ursatz_gui_piece_lock(id)` and `_unlock`, one mutex per piece directory (see Piece store,
  Concurrency).
- `routes/route_util.c`: the senders every route uses (`ursatz_gui_send_json`, `_send_json_status`, `_send_failure`
  with `{"ok":false,"message"}`, `_send_object`, and `ursatz_gui_reason_phrase`) and `ursatz_gui_read_json_body`, the
  JSON object body of a request (up to 1 MiB; with a status pointer it also sends the 400, 413 or 500 itself). `routes/materials.c` passes
  its own 256 KiB limit to `ursatz_gui_read_json_body_max`.
- `service/compose_common.c`: `ursatz_gui_compose_item`, which the theme and piece services share, the steps around a Composer run
  (update the uses, make the version folder, build the brief and the request, run, add the version, rename the folder
  when another writer added a version). Each service passes an `UrsatzGuiComposeKind` with its own checks, request,
  run, note and result body; there is no compose lock: each compose makes its files in a staging folder
  (`ursatz_gui_compose_stage_make`) and moves it to its version's folder when the version is written.
- `service/job_queue.c` and `service/compose_jobs.c`: the job queue and the compose runs as jobs (`routes/jobs.c` are
  its two routes). `frontend/public/backend-jobs.js` is the client: `runJob` sends a job request, polls every 500 ms
  and resolves with the status and body the route answered before jobs; `sendMaterialsRequest` runs every request
  marked `job: true` through it and shows the stage and a Cancel button while it runs.

### Backend services and who calls the Composer

Each Composer area has one service that makes its Composer API calls; a route parses the request, calls the service
and answers.

| Area | Service | Composer entries |
|---|---|---|
| Motifs | `service/motif_service.c` | `composer_api_motive`, `_motive_from_midi`, `_motive_render`, `_random_motive`, `_recognize`, `_validate`, `_variants`, `_variant_operations`; it also builds the motive request of a motif and analyzes a motif body on save |
| Themes | `service/theme_service.c` | `composer_api_theme` |
| Pieces | `service/piece_service.c` | `composer_api_piece`, `_piece_variations` |
| Export | `service/compose_common.c` | `composer_api_export` |
| Build and vocabulary | `service/composer_client.c` | `composer_api_versions`, `_vocabulary`; the reply type, `ursatz_gui_composer_reply_refused` and `_failure_status` |

`routes/composer.c` answers the status route with `composer_api_versions` itself and the vocabulary route from
`composer_client.c`; `routes/materials.c`, `routes/links.c` and the other routes call no `composer_api_*` entry. The material store's one-time migrations (`ursatz_gui_material_migrate_tones`,
`ursatz_gui_material_clear_themes`) are in `service/material_service.c`, which calls `motif_service.c` to analyze a
motif again.

Piece documents are read only through `service/analysis_service.c`, under the piece lock: `ursatz_gui_analysis_get_document`
(with no names, the whole stored document), `_get_filename` and `_piece_exists`. The link, style and collection services
use these, and no other file names a piece's `meta.json` or `analysis.json`.

On the page, `frontend/public/request.js` (`ApiRequest`) holds `jsonRequest`, `errorText`, `fieldForPath` and
`validateRequest`; the models and views call it and keep no copy.

## Frontend

Plain static HTML, JS and CSS served by the backend from `frontend/public/`.
No framework or build pipeline. Theming lives entirely in `style.css` as CSS
custom properties on `:root`, so a theme picker can swap colors without
touching markup or `app.js`. Frontend logic makes no styling decisions.

The Corpus tool is shown as "Collections" in the sidebar and view title; the
API, module and route names stay `corpus`.

The Library, a collection's detail and the add-pieces picker share one table
(`buildPieceTable` in `app.js`): a select column, Title, Composer, Source,
Tags and Actions.

- **Cards**: name, a one-line description, "<n> pieces" and the top 3
  composers and top 3 tags among the collection's pieces. The `⋯` button
  opens Rename, Edit description and Delete (the confirm names the
  collection; its pieces are not deleted). The header form takes a name and
  an optional description.
- **Detail**: the title and description each have an inline editor. The
  table lists the members with search, Composer, Source and Tag filters
  applied in the browser. Row actions are Open (analysis) and Remove from
  collection; checked rows can be removed together.
- **Add pieces**: opens a picker with the Library filters (applied by the
  backend) over pieces not yet in the collection. `Add selected (n)` and
  `Add all matching (n)` each send one `POST /api/corpus/<id>/pieces` with
  `piece_ids`; the result reads "Added 5, 2 were already in it".
- **Save as collection**: shown in the Library while a filter is on. It
  creates a collection named from the filter ("John Field", "Tag: whimsical",
  parts joined with " · ") and adds every shown piece in one request, then
  opens it. The collection is a snapshot and does not follow later tag
  changes; the sidebar Tags panel is the live grouping.
- **Build style**: builds a style (a StyleModel learned from pieces, see
  Styles routes) from a set of pieces. The button appears in three places:
  the Library while a filter is on (next to Save as collection), a
  collection's detail, and on each Tags panel row (a small ♫ button shown on
  hover and focus, labelled "Build style from <row>"). It opens an inline name
  prompt prefilled with the default name ("Composer: X", "Source: X",
  "Tag: x" joined with " · ", "Collection: <name>" or "Selected pieces");
  Build sends `POST /api/styles` and the button reads "Building..." until the
  answer. A plain composer, source or tag filter builds from that filter;
  Unassigned, Untagged and search have no backend form, so those build from
  the ids of the pieces shown. A failure shows under the button.
- **Styles**: a section under the collection cards listing each style with
  its name, source ("Tag: whimsical"), piece count and updated date, and the
  actions Open, Rebuild (replaces the style in place from today's pieces) and
  Delete (confirm). A finished build opens the style detail with a result line
  ("Built from 12 pieces, 2 skipped").
- **Style detail**: the source and counts, the pieces used (each a link to its
  analysis), the skipped pieces with the reason, and what the style learned:
  for each dimension and category the most common labels as a bar of
  `count/total` with the fraction as text, then "and <n> more".
  The four motive categories read "Motive degrees", "Motive intervals", "Motive rhythms" and "Non-chord tones in
  motives"; the rhythm categories `tempo` and `note-density` read "Tempo (quarter notes per minute)" and "Note
  density (attacks per beat)". A style built from a brief reads "Brief" as its source (briefs are made in the Themes
  view; the Collections view builds none).

### Score layers

`frontend/public/score-layers.js` (global `ScoreLayers`, no DOM) is the data
model of the claim layers drawn on the score. Each layer turns sections of
the analysis document into items `{key, layerId, text, label, start, end, at,
noteIds, style, claim, parent}`, with `start` and `end` in whole notes from
the piece start. A layer has one of three kinds: `ribbon` (a bar over the
claim's span in a lane above or below the staff), `mark` (a short label at
one point in a lane) or `notes` (the claim's noteheads colored, no lane).

| Layer | Group | Kind | Color | Items from |
|---|---|---|---|---|
| Keys | Harmony | ribbon above | `#1f6fd1` | `keys.regions` |
| Key changes | Harmony | ribbon above | `#7b4fd8` | `tonicizations` (dashed), `modulations` (solid) |
| Chords | Harmony | mark below | `#0b7a70` | `chords`, the Roman numeral at the span start |
| Cadences | Harmony | mark above | `#d0342c` | `cadences`, abbreviated (PAC, IAC, HC, ...) at the arrival |
| Non-chord tones | Harmony | notes | `#8a6a00` | `non_chord_tones` |
| Prolongations | Harmony | ribbon above | `#3b8a2f` | `prolongations` |
| Progressions | Harmony | ribbon above | `#b5530a` | every `progressions[].occurrences[]` |
| Form sections | Structure | ribbon above | `#3f4b5e` | `form.sections` |
| Themes | Structure | ribbon above | `#c2185b` | `themes` |
| Phrases | Structure | ribbon above | `#006f99` | `phrases` and their `subphrases` (thin) |
| Motives | Structure | notes | `#a2369a` | every `motives[].occurrences[]` |
| Schemas | Structure | ribbon above | `#7a5a3a` | `schemas` |
| Texture | Texture and melody | ribbon above | `#5f6b00` | `texture` |
| Melody | Texture and melody | notes | `#2c3fa8` | `melody` |

Any other top-level section holding claims (objects with `span` or
`location`), directly or in arrays inside an object, becomes an Other layer
(`other:<section>`, gray `#6b7280` ribbon above), so a new Analyzer section
shows up without a GUI change. `schema_version`, `metadata`, `meter`,
`notes`, `chord_vocabulary`, `harmonic_rhythm`, `outer_voices`, `markings`,
`voice_leading` and `rhythm` are never layers. Overlapping ribbons are packed
into at most four rows; the rest are flagged as overflow. When two notes
layers share a note, the one higher in the table colors it.

Which layers are on is remembered in `localStorage` under
`ursatz.scoreLayers.v1` as a JSON array of layer ids. Nothing is on by
default; the presets are Harmony (keys, key changes, chords, cadences) and
Structure (form, themes, phrases, cadences). Ids a piece lacks are kept for
the next piece.

### Score geometry

`frontend/public/score-geometry.js` (global `ScoreGeometry`) turns positions
into places on the rendered score. `extractLayout(osmd)` reads OSMD once into
a plain layout, in CSS pixels from the OSMD SVG's top-left:
`{unit, systems: [{index, top, bottom, left, right, measures: [{number, start,
end, x, width, entries: [{t, x}]}]}]}`. Every other function takes that
object, so tests use hand-built layouts. Times in a layout are OSMD absolute
timestamps in whole notes.

- `xAt(layout, t)` gives `{system, x}`; inside a measure x is interpolated
  between the staff entries. `tAt(layout, system, x)` is its inverse.
- `spanSegments(layout, t1, t2)` gives one segment per system a span
  touches; a segment that continues on the next system is open at that end.
- `calibrateOffset(pairs)` returns the timeline offset, the median of
  `osmd - ursatz` over the first 50 matched notes, plus the number of pairs
  that disagree. OSMD time is the Analyzer position plus the offset, which
  absorbs pickup measure numbering.
- `laneStack(lanes)` stacks lanes from the staff outward, 18 px per row, 4 px
  between lanes and 8 px before the first. Above the staff, nearest first:
  cadences, Other layers, progressions, prolongations, key changes, keys,
  texture, schemas, phrases, themes, form sections. Chords go below.
  `systemSpacingUnits` converts the stack heights into OSMD
  `MinimumDistanceBetweenSystems` and `PageTopMargin`, never below the
  defaults.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires CMake 3.20 or newer, a C11 compiler, SQLite development headers and
pkg-config. Ursatz is a git submodule at `external/ursatz`, pinned at PR #195
(`d2b85ce`). The Analyzer (version 0.6.0) is a second submodule,
`external/ursatz-analyzer`, pinned at PR #67 (`ee8a901`). cJSON 1.7.18 is
vendored at `external/cjson` and added first, so the Ursatz Analyzer reuses it.
`scripts/check-pins.sh` fails when a pin is not on its remote's `main` or when
one repository is pinned at two commits; CI runs it after the submodule
checkout.

Two more checks keep the GUI a client and CI runs both (the build is
configured with `URSATZ_LEGACY_INCLUDE_DIRS` OFF, so only staged public headers
are visible):

- `scripts/check-library-includes.sh build` fails when `backend/src` or
  `backend/include` includes an Ursatz header other than the two storage edge
  headers above, an Analyzer header outside `ursatz_analyzer/`, a Composer header
  other than `ursatz_composer/composer_api.h`, or any `*_internal.h`.
- `scripts/check-frontend-theory.sh` fails when a file under `frontend/public`
  (not `vendor/`) defines `playbackFromMidi`, `randomSettings`, `defaultCadence`,
  `parseHarmonyToken`, `HARMONY_TOKEN`, `KEY_TONICS`, `RANDOM_METERS`,
  `chordProblems` or `numeralsOk`.

Done-when for the client boundary (P27), checked by hand: analyze a reference
piece (same claims and score as before the phase); make the "Knight's house"
theme with the same seeded plan; during a compose `ps` shows no
`ursatz-composer` process; opening a piece imports it once (the import counter);
all checks above pass.
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF). The Composer is a third submodule,
`external/ursatz-composer`, pinned at PR #95 (`10ea20b`, version 0.4.1, API 1.1). The backend links the Composer
library from the submodule (see Composer routes), without the command, and it reuses the cJSON, Ursatz and Analyzer
targets above. If the submodules are missing the build stops: run `git submodule update --init
--recursive` and build again.

## Tests

`ctest` runs the backend tests (health, analyzer, corpus, styles, composer, materials, theme, subjects, links and id routes, the piece metadata rules, the material store and the link service) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node CMake prints one status line
and skips them. The frontend tests are plain Node scripts that can also be
run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against
`frontend/tests/fixtures/mozart545.analysis.json`, the Analyzer CLI's
document for Mozart 545 cut to its first 6 items per array, and that each tab loaded per section equals the tab built
from the whole document.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, compares the
stored analysis with the Analyzer CLI's document for the same file, and
covers list, detail, metadata editing, filtering and groups, rename,
re-run and delete; the summary and the `/document` route; and, through a counter in the import cache, that the
detail, score, meta and playback of one piece import it once and a PATCH or re-analyze makes the next open import
again. `test_styles_route` uploads Clementi 36 and Mozart 545,
tags them and builds styles from a tag, a collection and ids, then reads the
model back with the Analyzer, rebuilds, re-analyzes an old piece and deletes.
`test_link_service` resolves each scope kind (a project through its subjects), runs the four link fixtures
(`backend/tests/fixtures/links`, copied from the Analyzer's `tests/links`), checks that a second run over the same
scope compares nothing, analyzes a document without features again and reads an interrupted run as failed.
`test_links_route` starts runs over HTTP, polls them to done, and covers the `409`, `400`, `404` and `422` answers,
the list, delete and a piece's links.
`test_piece_lock` covers the piece lock without a server: threads counting under one id, 50 PATCHes and 50
re-analyses of one piece while a reader checks that each detail, score and playback reply parses and that the
document's meter matches the notation override returned with it, and deletes during reads.
`test_piece_metadata` covers the normalizing, grouping and
filter rules without a server. `test_piece_snapshot` compares a piece's detail, score and playback replies with the snapshots in `backend/tests/fixtures/pieces`. `test_stale_analysis` covers the stale-analysis rules (Staleness keys, above). `test_material_service` covers the projects,
items, versions, version body rules and item references without a server. `style-view.test.js` covers the style detail
rows (order, fractions, "and n more") and `piece-metadata.test.js` the style
names and request bodies. `test_job_queue` covers the queue without a server: a job's answer and kind, progress seen
while it runs, the cancel of a running theme compose (it ends `cancelled`) and of a queued job, two workers running two
jobs at once, and stop joining the workers. `backend-jobs.test.js` covers the job client with a fake fetch. `test_materials_route` replays 35 requests captured while the Composer still ran as a command (`backend/tests/fixtures/composer/b5_replies.json`: motif analysis, degrees to notes, MIDI import, render, operations, recognize, random motifs, status) and checks each reply equal (a refusal by status), and the refusal paths (`/meter`, `/input/text`); `test_composer_route` checks the versions the status reports. `test_theme_route` checks the theme body rules, the request a compose
writes (slots, a pool of approved and unflagged forms without the rejected, the brief, `without`, read back from the
version's `request.json`), the version it writes with its files, the files route, a stuck run writing nothing and
leaving no staging folder, `update_uses`, two composes of one theme at once, the jobs routes, and one compose over a
brief of fixture pieces whose plan, MIDI and MusicXML equal what the Composer command wrote in B6
(`backend/tests/fixtures/composer/b6_compose`). `test_piece_route` does the same for a piece: its compose,
regenerate, variations and choose are compared with the same capture (a chosen variation's ending is made from a seed
read from the clock, so only its loop version is compared).

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer has no blend, corpus seeding or explain.
