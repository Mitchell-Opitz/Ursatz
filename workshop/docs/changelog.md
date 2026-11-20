# Workshop changelog

Newest first. [Docs index](README.md).

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
