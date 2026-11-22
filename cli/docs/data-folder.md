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

An address names a thing in the folder: `<kind>:<id>`, `item:<id>@v<n>` for one version of an item, and `/<section>` after an item or a piece address (`item:<id>@v<n>/<section>`, `piece:<id>/<section>`) for one section of it.

| Address | What it is | Query |
| --- | --- | --- |
| `item:<id>` | an item with its versions | `item.view` (`item.get` for json) |
| `item:<id>@v<n>` | one version of an item | `item.view` (`item.version` for json) |
| `piece:<id>` | a Library piece | `library.view` (`library.piece` for json) |
| `project:<id>` | a project | `project.get` |
| `subject:<id>` | a subject | `subject.get` |
| `collection:<id>` | a collection | `collection.get` |
| `style:<id>` | a style | `style.get` |
| `links:<id>` | a motive link run | `links.get` |
| `job:<id>` | a job | `job.get` |

`ursatz show <address|file> [--view <view>] [--measures <first>-<last>]` prints what the workshop holds under the address, or what a layer file (a plan, a realization, a reading) holds, as a view in Ursatz Text ([the grammar and the views](../../library/text/README.md)) or as JSON. `--view` is `summary` (the default for an item, a piece and a file), `structure`, `harmony`, `line:<name>`, `part:<name>`, `reading`, `full` (the lossless text of every layer the address holds) or `json` (indented). `--measures` limits a view to bars `<first>-<last>` (bars count from 1); a section limits it to its bars. The other kinds of address (`project`, `subject`, `collection`, `style`, `links`, `job`) show only `--view json`. A view starts `ursatz-text 1 view <view> <address>` and ends with the lines `# leaves out:` and `# expand:`, which say what it leaves out and the command that shows more. An item address calls `item.view`, a piece address `library.view`, a file the tool `text`.

```sh
ursatz --data ~/ursatz-data show item:itm-3f2a@v2 --view harmony --measures 1-8
ursatz --data ~/ursatz-data show item:itm-3f2a@v2/A --view structure
ursatz show plan.json --view line:melody
```

`ursatz history [<address>] [--limit <n>] [--format <text|json>]` lists the journal, newest first (`--limit` 1 to 1000, default 20): one line a record with the sequence number, the time, the actor, the command and its target; `--format json` prints the records with the paths each wrote and removed. An address limits it to the records about that target.

## apply

`ursatz apply [<file>] [--wait] [--format <text|json>]` applies commands one after the other. They come from the file, or from standard input (`-` or no file), as Ursatz Text or as JSON (JSON when the first character is `{` or `[`).

Ursatz Text is one command a line: `<command> [<kind>:<id>[@v<n>]] {<param> <value>}`. The target is present when the command has one, and the version you read (`@v<n>`) when the command changes an item that has versions. An object param is written below the command, two spaces in, in the tree form of Ursatz Text; a command that takes a file ends with `file <path>`; `#` starts a comment line. The names, params and examples of every command are in the generated [command reference](../../workshop/docs/command-reference.md), `ursatz help commands` prints them, `ursatz help <command>` (a name with a dot, as `ursatz help item.save`) the help of one, and `ursatz help grammar` the grammar.

```
# add a project and a subject, then compose a piece
project.create name demo
subject.create name hero kind character
item.compose item:pce-2c8d54834a9c499f@v2 seed 1
```

A command in JSON is an object:

```json
{"command": "item.save", "target": "itm-3f2a", "base_version": 3, "params": {"note": "longer ending", "body": {}}, "file": "motif.mid"}
```

Only `command` is needed. `file` is a path whose bytes go with the command (`library.import`), named by its base name. An `actor` member is refused: the actor comes from `--actor`. All the commands are checked, and every file they name is read, before the first one runs: a mistake in the input applies nothing; in a text it names its line (`line 2: project.create takes no param 'nme'`).

Each command prints its number, name, target (`-` for none) and the journal sequence it wrote, then `v<n>` when the reply carries a version (a saved version, a restore) and `job <id>` when it started a job; with `--wait` the job is waited for and its state follows, with the version it made (a failed job prints its error instead). The first command the workshop refuses stops the run; the ones before it stay applied. `--format json` prints each reply as one JSON line.

## compose, regenerate and render of an item

```sh
ursatz compose item:<id> [--seed <n>] [--update-uses] [--base <n>] [--no-wait]
ursatz regenerate item:<id> --section <id> [--lane <lane>] [--seed <n>] [--version <n>] [--base <n>] [--no-wait]
ursatz render item:<id>[@v<n>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi]
```

`compose` composes a theme or piece into its next version, `regenerate` writes one section of a piece version again as the next version. Both start a job, wait for it and print `item <id> version <n>`; a failed job prints `Error: <its error>` and exits 2; `--no-wait` prints the job's id. The command is made against `--base` (default: the item's current version): a newer version refuses it. `--update-uses` moves every updated use of a theme to its source's current version first. `regenerate` takes the section of `--version` (default: the current version).

`render` renders the `plan.json` of a version (the current one when the address has none) to MIDI or MusicXML as `render <plan.json>` does; a motif has no plan (exit 2). `regenerate <plan.json>` and `render <plan.json>` stay the forms for a file.

## rate and explore

`ursatz --data <dir> rate <ses-id> --round <n> --seed <s> [--rating <batch>=<1-5>]... [--better <batch>|same] [--note <text>]` records the ratings of the samples of a listening session and a pick between its batches (`ratings.rate`, `ratings.pick`, [ratings](../../workshop/docs/ratings.md)); `rate item:<id>@v<n> --rating <1-5>` rates a version (`ratings.rate-version`). It prints one line for each id made: `rat-... <batch> <rating>`, `pck-... better <batch>`, `rat-... v<n> <rating>`. A request the workshop refuses exits 1, any other failure 2. A value is kept as a taste default with `ratings.keep`, which cites those ids (written as an apply command).

`ursatz --data <dir> critique item:<id>[@v<n>] [--settings <file>] [--note <text>]` has the critic judge a version of a motif or a theme (the current one without `@v<n>`) and keeps the judgment (`ratings.critique`, [ratings](../../workshop/docs/ratings.md#the-critics-judgments-r-723)): `crt-... score <n/d>`, then one line for each observation, `obs-...  bar <b>  <rule>  <message>`. `critique --verdict <obs-id> agree|disagree item:<id> [--note <text>]` records your verdict on one (`ratings.verdict`) and prints `vrd-...`. A request the workshop refuses (a stale version, an observation the item does not hold) exits 2 for a conflict or a missing id and 1 for a bad value. [cli-critique.md](cli-critique.md) has both forms.

`ursatz --data <dir> explore item:<id> [--distance <n>] [--seed <n>] [--taste <name>] [--base <n>] [--no-wait]` composes a theme as `compose` does with some settings moved away from the taste (`item.explore`, [settings](../../workshop/docs/settings.md#composing-with-settings-and-exploring)); it waits for the job and prints `item <id> version <n>`, or the job's id with `--no-wait`. A piece or an item that is not there is refused (2 for an item that is not there).

## restore

`ursatz restore item:<id>@v<n> [--base <n>] [--note <text>]` makes the next version of the item a copy of version n: its files, its body (the paths of its result in the new folder) and its uses (the command `item.restore`). Nothing is rewritten, so restoring is an undo that keeps the history: it prints `item <id> version <new> (restored v<n>)`. The note of the new version is `restore v<n>` unless `--note` gives one; `--base` is the version the command is made against (default: the current one). The current version cannot be restored, and a version that is not there is not found. The GUI's versions list has the same action.

## migrate

`ursatz migrate [--dry-run] [--backup <dir>]` migrates a data folder of an older layout to the current one (layout 2); the GUI backend no longer does. `--dry-run` writes nothing and reports every planned move and rewrite. A real run copies the whole folder to `--backup` first (default: beside it, `<folder>-backup-<time>`), migrates and prints the report as JSON. The folder must not be open: while a process holds it, the command exits 2 with `stop the Ursatz process <pid> first`. A folder in an older layout is refused by every other command with the two commands to run (`migrate --dry-run`, then `migrate`). See [older folders](../../workshop/docs/data-folder.md#older-folders-and-migration).
