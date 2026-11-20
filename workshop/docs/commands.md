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
- `actor` says who asked; the GUI backend sends `michelle`. It is written into the journal and the version envelope.
- `target` is the id of what the command changes; a command that creates something has none.
- `base_version` is the version of the item the caller read. A command that changes an item that has versions (marked **base** below) is refused with `URSATZ_ERR_CONFLICT`, "changed since you read it", path `/base_version`, and `current_version` in the reply when it is not the item's current version. The store also refuses a version write of the command's target made against another version, so a compose job that finds a newer version when it writes fails the same way (409 in its result). A write of the target moves the command's base to the version it wrote, so `item.compose` with `update_uses` (a version that moves the uses, then the composed one) is not refused for its own first write. A missing item is not found, not a missing base version. Projects, subjects, collections, styles and link runs have no versions and take none.
- `bytes` carries a file in (an upload, a MIDI file), and the reply carries one out (`bytes`, `content_type`, `filename`).
- The reply holds the members of the answer (the HTTP layer adds `ok`) and, for a command, `journal_seq`: the last record the command's thread committed (0 for a command whose job writes it later, or for one that writes none). A reply is also filled when the status is not `URSATZ_OK` and the answer has members (a refused delete lists `used_by`).

A command that runs long answers with its job (`{"job": {"id", "kind", "state"}}`); see [jobs and events](jobs-and-events.md).

## Commands

42 commands change the data folder; `job.cancel` is a runtime control and writes no record.

| Command | Target | Params | Base | Reply |
|---|---|---|---|---|
| `library.import` | | `filename`; the file in `bytes` | | the job (`analysis`) |
| `library.update` | piece | `filename`, `composer`, `source`, `tags`, `add_tags`, `remove_tags`, `notation` (an object, or null to clear) | | the piece |
| `library.delete` | piece | | | `{}`: the piece leaves every collection and subject in the same transaction |
| `library.reanalyze` | piece | | | the job (`analysis`) |
| `collection.create` | | `name`, `description`, `id` | | the collection |
| `collection.update` | collection | `name`, `description` | | the collection |
| `collection.delete` | collection | | | |
| `collection.add-pieces` | collection | `piece_id` or `piece_ids` | | `{added, already}` |
| `collection.remove-piece` | collection | `piece_id` | | |
| `style.build` | | `name` and exactly one of `filter`, `collection_id`, `piece_ids`, `brief` | | the job (`style`) |
| `style.rebuild` | style | | | the job (`style`) |
| `style.delete` | style | | | |
| `links.run` | | `scope` (`all`, `collection`, `filter`, `subject`, `project`) | | `{run, job}`; a second run waits in the queue |
| `links.delete` | run | | | the run's job is cancelled first |
| `project.create` | | `name`, `description` | | the project |
| `project.update` | project | `name`, `description` | | the project |
| `project.delete` | project | | | refused while it holds items |
| `item.create` | | `type`, `name`, `project_id`, `body`, `note` | | the item (a motif's body is analyzed first) |
| `item.update` | item | `name`, `project_id` | base | the item (a move copies the folder into the other project, then removes the old one) |
| `item.delete` | item | `force` | base | the item and its variants go; refused with `used_by` while another item uses it, unless forced |
| `item.save` | item | `body`, `note` | base | the new version |
| `item.reanalyze` | item | | base | the new version |
| `item.compose` | item | `seed`, `update_uses` | base | the job (`theme` or `piece`) |
| `item.regenerate` | item | `version`, `section_id`, `lane`, `seed` | base | the job (`regenerate`): a section again as the next version |
| `item.variations` | item | `version`, `section_id`, `lane`, `count` (1 to 8), `seed` | | the job (`variations`): a new set of files |
| `item.choose` | item | `version`, `set`, `index` | base | the job (`choose`): a variation as the next version |
| `item.set-op-checks` | item | `checks` | | the marks of Try every operation (not a version) |
| `item.save-variant` | motif | `variant` (a candidate of `item.variants`), `name`, `flag` | | the new item |
| `item.flag` | variant | `flag` (`approved`, `rejected`, `unflagged`), `kind` | base | the item |
| `item.promote` | variant | the promote request (`name`, `project_id`; see the [materials routes](../../gui/docs/api/materials-routes.md)) | base | the new item |
| `item.link-form` | variant | the link request (see the [materials routes](../../gui/docs/api/materials-routes.md)) | base | the item |
| `item.reroll` | item | | base | the job (`reroll`): the stored chain again with a new seed |
| `item.update-use` | item | `source_id` | base | the version that follows the source's current one |
| `subject.create` | | `name`, `kind`, `note` | | the subject |
| `subject.update` | subject | `name`, `kind`, `note` | | the subject |
| `subject.delete` | subject | | | |
| `subject.link-item` / `subject.unlink-item` | subject | `item_id` | | |
| `subject.link-piece` / `subject.unlink-piece` | subject | `piece_id` | | |
| `subject.relate` / `subject.unrelate` | subject | `other_id` | | |
| `job.cancel` | job | | | a queued job stops at once, a running one at its next report |

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

`item.validate` resolves the references of a body and calls `composer_api_validate`; it answers the Composer's refusal with its path (`/request/...`). `item.variants` and `item.each-op` run the Composer's variant engine on a motif body and answer the candidates without storing them (the page stores one with `item.save-variant`). `library.piece` also publishes `analysis-stale` when the piece's analysis is stale.

## Failures

A failure is a `UrsatzStatus` and a `UrsatzError {message, path}` (`common/status.h`): `REQUEST` (malformed or invalid input), `UNSATISFIABLE` (valid, but nothing satisfies it), `NOT_FOUND`, `CONFLICT` (exists, still referenced, busy, or changed since read), `FORMAT` (a stored or imported file that does not read), `IO`, `CANCELLED`, `ALLOC`, `INTERNAL`. A `path` is a JSON Pointer into the call (`/params/...`, `/target`, `/bytes`, `/base_version`) or into the request the Composer reads (`/request/...`); the GUI maps the pair to an HTTP status in one table (`REQUEST` 400, or 422 when the path points into a Composer request; `UNSATISFIABLE` 422; `NOT_FOUND` 404; `CONFLICT` 409; `CANCELLED` 499; the rest 500; a body over its cap 413).

## Views of Ursatz Text

Two queries print the layers a version or a library piece holds as Ursatz Text views ([text](../../library/text/README.md)).

- `item.view` (`id`, `version` (the current one when absent), `view` (default `summary`), `measures` (`{first, last}`), `section`) reads the version folder's `plan.json`, `realization.json` and `readings/composer.json` (the ones present) and answers `{id, view, text}`. `full` is each present layer's text, plan first, joined by a blank line; a version with no layer gives its body as a `json` document. `json` answers what `item.version` answers. A version with no layer refuses the other views with `URSATZ_ERR_NOT_FOUND`, path `/params/view`, `this version holds no plan, realization or reading`.
- `library.view` (`id`, `view` (default `summary`), `measures`, `section`, `title`) replaces `library.export`. `reading` is the Analyzer's text export as it was (`measures` and `title` as `from`, `to` and the file name were), framed as a view: `ursatz-text 1 view reading piece:<id>`, the export, `# leaves out: nothing the analysis document holds; the notes are the import's`, `# expand: ...`. `summary`, `structure`, `harmony` and `part:<name>` read the piece's `readings/analyzer.json` and `realization.json`; `full` is their texts; `json` answers what `library.piece` answers. A piece analyzed before the layers were stored refuses the layer views with `URSATZ_ERR_NOT_FOUND`, path `/params/view`, `re-analyze the piece to store its layers`.

Both refuse as the tool `text` does: a view a layer cannot give at `/params/view`, an unknown section at `/params/section`, a measure range outside the piece at `/params/measures`.
