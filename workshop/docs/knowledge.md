# Knowledge queries

The workshop answers four queries about the knowledge (R-765), none of which changes anything. The commands that write her entries (`knowledge.add` and the rest) are added with the user knowledge commands. Back to the [docs index](README.md).

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
