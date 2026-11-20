# Workshop architecture

What the workshop is, how its source is laid out and what calls what. [Docs index](README.md).

## What it is

The workshop is the one process that holds the data folder and the services over it. It sits above the two apps and below the GUI (R-641, R-537):

```
GUI backend (routes)          one route table over workshop_api.h: parse, call, answer, no logic
  ursatz (cli)                the second client: opens the folder, or connects to the process that holds it (workshop routes)
  workshop                    data folder, transactions, jobs, events, queries and commands
    Analyzer   Composer       every music answer (analyzer_api.h, composer_api.h)
      library                 theory, models, readers and writers
        common                status, files, strings, JSON, hash, job handle, events
```

It holds no music logic: a query or command reads or changes files and asks the Analyzer or the Composer. The one header a program includes is `workshop/include/ursatz_workshop/workshop_api.h`; the library exports only its 23 names (`workshop/src/exports.txt`, checked by the test `app_exports`): `workshop_settings_resolve`, `_open`, `_start`, `_stop`, `_close`, `_query`, `_command`, `_reply_destroy`, `_migrate`, `_status`, `_events_subscribe/next/unsubscribe/close`, `_recovery_summary` and the settings accessors (`_data_dir`, `_port`, `_http_threads`, `_job_workers`).

## Life of a workshop

1. `workshop_settings_resolve` merges defaults, `<data>/ursatz.json`, the environment and the flags ([settings](data-folder.md#settings)).
2. `workshop_open` creates or checks the data folder (`workshop_data_folder_prepare`: a layout 2 folder passes, an empty one gets `data-folder.json`, an older one is refused with the commands to run), takes the lock, runs recovery ([transactions](data-folder.md#changes-are-transactions)) and builds the knowledge snapshot and the Composer context.
3. `workshop_start` starts the stores, the link run index, the job queue and the event bus, and makes the data folder the process's working directory (the services' paths are relative to it).
4. `workshop_query` and `workshop_command` ([commands](commands.md)), `workshop_tool` ([tools](tools.md)), `workshop_events_*` ([jobs and events](jobs-and-events.md)).
5. `workshop_stop` cancels every job and joins the workers; `workshop_close` releases the folder.

## Source layout

`workshop/src/`, lowest layer first. Each folder is a module of `cmake/layer-table.txt`; a module may include only the modules its `uses` row names (the test `layer_table`).

| Folder | Holds |
|---|---|
| `base/` | what every service uses: the command context (name, actor, target, base version, per thread), the event hub, the HTTP-status table of job results, the knowledge snapshot, the last error |
| `jobs/` | the job queue ([jobs](jobs-and-events.md)) |
| `items/` | the store: layout and formats, the transaction, journal and recovery, projects and items with their versions, bodies and references, layers, subjects, collections, the Library piece store and metadata, the migration and the data check |
| `analysis/` | the Analyzer's services: import and re-analysis (and their jobs), styles, motive links, the import cache |
| `composition/` | the Composer's services: compose jobs, themes, pieces, motifs, the variant engine ([variants](variants.md)), the request resolver, the Composer client |
| `rendering/` | the render cache, playback of versions and pieces, the export of a version's files |
| `coordinator/` | the API: `workshop.c` (open, start, stop), `dispatch.c` (the two tables of names), `api_*.c` (one file per family of handlers), `events.c`, `settings.c`, `data_lock.c` |

Rules the layer table holds: `items/` does not reach up to `analysis/`, `composition/` or `rendering/`, so a function that needs the analysis layer to cascade (`workshop_library_piece_json`, `_patch`, `_delete`) lives in `analysis/analysis.c`; the Composer's vocabulary is not read by the store at all (R-649), only by the Composer client; the one cycle left between the store and the subjects is a registered callback (`workshop_store_set_forget_item`).

## Commands as schemas

Every command is defined once, as a schema in `workshop/commands/<family>.json` (embedded into the build): its target, base version, params with their types and limits, reply and an example. `workshop_command` checks a call against its schema before the handler runs and refuses a target, a param or a type the schema does not declare at its path (`/params/<name>`). The generated sheet [command-reference.md](command-reference.md), the text grammar of `commands` and `ursatz help` read the same schemas, so a command added in one place appears in all of them ([commands](commands.md#commands)).

## Locks

The transaction is the outermost lock: a writer begins it (`workshop_txn_begin`, one per process, a mutex) before it takes its service's lock; commit and abort release it and free it. A reader never sees a half change: the piece lock (`items/piece_lock.c`, one mutex per piece folder) is held from the first read of a re-analysis, update or delete through its commit. The link run index lock is taken after the service lock, never before. The process lock is `.ursatz.lock`.

## Tests

`workshop/tests/`: `workshop_open`, `workshop_settings`, `workshop_lock`, `workshop_formats` (validates every format against its schema), `workshop_migrate`, `workshop_body_migration`, `workshop_request_resolve` (39 recorded requests resolve to the recorded requests member for member), `workshop_recovery`, `workshop_crash_points` (slow: every step of every command kind it names) and its smoke test, `workshop_layers`, `workshop_commands`, `workshop_queries`, `workshop_base_version`, `workshop_variants`, `workshop_jobs`, `workshop_events` and the service tests that moved with the services (`item`, `subject`, `link`, `piece lock`, `piece metadata`, `id`, `stale analysis`). The GUI's route tests play the whole stack through HTTP ([GUI testing](../../gui/docs/testing.md)).
