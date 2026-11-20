# Data folder commands

The commands that read and change a [data folder](../../workshop/docs/data-folder.md): `status`, `query`, `show`, `history`, `apply`, `migrate`, and the item forms of `compose`, `regenerate` and `render`. Back to the [cli docs](README.md).

The folder is named before the subcommand, with `--data <dir>` or the environment variable `URSATZ_DATA`. The command opens the workshop on it, or connects to the process that holds it ([connect or run](README.md#connect-or-run)); the answers are the same. Every change is a command of the workshop with an actor, a journal record and a base version: `--actor <name>` (default `claude`) names who did it.

```sh
ursatz --data ~/ursatz-data status
ursatz --data ~/ursatz-data query project.list
ursatz --data ~/ursatz-data show item:itm-3f2a
ursatz --data ~/ursatz-data compose item:itm-3f2a --seed 7
```

## status

`ursatz status [--format <text|json>]` prints the folder, whether this command opened it (and with how many job workers) or another Ursatz process serves it (its process id and port), the versions of the Composer and the Analyzer and, when the open recovered a stopped change, the recovery line. Exit 2 when the folder cannot be opened or reached.

## query

`ursatz query <name> [--params <file>] [--out <file>] [--format <text|json>]` calls a query of the workshop ([queries](../../workshop/docs/commands.md#queries)) and prints the answer as JSON (on one line with `--format json`). `--params` names a JSON file with the params object, or `-` for standard input; the default is `{}`. A query that answers a file (an export, a playback file) needs `--out <file>`. A query changes nothing.

```sh
echo '{"id": "item-1"}' | ursatz --data ~/ursatz-data query item.get --params -
```

## Addresses, show and history

An address names a thing in the folder: `<kind>:<id>`, or `item:<id>@v<n>` for one version of an item.

| Address | What it is | Query |
| --- | --- | --- |
| `item:<id>` | an item with its versions | `item.get` |
| `item:<id>@v<n>` | one version of an item | `item.version` |
| `piece:<id>` | a Library piece | `library.piece` |
| `project:<id>` | a project | `project.get` |
| `subject:<id>` | a subject | `subject.get` |
| `collection:<id>` | a collection | `collection.get` |
| `style:<id>` | a style | `style.get` |
| `links:<id>` | a motive link run | `links.get` |
| `job:<id>` | a job | `job.get` |

`ursatz show <address> [--view json]` prints what the workshop holds under the address as JSON; json is the only view for now. `ursatz history [<address>] [--limit <n>] [--format <text|json>]` lists the journal, newest first (`--limit` 1 to 1000, default 20): one line a record with the sequence number, the time, the actor, the command and its target; `--format json` prints the records with the paths each wrote and removed. An address limits it to the records about that target.

## apply

`ursatz apply [<file>] [--wait] [--format <text|json>]` applies commands one after the other. They come from the file, or from standard input (`-` or no file): JSON Lines, or one JSON array. A command is an object:

```json
{"command": "item.save", "target": "itm-3f2a", "base_version": 3, "params": {"note": "longer ending", "body": {}}, "file": "motif.mid"}
```

Only `command` is needed. `file` is a path whose bytes go with the command (`library.import`), named by its base name. An `actor` member is refused: the actor comes from `--actor`. The names and params are the workshop's [commands](../../workshop/docs/commands.md). All the commands are checked, and every file read, before the first runs.

Each command prints its number, name, target (`-` for none) and the journal sequence it wrote, and `job <id>` when it started a job; with `--wait` the job is waited for and its state follows (a failed job also prints its error). The first command the workshop refuses stops the run; the ones before it stay applied, each is its own transaction. A stale base version says `changed since you read it` and the `current version`. Exit 0 all applied; 1 usage or a command refused as a request; 2 anything else.

## compose, regenerate and render of an item

```sh
ursatz compose item:<id> [--seed <n>] [--update-uses] [--base <n>] [--no-wait]
ursatz regenerate item:<id> --section <id> [--lane <lane>] [--seed <n>] [--version <n>] [--base <n>] [--no-wait]
ursatz render item:<id>[@v<n>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi]
```

`compose` composes a theme or piece into its next version, `regenerate` writes one section of a piece version again as the next version. Both start a job, wait for it and print `item <id> version <n>`; a failed job prints `Error: <its error>` and exits 2; `--no-wait` prints the job's id. The command is made against `--base` (default: the item's current version): a newer version refuses it. `--update-uses` moves every updated use of a theme to its source's current version first. `regenerate` takes the section of `--version` (default: the current version).

`render` renders the `plan.json` of a version (the current one when the address has none) to MIDI or MusicXML as `render <plan.json>` does; a motif has no plan (exit 2). `regenerate <plan.json>` and `render <plan.json>` stay the forms for a file.

## migrate

`ursatz migrate [--dry-run] [--backup <dir>]` migrates a data folder of an older layout to the current one (layout 2); the GUI backend no longer does. `--dry-run` writes nothing and reports every planned move and rewrite. A real run copies the whole folder to `--backup` first (default: beside it, `<folder>-backup-<time>`), migrates and prints the report as JSON. The folder must not be open: while a process holds it, the command exits 2 with `stop the Ursatz process <pid> first`. A folder in an older layout is refused by every other command with the two commands to run (`migrate --dry-run`, then `migrate`). See [older folders](../../workshop/docs/data-folder.md#older-folders-and-migration).
