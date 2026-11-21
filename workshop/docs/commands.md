# Queries and commands

The two calls a program makes to the workshop, the names they take and what each answers. A third call, `workshop_tool`, works on files and not on the data folder: see [tools](tools.md). [Docs index](README.md).

## The calls

```c
UrsatzStatus workshop_query(Workshop *, const char *name, const cJSON *params, const WorkshopBytes *bytes, WorkshopReply *, UrsatzError *);
UrsatzStatus workshop_command(Workshop *, const cJSON *command, const WorkshopBytes *bytes, WorkshopReply *, UrsatzError *);
```

A **query** reads: it changes no file outside `cache/`, and a stale analysis is answered as stored with `stale: true`. A **command** changes: it is journaled under its name with its actor (see [the data folder](data-folder.md#changes-are-transactions)).

```json
{"command": "item.save", "actor": "michelle", "target": "mat-3f2a", "base_version": 4, "params": {"body": {}, "note": "longer ending"}}
```

- `command` is a name below; an unknown one, a missing `actor` or a `params` member of the wrong type is `URSATZ_ERR_REQUEST` naming its path (`/command`, `/actor`, `/target`, `/params/<member>`, `/base_version`).
- `actor` says who asked; the GUI backend sends `michelle`, `ursatz apply` `claude`. It is written into the journal and the envelope of every version the command makes (a job writes under the actor that queued it; a version made by a migration is `michelle`'s). `item.get`'s versions and `item.version` answer each version's `actor` and `lineage`.
- `target` is the id of what the command changes; a command that creates something has none.
- `base_version` is the version of the item the caller read. A command that changes an item that has versions (marked **base** below) is refused with `URSATZ_ERR_CONFLICT`, "changed since you read it", path `/base_version`, and `current_version` in the reply when it is not the item's current version. The store also refuses a version write of the command's target made against another version, so a compose job that finds a newer version when it writes fails the same way (409 in its result). A write of the target moves the command's base to the version it wrote, so `item.compose` with `update_uses` (a version that moves the uses, then the composed one) is not refused for its own first write. A missing item is not found, not a missing base version. Projects, subjects, collections, styles and link runs have no versions and take none. Four more commands are marked **base**: `item.save-variant`, `item.set-op-checks`, `item.variations` and `ratings.rate-version`. A write without a command context (an actor and a base version) is refused: every command runs in one, and the startup import runs in its own.
- `bytes` carries a file in (an upload, a MIDI file), and the reply carries one out (`bytes`, `content_type`, `filename`).
- The reply holds the members of the answer (the HTTP layer adds `ok`) and, for a command, `journal_seq`: the last record the command's thread committed (0 for a command whose job writes it later, or for one that writes none). A reply is also filled when the status is not `URSATZ_OK` and the answer has members (a refused delete lists `used_by`).

A command that runs long answers with its job (`{"job": {"id", "kind", "state"}}`); see [jobs and events](jobs-and-events.md).

## Commands

62 commands; `job.cancel` is a runtime control and writes no record, the others are journaled under their name. Each is defined once, as a schema in `workshop/commands/<family>.json` (target, base version, params with their types and limits, reply, job, example); `workshop_command` checks a command's target and params from it before the handler runs, and the generated [command reference](command-reference.md) lists every command with its params. The tool `commands` prints the reference (`sheet`, `markdown`), the grammar of the text form, every schema as JSON and the help of one command ([tools](tools.md#commands)); a command written as text is read by `ursatz apply` ([data folder](../../cli/docs/data-folder.md)).

A check that fails is `URSATZ_ERR_REQUEST` at its path: `/target` (`<command> needs a target`, `<command> takes no target`), `/params/<name>` (`<name> is required`, `<name> must be a string`, `a whole number`, `true or false`, `one of a, b, c`, `a rational like 3/4`, `an object`, `a list`; `<name> must be from <min> to <max>`; `<name> is longer than <n> characters`; `/params/<name>/<i>` for the item of a list) and `give exactly one of <a>, <b>`. A member no schema declares passes until the GUI sends exactly the declared params; the handlers keep the checks of meaning (a note a motif needs, a thing that is not found, a conflict).

### The ratings commands

`ratings.session`, `ratings.batch`, `ratings.rate`, `ratings.pick`, `ratings.rate-version` and `ratings.keep` record what was listened to and keep a setting as a taste default citing the ratings behind it ([ratings](ratings.md)).

### The settings commands

`item.compose` and `item.explore` take `settings`, `overrides`, `off` and `taste` for one compose ([settings](settings.md#composing-with-settings-and-exploring)).

`settings.set`, `settings.unset`, `settings.import`, `settings.override`, `settings.switch` and `settings.use-taste` change one scope file of the [settings](settings.md): each takes a `scope` (`style:<id>`, `project:<id>`, `item:<id>`, and `taste:<name>` for an import) and answers with the scope's content. Their `value` param is a number, true or false, a word, `n/d` or a list.

### The section commands

`section.insert`, `section.delete`, `section.move`, `section.resize` and `section.set` change the sections of a piece (R-666). Each takes a piece item with the version read (`item:<id>@v<n>`), edits the Composer request of that version (`body.request`), checks the edit as `item.validate` does (the references resolved, then `composer_api_validate` of kind `piece`) and saves it as the next version, with the note given or one that says what changed (`insert section T0 after s0`). The new version holds the body only (no result); `item.compose` composes it. The reply is `{version, sections}`: the new version number and the ids of the sections in order.

- A game loop request (`schema_version` 1, or `form` `game-loop`; a migrated loop piece is the latter) takes every command on its own `sections`. A section is inserted as `{id, role, letter?, bars|beats, pickup?, cadence?, function?, fill?, region?, harmony?}`; a section that says `part` keeps it; `section.resize` writes the unit given and removes the other; `section.set` takes the words `none` (function, cadence, harmony) and `home` (region) to remove a member.
- A form piece takes its sections from its form: only `section.set` of `region`, `harmony` and `fill` is allowed, written into the entry of its `sections` overlay with that `name` (made when missing); anything else is refused at `/target`, `a form piece takes its sections from its form`.
- An unknown `id`, `after` or `before` is `URSATZ_ERR_NOT_FOUND` at its param; an `id` taken is `URSATZ_ERR_CONFLICT` at `/params/id`; deleting the last section is refused, `a piece keeps at least one section`; the Composer's refusal of the edit keeps its path into the request (`/request/sections/<i>/region`). A target that is not a piece is refused at `/target`, `section commands change a piece`. Composing the new section into a kept piece is later work (P71).

## Queries

Names are `<family>.<what>`; params go in one JSON object. `id` names the thing asked for.

| Family | Queries |
|---|---|
| Library | `library.list` (filters `composer`, `source`, `tag`, `unassigned`, `untagged`, and `collection`: its members only), `library.groups` (composers, sources, tags and their counts, `collection` as above), `library.piece` (`id`, `document=summary` for the summary only), `library.document` (`id`, `sections`), `library.view`, `library.musicxml`, `library.meter-defaulted`, `library.playback` |
| Collections | `collection.list`, `collection.get`, `collection.pieces` |
| Composer | `composer.status`, `composer.vocabulary`, `composer.forms`, `composer.variant-operations`, `composer.render`, `composer.random-motifs`, `composer.import-motif` (a MIDI file in `bytes`), `composer.recognize` |
| Styles and links | `style.list`, `style.get`, `links.list`, `links.get`, `links.piece` |
| Jobs | `job.get`, `job.list` (queued and running jobs, newest first) |
| Journal | `journal.list` (`target`: a record's target, `limit`: 1 to 1000, default 20; the records newest first, each `{seq, id, command, actor, at, target, base_version, writes, removes}` with the final paths it wrote and removed) |
| Projects and items | `project.list`, `project.get`, `item.list`, `item.get`, `item.version`, `item.view`, `item.export`, `item.score`, `item.playback`, `item.file`, `item.variation-file`, `item.variation-playback`, `item.op-checks`, `item.variant-tree`, `item.validate`, `item.variants`, `item.each-op` |
| Subjects | `subject.list`, `subject.groups`, `subject.get` |
| Ratings | `ratings.session` (`session`), `ratings.sessions`, `ratings.item` (`item_id`) ([ratings](ratings.md)) |
| Settings | `settings.resolve` (`project_id`, `item_id`, `style_id`, `taste`, `settings`, `overrides`, `off`, `all`), `settings.scope` (`scope`), `settings.list`, `settings.keys` ([settings](settings.md)) |

`item.validate` resolves the references of a body and calls `composer_api_validate`; it answers the Composer's refusal with its path (`/request/...`). `item.variants` and `item.each-op` run the Composer's variant engine on a motif body and answer the candidates without storing them (the page stores one with `item.save-variant`). `library.piece` also publishes `analysis-stale` when the piece's analysis is stale.

## Failures

A failure is a `UrsatzStatus` and a `UrsatzError {message, path}` (`common/status.h`): `REQUEST` (malformed or invalid input), `UNSATISFIABLE` (valid, but nothing satisfies it), `NOT_FOUND`, `CONFLICT` (exists, still referenced, busy, or changed since read), `FORMAT` (a stored or imported file that does not read), `IO`, `CANCELLED`, `ALLOC`, `INTERNAL`. A `path` is a JSON Pointer into the call (`/params/...`, `/target`, `/bytes`, `/base_version`) or into the request the Composer reads (`/request/...`); the GUI maps the pair to an HTTP status in one table (`REQUEST` 400, or 422 when the path points into a Composer request; `UNSATISFIABLE` 422; `NOT_FOUND` 404; `CONFLICT` 409; `CANCELLED` 499; the rest 500; a body over its cap 413).

## Views of Ursatz Text

Two queries print the layers a version or a library piece holds as Ursatz Text views ([text](../../library/text/README.md)).

- `item.view` (`id`, `version` (the current one when absent), `view` (default `summary`), `measures` (`{first, last}`), `section`) reads the version folder's `plan.json`, `realization.json` and `readings/composer.json` (the ones present) and answers `{id, view, text}`. `full` is each present layer's text, plan first, joined by a blank line; a version with no layer gives its body as a `json` document. `json` answers what `item.version` answers. A version with no layer refuses the other views with `URSATZ_ERR_NOT_FOUND`, path `/params/view`, `this version holds no plan, realization or reading`.
- `library.view` (`id`, `view` (default `summary`), `measures`, `section`, `title`) replaces `library.export`. `reading` is the Analyzer's text export as it was (`measures` and `title` as `from`, `to` and the file name were), framed as a view: `ursatz-text 1 view reading piece:<id>`, the export, `# leaves out: nothing the analysis document holds; the notes are the import's`, `# expand: ...`. `summary`, `structure`, `harmony` and `part:<name>` read the piece's `readings/analyzer.json` and `realization.json`; `full` is their texts; `json` answers what `library.piece` answers. A piece analyzed before the layers were stored refuses the layer views with `URSATZ_ERR_NOT_FOUND`, path `/params/view`, `re-analyze the piece to store its layers`.

Both refuse as the tool `text` does: a view a layer cannot give at `/params/view`, an unknown section at `/params/section`, a measure range outside the piece at `/params/measures`.
