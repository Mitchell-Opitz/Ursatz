# Workshop changelog

Newest first. [Docs index](README.md).

## P64: tools (2026-11)

- **Tools.** A third call, `workshop_tool`, works on the files its params name and never on the data folder (R-656). `workshop_tools_open` opens a workshop with no data folder; `workshop_api.h` has 21 exported names. The tools: `composer` (every Composer API entry), `corpus-passages`, `pitch-names` and `versions`, then the Analyzer's `analyze`, `corpus` and `style-profile` ([tools](tools.md)). The analysis failure keeps its status (A20261115-79).

## P64 B8: the data folder from ursatz (2026-11)

- **Journal query.** `journal.list` (params `target`, `limit`) answers the journal's records newest first, for `ursatz history`. The migration moved to `ursatz migrate`: the refusals of `workshop_data_folder_prepare` name it, and the GUI backend no longer takes `--migrate`.

## P64 B7: connect or run (2026-11)

- **The command line as a client.** The lock file holds `<pid> <port>` once a process serves HTTP (`workshop_announce`), and `workshop_lock_holder` finds that process; `ursatz --data <dir>` opens the folder, or connects to the process that holds it through the [generic routes](../../gui/docs/api/workshop-routes.md) (R-659). The exported names are 23.

## P63: the workshop (2026-11)

- **The part.** `workshop/` is a new part with one API header, `workshop_api.h` (19 exported names), and the services the GUI backend held before: the store, the Library piece store, styles, motive links, motifs, themes, pieces, the variant engine, playback and export, and the job queue. The GUI backend hosts it (R-641).
- **Data folder layout 2.** One folder layout and one reader and writer for every file the workshop owns, each with a `format`, a `format_version` and a JSON schema (R-642). `ursatz-gui-backend --migrate [--dry-run] [--backup <dir>]` brings a layout 0 or layout 1 folder forward after a verified backup ([data folder](data-folder.md#older-folders-and-migration)).
- **Atomic saves.** Every change is a transaction committed through `journal/journal.jsonl`; a stop at any step leaves the old or the new state, and the open recovers (R-643).
- **Layers in the store.** A Library piece and a composed version keep `realization.json` and a reading beside their documents (R-644).
- **One job queue.** Composes, variants, link runs, style builds, imports and re-analyses are jobs; a second link run or style build waits in the queue; queries never write (R-645).
- **Queries and commands.** `workshop_query` and `workshop_command` replace the GUI's service calls: 45 queries, 42 changing commands and `job.cancel`, each command with an actor and, for an item, the version it was based on (R-646, [commands](commands.md)).
- **Events.** The workshop publishes changes and job progress on a bus (`common/events.h`); the GUI streams them at `GET /api/events` (R-648, [jobs and events](jobs-and-events.md#events)).
- **The Composer is the only checker.** The store checks only what it owns; a draft may hold any word, and validate and compose answer the Composer's refusal with its path (R-649).
- **Bodies as Composer requests.** A theme or piece body (format 2) holds the request as sent, with `$item` and `$pool` references a resolver replaces before the call (R-650).
- **A collection's members.** `library.list` and `library.groups` take `collection` and answer that collection's members only.
