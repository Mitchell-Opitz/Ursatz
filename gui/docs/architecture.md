# GUI architecture

How the GUI is built, what it calls, and where a request goes. [Docs index](README.md).

## Purpose

A local web UI over the Ursatz platform: it imports MIDI and MusicXML files, shows their analysis next to the score, plays them, and composes motifs, themes and pieces. It is a client: every music answer comes from the Analyzer or the Composer; the GUI stores files and draws pages.

## Where it sits

- Below it, three submodules in `external/`, pinned by commit (versions in [Building](../README.md#building)): [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) (storage only), the [Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/architecture.md) and the [Composer](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/architecture.md).
- Above it, the browser only.

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `backend/src/routes/` | Route families: parse, call a service, answer | [api/](api/analysis-routes.md) |
| `backend/src/service/` | The only code that calls the Analyzer, Composer and stores | [storage.md](storage.md), [motifs-backend.md](motifs-backend.md) |
| `backend/src/util/`, `backend/include/` | File, JSON and lock helpers; the tool header | [Helpers](#backend-helper-modules) |
| `backend/tests/` | Tests and fixtures | [testing.md](testing.md) |
| `frontend/public/` | The static pages, models and views | [frontend.md](frontend.md), [views/](views/library.md) |
| `scripts/` | Pin, include, theory and docs checks | [testing.md](testing.md) |

## Public API

Handlers are in `backend/src/routes/`. Paths under `/api/materials/` are shortened to `...`.

| Routes | Handler | Doc |
|---|---|---|
| `/api/analyzer/pieces` (`GET POST`), `.../<id>` (`GET PATCH DELETE`), `.../<id>/document`, `/musicxml`, `/musicxml/meta`, `/playback`, `/reanalyze`, `/api/analyzer/groups` | `analyzer.c` | [Analyzer routes](api/analysis-routes.md#analyzer-routes) |
| `/api/corpus`, `/api/corpus/<id>`, `.../<id>/pieces`, `.../pieces/<piece_id>` | `corpus.c` | [Corpus routes](api/analysis-routes.md#corpus-routes) |
| `/api/styles`, `/api/styles/<id>`, `.../<id>/rebuild` | `styles.c` | [Styles routes](api/analysis-routes.md#styles-routes) |
| `/api/links/runs`, `/api/links/runs/<id>`, `/api/links/pieces/<id>` | `links.c` | [Motive links routes](api/analysis-routes.md#motive-links-routes) |
| `GET /api/health/status` | `health.c` | [Analyzer routes](api/analysis-routes.md#analyzer-routes) |
| `.../projects`, `.../projects/<id>`, `.../items`, `.../items/<id>`, `.../items/validate` | `materials.c` | [Materials and subjects routes](api/materials-routes.md#materials-and-subjects-routes) |
| `.../items/<id>/versions`, `.../versions/<n>`, `/files/<name>`, `/score`, `/playback`, `/compose`, `/regenerate`, `/variations`, `/variations/<k>/choose` | `materials.c` | same |
| `.../items/<id>/reanalyze`, `/variants`, `/variants/save`, `/variant-tree`, `/reroll`, `/each-op`, `/op-checks`, `/link-form`, `/promote`, `/flag`, `/uses/<source>/update` | `materials.c` | same |
| `.../variant-operations`, `.../recognize`, `.../render`, `.../motifs/random`, `.../motifs/to-c`, `.../motifs/import` | `materials.c` | same |
| `/api/subjects`, `/api/subjects/groups`, `/api/subjects/<id>`, `.../<id>/items/<item>`, `/pieces/<piece>`, `/related/<other>` | `materials.c` | same |
| `GET /api/composer/status`, `GET /api/composer/vocabulary` | `composer.c` | [Composer routes](api/composer-routes.md#composer-routes) |
| `/api/jobs/<id>` (`GET DELETE`) | `jobs.c` | [Jobs](api/composer-routes.md#jobs) |

## Version constants and schemas

| Name | Where reported | Current value | Doc |
|---|---|---|---|
| `version`, `api_version`, `analyzer_version`, `ursatz_commit` | `GET /api/composer/status`, from `composer_api_versions` | the pinned Composer (0.4.1, API 1.1) and the builds it links | [Composer routes](api/composer-routes.md#composer-routes) |
| `analyzer_version`, `ursatz_commit`, `document_schema_version` | each piece's `meta.json`, from `ursatz_analyzer_version`, `ursatz_analyzer_ursatz_commit` and `URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION` | the pinned Analyzer (0.6.0) | [Piece store](storage.md#piece-store) |
| `api_version`, `composer_version`, `ursatz_commit`, `render_key`, `tempo` | `render.json` beside a cached motif render | the pinned Composer | [Motif score and playback](views/playback.md#motif-score-and-playback) |
| `schema_version` of a Composer request | `theme_service.c`, `piece_service.c` | 2 for a theme, 1 for a piece | [Storage](storage.md#theme-bodies-and-version-files) |

`GET /api/health/status` reports only whether the store opens (`ok`, `message`).

## What it calls below

| Service | Entry used | From | For |
|---|---|---|---|
| `analysis_service.c` | `input_pipeline_import_file`, `run_analysis_pipeline`, `analysis_document_build`, `analyzer_document_status`, `analyzer_score_export_musicxml`, `analyzer_imported_meter_defaulted`, `analyzer_playback_build` | Analyzer | import, analysis, score and playback of a piece |
| `playback_service.c` | `input_pipeline_import_file`, `analyzer_playback_build` | Analyzer | playback of a stored file |
| `style_service.c`, `link_service.c` | `style_profile_*`, `style_model_*`, `motive_link_scope_*`, `motive_links_run` | Analyzer | style models and motive links |
| `motif_service.c`, `theme_service.c`, `piece_service.c`, `compose_common.c` | `composer_api_*`, listed in [Backend services](#backend-services-and-who-calls-the-composer) | Composer | motifs, themes, pieces, export, vocabulary |
| `persistence_service.c`, `collection_service.c` | `persistence_open`, `corpus_api_open`, `corpus_api_list_corpora`, `corpus_api_list_piece_refs` | Ursatz | the piece database and the corpus index |

## Data flow

One piece, from file to composed motif, across four repos.

1. **GUI.** The browser sends the file to `POST /api/analyzer/pieces` (`routes/analyzer.c`).
2. **Analyzer** ([architecture](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/architecture.md)). `analysis_service.c` calls `input_pipeline_import_file`.
3. **Ursatz** ([architecture](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md)). The Analyzer's pipeline runs the Ursatz frameworks (`run_analysis_pipeline`) and `analysis_document_build` makes the analysis document.
4. **GUI.** The service writes `analysis.json` and `meta.json` under the piece lock ([Piece store](storage.md#piece-store)), and the table fills in.
5. **GUI and Analyzer.** Opening a piece asks for the summary, the sections the score draws, the MusicXML (`analyzer_score_export_musicxml`) and the playback (`analyzer_playback_build`); the frontend draws and plays them ([views](views/analysis-and-score.md)).
6. **GUI and Composer** ([architecture](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/architecture.md)). A service builds a Composer request and calls a `composer_api_*` entry in process; long runs use the job queue ([Jobs](api/composer-routes.md#jobs)).
7. **GUI.** The result is stored as a version in the material store ([Material store](storage.md#material-store)) and shown in its view.

### GUI actions

| Action | Route | GUI service | Answered by |
|---|---|---|---|
| Import or analyze a piece | `POST /api/analyzer/pieces`, `.../<id>/reanalyze` | `analysis_service.c` | Analyzer: `input_pipeline_import_file`, `run_analysis_pipeline` |
| Open the Score | `GET .../musicxml`, `.../document?sections=` | `analysis_service.c` | Analyzer: `analyzer_score_export_musicxml`, `analysis_document_to_json` |
| Play | `GET .../pieces/<id>/playback` | `playback_service.c` | Analyzer: `analyzer_playback_build` |
| Create, analyze and save a motif | `POST /api/materials/items`, `.../<id>/versions`, `.../reanalyze` | `motif_service.c` | Composer: `composer_api_motive` |
| Random motif | `POST /api/materials/motifs/random` | `motif_service.c` | Composer: `composer_api_random_motive` |
| Variants | `POST .../items/<id>/variants`, `.../variants/save` | `motif_service.c` | Composer: `composer_api_variants`, `composer_api_variant_operations` |
| Compose a theme | `POST .../items/<thm-id>/compose` | `theme_service.c`, `compose_jobs.c` | Composer: `composer_api_theme` |
| Compose a piece | `POST .../items/<pce-id>/compose` | `piece_service.c`, `compose_jobs.c` | Composer: `composer_api_piece`, `composer_api_piece_variations` |
| Export | `GET .../versions/<n>/files/<name>`, `/score` | `compose_common.c` | Composer: `composer_api_export` writes the files at compose |

## Finding things

Every doc is in the [docs index](README.md).

## Layers

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

## Backend helper modules

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
- `service/piece_lock.c`: `ursatz_gui_piece_lock(id)` and `_unlock`, one mutex per piece directory (see [Piece store](storage.md#piece-store), [Concurrency](storage.md#concurrency)).
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

## Backend services and who calls the Composer

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
