# Backend modules

The backend's files, its route table and the route helpers. Back to the [docs index](README.md).

## The process

`ursatz-gui-backend [--data <dir>] [--port <n>]` (`backend/src/main.c`):

1. It resolves the settings (`workshop_settings_resolve`: the defaults, `<data>/ursatz.json`, `URSATZ_DATA`, `URSATZ_PORT`, `URSATZ_JOB_WORKERS`, then the flags). An unknown flag prints the usage line and exits 2; a port that is not a whole number from 1 to 65535 is refused.
2. It opens the workshop (`workshop_open`; a second process on one folder exits 1 with "the data folder <path> is in use by another Ursatz process (pid <n>)"), prints the recovery line to stderr when the open recovered something, starts it (`workshop_start`) and starts the HTTP server (`server.c`) over `frontend/public/` with the workshop's `http_threads` request threads (default 16, each with a 32 MB stack for the Composer's deepest entries).
3. On SIGINT or SIGTERM the server ends the event streams (`ursatz_gui_events_stop`, `workshop_events_close`, so the stream threads come back) and stops; `workshop_close` then stops the workshop (it cancels every job and joins the workers) and releases the data folder.

Everything but `main.c` is built as the static library `ursatz_gui_core` (`backend/CMakeLists.txt`); the server `ursatz-gui-backend` is `main.c` linked to it. The route tests that read the workshop's internals link `ursatz_gui_core_internal` (the engine archive); never link a library and its engine into one executable. A new source file is added to the library once. The GUI includes only `workshop_api.h`, `common/`, civetweb and cJSON; the layer table enforces it.

## Routes

`backend/src/routes/` holds one file per family: `library_routes.c`, `collections_routes.c`, `styles_routes.c`, `links_routes.c`, `materials_routes.c` (projects and items), `motifs_routes.c` (the motif and variant routes; its rows come first because `/items/validate` is a word, not an id), `subjects_routes.c`, `composer_routes.c`, `jobs_routes.c` and `events_routes.c`. A file exports the rows `{method, pattern, handler}` of its family; `route_table.c` holds the one parser (`ursatz_gui_route_match`: a pattern's literal segments equal the path's, `<id>`, `<n>` and `<name>` take one segment each, at most 6 arguments of under 256 bytes, no empty segment) and the one civetweb handler for `/api`. A path no row fits is `404`, a path another method fits `405`, both in the envelope ([the envelope](api/envelope.md)). `events_routes.c` writes its own response.

The helpers of `route_util.c` are the only way a route reads or answers:

- `ursatz_gui_read_body` (and `ursatz_gui_read_json_body`): the one body reader with its caps (JSON 1 MB, uploads 64 MB, MIDI motifs 256 KB); over the cap is `413`, unreadable `400`.
- `ursatz_gui_query_params` and `ursatz_gui_request_params(conn, name, value, ..., NULL)`: the query variables of the request as a JSON object, with the path's arguments added.
- `ursatz_gui_answer_query(conn, name, params, bytes, failure, ...)` and `ursatz_gui_answer_command(conn, name, target, params, bytes, failure, ...)`: build the call (a command carries the actor `michelle` and the `base_version` the page sent), run it on the workshop and send the reply (`ursatz_gui_send_reply_ok`, `_json` or `_bytes`) or the failure.
- `ursatz_gui_failed(conn, status, error)` and `ursatz_gui_send_failure_at(conn, http, message, code, path)`: the envelope's failure, with the status of `ursatz_gui_http_status`, the one table that maps a workshop status to HTTP.
- `ursatz_gui_valid_id`: an id of 1 to 64 letters, digits, hyphens or underscores.

A route has no logic of its own: it names a workshop call and the shape of its params. What a query or command does is in the [workshop docs](../../workshop/docs/commands.md).

## Who calls the Composer

Each Composer area has one service in the workshop that makes its Composer API calls ([workshop architecture](../../workshop/docs/architecture.md)); the GUI makes none.

| Area | Service | Composer entries |
|---|---|---|
| Motifs | `workshop/src/composition/motifs.c` | `composer_api_motive`, `_motive_from_midi`, `_motive_render`, `_random_motive`, `_recognize`, `_validate`, `_variants`, `_variant_operations`; it also builds the motive request of a motif and analyzes a motif body on save |
| Themes | `workshop/src/composition/themes.c` | `composer_api_theme` |
| Pieces | `workshop/src/composition/pieces.c` | `composer_api_piece`, `_piece_variations`; a piece of any form: one plan, MIDI and MusicXML file for each version of its form (R-430) |
| Compose | `workshop/src/composition/compose_common.c`, `request_resolve.c` | the steps around a Composer run: update the uses, resolve the body's references into the request, run, commit the version with its files and layers in one transaction; `composer_api_export` writes the files at compose |
| Material export | `workshop/src/rendering/export.c` | `composer_api_motive_render` (through `motifs.c`); it gathers the MusicXML and the store values and the Analyzer's `analyzer_text_export_material` writes the text (`GET /api/materials/items/<id>/export`) |
| Build, vocabulary and forms | `workshop/src/composition/composer_client.c` | `composer_api_versions`, `_vocabulary`, `_forms` |

`composer_routes.c` answers the status route from `composer.status` and serves the vocabulary and the forms from the workshop's snapshot; the other route files name queries and commands only.

On the page, `frontend/public/core/request.js` (`ApiRequest`) holds `jsonRequest`, `sendRequest` (the one fetch of JSON: `{ok, status, data}`, a network failure is `{ok: false, status: 0, data: {error, code: "unreachable"}}`), `errorText`, `fieldForPath`, `validateRequest` and the base-version helpers; `core/api.js` the page's three JSON calls. The models and views call them and keep no copy.
