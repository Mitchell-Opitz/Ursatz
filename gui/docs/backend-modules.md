# Backend modules

The backend helper modules and the services that call the Composer. Back to the [docs index](README.md).

## Backend helper modules

Everything but `main.c` is built as the static library `ursatz_gui_core` (`backend/CMakeLists.txt`); the server
(`ursatz-gui-backend`) is `main.c` linked to it, and each test is built with `ursatz_gui_add_test(name source workdir)`
(`backend/tests/CMakeLists.txt`), which links the same library. A new source file is added to the library once.

- Files, folders, text, JSON and hashes come from `common/` (`<common/files.h>`, `strings.h`, `json.h`, `hash.h`; see the common part's headers): the backend keeps no copy of them. `links.c` keeps its own `iso_utc_now`, which adds milliseconds so two runs in one second still order.
- `workshop/src/items/lock.h`: `WorkshopLock` (a pthread mutex) with `WORKSHOP_LOCK_INIT` and
  `workshop_lock_acquire/release`. Each store wraps it in `LOCK()` and `UNLOCK()`.
- `workshop/src/items/piece_lock.c`: `workshop_piece_lock(id)` and `_unlock`, one mutex per piece directory (see [Piece store](../../workshop/docs/data-folder.md#the-library), [Concurrency](../../workshop/docs/data-folder.md#changes-are-transactions)).
- `workshop/src/items/data_check.c`: `workshop_data_check`, the startup check `server.c` runs first (see [Data check](../../workshop/docs/data-folder.md#older-folders-and-migration)).
- `routes/route_util.c`: the senders every route uses (`ursatz_gui_send_json`, `_send_json_status`, `_send_failure`
  with `{"ok":false,"message"}`, `_send_object`, and `ursatz_gui_reason_phrase`) and `ursatz_gui_read_json_body`, the
  JSON object body of a request (up to 1 MiB; with a status pointer it also sends the 400, 413 or 500 itself). `routes/materials.c` passes
  its own 256 KiB limit to `ursatz_gui_read_json_body_max`.
- `workshop/src/composition/compose_common.c`: `workshop_compose_item`, which the theme and piece services share, the steps around a Composer run
  (update the uses, make the version folder, build the brief and the request, run, add the version, rename the folder
  when another writer added a version). Each service passes an `WorkshopComposeKind` with its own checks, request,
  run, note and result body; there is no compose lock: each compose makes its files in a staging folder
  (`workshop_compose_stage_make`) and moves it to its version's folder when the version is written.
- `workshop/src/jobs/job_queue.c` and `workshop/src/composition/compose_jobs.c`: the job queue and the compose runs as jobs (`routes/jobs.c` are
  its two routes). `frontend/public/backend-jobs.js` is the client: `runJob` sends a job request, polls every 500 ms
  and resolves with the status and body the route answered before jobs; `sendMaterialsRequest` runs every request
  marked `job: true` through it and shows the stage and a Cancel button while it runs.

## Backend services and who calls the Composer

Each Composer area has one service that makes its Composer API calls; a route parses the request, calls the service
and answers.

| Area | Service | Composer entries |
|---|---|---|
| Motifs | `workshop/src/composition/motifs.c` | `composer_api_motive`, `_motive_from_midi`, `_motive_render`, `_random_motive`, `_recognize`, `_validate`, `_variants`, `_variant_operations`; it also builds the motive request of a motif and analyzes a motif body on save |
| Themes | `workshop/src/composition/themes.c` | `composer_api_theme` |
| Pieces | `workshop/src/composition/pieces.c` | `composer_api_piece`, `_piece_variations`; a piece of any form: one plan, MIDI and MusicXML file for each version of its form (R-430) |
| Export | `workshop/src/composition/compose_common.c` | `composer_api_export` |
| Material export | `workshop/src/rendering/export.c` | `composer_api_motive_render` (through `motifs.c`); it gathers the MusicXML and the store values and the Analyzer's `analyzer_text_export_material` writes the text (`GET /api/materials/items/<id>/export`) |
| Build, vocabulary and forms | `workshop/src/composition/composer_client.c` | `composer_api_versions`, `_vocabulary`, `_forms`; the reply type, `workshop_composer_reply_refused` and `_failure_status` |

`routes/composer.c` answers the status route with `composer_api_versions` itself and the vocabulary route from
`composer_client.c`; `routes/materials.c`, `routes/links.c` and the other routes call no `composer_api_*` entry.

Piece documents are read only through `workshop/src/analysis/analysis.c`, under the piece lock: `workshop_analysis_get_document`
(with no names, the whole stored document), `_get_filename` and `_piece_exists`. The link, style and collection services
use these, and no other file names a piece's `meta.json` or `analysis.json`.

On the page, `frontend/public/request.js` (`ApiRequest`) holds `jsonRequest`, `errorText`, `fieldForPath` and
`validateRequest`; the models and views call it and keep no copy.
