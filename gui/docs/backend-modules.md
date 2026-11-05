# Backend modules

The backend helper modules and the services that call the Composer. Back to the [docs index](README.md).

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
`ursatz_gui_material_migrate_segments`, `ursatz_gui_material_clear_themes`) are in `service/material_service.c`, which calls `motif_service.c` to analyze a
motif again.

Piece documents are read only through `service/analysis_service.c`, under the piece lock: `ursatz_gui_analysis_get_document`
(with no names, the whole stored document), `_get_filename` and `_piece_exists`. The link, style and collection services
use these, and no other file names a piece's `meta.json` or `analysis.json`.

On the page, `frontend/public/request.js` (`ApiRequest`) holds `jsonRequest`, `errorText`, `fieldForPath` and
`validateRequest`; the models and views call it and keep no copy.
