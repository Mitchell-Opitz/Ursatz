# Workshop changelog

Newest first. [Docs index](README.md).

## P66 B4: settings (2026-11)

- **Settings.** The format `ursatz-settings` for a taste profile, a style, a project and an item, `ursatz-settings-snapshot` for a composed version; the resolver over its scopes with the source of each value; the queries `settings.resolve`, `settings.scope`, `settings.list` and `settings.keys`; the commands `settings.set`, `settings.unset`, `settings.import`, `settings.override`, `settings.switch` and `settings.use-taste` (55 commands); the param type `value`; the event `settings-changed`. `workshop_settings_resolve` is `workshop_config_resolve` (it resolves the app configuration, `ursatz.json`). See [settings](settings.md).

## P65 B8: restore a version (2026-11)

- **`item.restore`.** The next version of an item is a copy of an older one: its files, its body with the paths of its result in the new folder, the same uses; the envelope's `lineage` gains `restored_from` ([data folder](data-folder.md)). The commands are 49 ([commands](command-reference.md)).

## P65 B7: section commands (2026-11)

- **Section commands.** `section.insert`, `section.delete`, `section.move`, `section.resize` and `section.set` edit the sections of a piece version's request and save the next version ([commands](commands.md#the-section-commands)); the commands are 48. A param of the type `object` may name words that stand in place of the object (`words` in `workshop/commands`): `section.set`'s `region home` and `harmony none`.

## P65 B5: command schemas (2026-11)

- **Commands as schemas.** Each of the 43 commands is defined once in `workshop/commands/<family>.json` (meta-schema `workshop/schema/ursatz-commands.v1.schema.json`), embedded in the build by `cmake/embed_json.py`. `workshop_command` checks the target and the declared params from the schema (R-664; the messages are in [commands](commands.md#commands)), and `workshop_open` refuses a schema with no command or a command with no schema. The tool `commands` prints the generated [command reference](command-reference.md), the grammar and the help of each command ([tools](tools.md#commands)).
- **The actor of a version.** The envelope of a version records the actor of the command that made it (it was always `michelle`); `item.get`'s versions and `item.version` answer `actor` and `lineage`.

## P65 B3: Ursatz Text (2026-11)

- **The text tool and the view queries.** The tool `text` ([tools](tools.md)) prints a layer file or a text as Ursatz Text, a view of it or its JSON. The queries `item.view` and `library.view` print the layers a version or a library piece holds ([commands](commands.md#views-of-ursatz-text)); `library.view` replaces `library.export`, whose text export is its `reading` view, framed. The `analyze` tool's `text` answer is framed the same way.

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
