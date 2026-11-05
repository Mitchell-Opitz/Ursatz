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

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer has no blend, corpus seeding or explain.
