# Knowledge queries

The workshop answers four queries about the knowledge (R-765), none of which changes anything. The commands that write her entries are [below](#the-commands). Back to the [docs index](README.md).

The snapshot they read is the one the workshop opened with: the built-in entries and the `.json` files of the data folder's `knowledge/` folder, whose ids start `user.`.

## knowledge.palette

Params `key` (required, as "C minor") and `with` (a mode the Composer writes keys in other than the key's own). The answer is the Composer's `composer_api_palette` document unchanged ([palette](../../composer/docs/harmony-lab.md#the-palette)): the chords of the key's material and of `with` on the same tonic, each with its chord text, the ones both hold and the color carriers. The Composer's refusals pass through with their paths (`/key`, `/with`).

## knowledge.find

Params `words` (required: words separated by spaces) and `kind` (optional). The entries whose id, name or any alias holds every word, in id order, case ignored, at most 50:

```
{"entries": [{"id", "kind", "subtype", "name", "status", "user"}], "more": false}
```

`subtype` is left out for a kind with none; `user` is true for an id that starts `user.`; `more` is true when more than 50 match. A `kind` no entry has is `URSATZ_ERR_NOT_FOUND` at `/params/kind`.

## knowledge.show

Params `id` (required) and `view`, `json` (the default) or `text`. `json` answers `{"view": "json", "entry": <the entry as loaded>}`, `text` `{"view": "text", "text": <the entry as Ursatz Text>}` (the knowledge-entry kind of `text_from_json`). An unknown id is `URSATZ_ERR_NOT_FOUND` at `/params/id`.

## knowledge.check

Params `file`, an object: a knowledge document. A snapshot is built from the built-in entries, the data folder's `knowledge/*.json` and this document (named `file` in messages), then freed; the workshop's own snapshot is not touched. `{"ok": true, "entries": n}` with the document's entry count, or `URSATZ_ERR_REQUEST` at `/file` with the loader's message (`file: <id>: <field>: <problem>`). An id that is already in the folder is a duplicate, so check a file before it is copied into the folder.

## On the command line

`ursatz knowledge find|show|check` ([cli-knowledge.md](../../cli/docs/cli-knowledge.md)) calls them on the data folder, or, with no data folder, their tools of the same names (`knowledge-find`, `knowledge-show`, `knowledge-check`) on a tools workshop, which reads the built-in entries only.

## The commands

The `knowledge` family (R-768) writes her entries in `<data>/knowledge/user.json` (`{"schema": "ursatz-knowledge", "schema_version": 1, "source": "user", "entries": []}`; a missing file is an empty one) and nothing else: her other files in the folder stay hers to edit. Each command is journaled in one transaction, as `settings.set` is.

- `knowledge.add` (`entry`, `note`): an entry whose id starts `user.` and that the snapshot does not hold. A duplicate is `URSATZ_ERR_CONFLICT` at `/entry/id`.
- `knowledge.update` (`id`, `entry`, `note`) and `knowledge.delete` (`id`, `note`): an entry of `user.json`. An id another file of the folder defines is refused with "`<id>` is in `<file>`; edit that file"; an id that is nowhere is not found.
- `knowledge.keep-schema` (`item`, `version`, `name`, `slug`, `loop`, `note`): the composed harmony version's `result.harmony.stages` as `user.schema.<slug>`, a progression schema of hers (`role` `loop` or `cadence`, `materials` the plan's mode and `with`, a `michelle` citation "kept from `<item>` version `<n>`", and the plan's chord texts as its example). No music is computed in the workshop: the stages are the Composer's. A version that is not composed is refused at `/params/version`.

Each answers the entry as stored (`{"entry": ...}`; delete answers the id). Before the commit a snapshot is built from the built-in entries, the folder's other files and the candidate `user.json`; a refusal is the loader's message (`knowledge/user.json: <id>: <field>: <problem>`) at `/entry`, and nothing is written. After the commit the workshop's snapshot is built again (`workshop_knowledge_rebuild`; the old one is kept until the workshop closes, since a running job may hold it) and the Composer's vocabulary is forgotten (`workshop_composer_vocabulary_reset`), so a new voicing, pattern or schema is offered by `composer.vocabulary` and read by the next request at once. The commands publish the journal's change as every command does.
