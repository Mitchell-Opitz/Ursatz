# ursatz docs

The program `ursatz`: one command line for Claude sessions and scripts, over the [workshop](../../workshop/docs/README.md). It composes, analyzes and renders music, and it reads and changes a data folder. Back to the [part README](../README.md).

```sh
ursatz <command> [options]
ursatz help <command>
ursatz --version
```

`ursatz` alone lists the commands; `ursatz help <command>` and `ursatz <command> --help` print the help of one command, made from the command table and the flag table in `cli/src` (the usage line, what the command does, the notes on its options, the exit status, an example, then one line a flag). Every flag has one meaning in every command that takes it: the [flag table](flags.md) lists them.

## Commands

| Command | What it does | Page |
| --- | --- | --- |
| `progression` | a chord progression with a bass line for one phrase | [cli-progression.md](cli-progression.md) |
| `motive` | analyzes a motive, renders it in any key | [cli-motive.md](cli-motive.md) |
| `develop` | motives related to a motive by one operation | [cli-develop.md](cli-develop.md) |
| `variants` | variants of a motive from chains of operations | [cli-variants.md](cli-variants.md) |
| `recognize` | names the operations that turn one motive into another | [cli-recognize.md](cli-recognize.md) |
| `random-motive` | random motives that keep the hard melodic rules | [cli-random-motive.md](cli-random-motive.md) |
| `counterpoint` | a counter-melody against a line | [cli-counterpoint.md](cli-counterpoint.md) |
| `grow` | grows a motive into a sentence, a period or a hybrid theme | [cli-grow.md](cli-grow.md) |
| `theme` | grows a theme from the motives of a request | [cli-theme.md](cli-theme.md) |
| `piece` | composes a looping game track from a request | [cli-piece.md](cli-piece.md) |
| `plan`, `fill` | the plan of a short piece from a form template, and its realization | [cli-plan.md](cli-plan.md), [cli-fill.md](cli-fill.md) |
| `lock`, `regenerate` | makes material of a plan Given; writes one node again | [cli-regenerate.md](cli-regenerate.md) |
| `variations`, `harmonize` | several versions of one node of a plan | [cli-variations.md](cli-variations.md) |
| `render` | renders a plan to MIDI or MusicXML | [export.md](../../composer/docs/export.md) |
| `listen` | batches of motifs or themes from fixed seeds for listening | [cli-listen.md](cli-listen.md) |
| `analyze` | the analysis document of a MIDI or MusicXML file, or its text export | [analyze.md](analyze.md) |
| `corpus`, `style-profile` | analyzes a folder of pieces; counts its style | [corpus.md](corpus.md) |
| `status`, `query`, `show` (in text views), `history`, `apply` (text commands), `restore`, `migrate`, `rate`, `explore`; `compose`, `regenerate` and `render` of an item | the data folder | [data-folder.md](data-folder.md) |

[workflows.md](workflows.md) has the commands in order: a piece from a plan, the middle-out start, the Analyzer's check, and a piece from a corpus. [changelog.md](changelog.md) lists the changes of this part.

## Global flags

Three flags stand before the subcommand and name the data folder the command works on:

```sh
ursatz --data ~/ursatz-data --actor claude status
```

| Flag | Meaning |
| --- | --- |
| `--data <dir>` | the data folder (or the environment variable `URSATZ_DATA`) |
| `--port <n>` | the HTTP port of the workshop, 1 to 65535 |
| `--actor <name>` | who the journal records a command as; default `claude` |

The commands that work on files (`motive`, `grow`, `plan`, ...) need none of them.

## Connect or run

A data folder belongs to one process at a time: it is locked with `.ursatz.lock`. A data folder subcommand opens the workshop on the folder, runs, and lets go. When another process holds the folder and serves HTTP (the GUI backend writes its port into the lock file), the subcommand connects to it and makes the same calls over `/api/workshop/...` ([routes](../../gui/docs/api/workshop-routes.md)); nothing differs for the command. When the holder serves no HTTP, the command fails, names the process and says to run again when it ends. `ursatz status` says which of the two happened. The tools (`composer`, `analyze`, ...) run in the command's own process either way.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | success |
| 1 | usage, an invalid argument, an input that cannot be read, or a request the workshop refuses |
| 2 | the command could not be done: nothing composed, a folder that cannot be opened or reached, a job that failed |
| 3 | an output file could not be written (the commands that write files) |

Each command's help names what 1, 2 and 3 mean for it.

## Addresses

`show`, `history`, `compose`, `regenerate` and `render` take an address in place of a file name: `<kind>:<id>` with the kind `item`, `piece`, `project`, `subject`, `collection`, `style`, `links` or `job`, and `item:<id>@v<n>` for one version of an item. A text is an address when it starts with a known kind and a colon; any other text is a path. See [data-folder.md](data-folder.md).
