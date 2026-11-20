# cli changelog

Newest first. [Docs index](README.md).

## P65: Ursatz Text (2026-11)

- **Restore.** `ursatz restore item:<id>@v<n> [--base <n>] [--note <text>]` makes the next version a copy of an older one (R-667); `--note` joins the flag table (29 commands).
- **Text commands.** `ursatz apply` takes commands written in Ursatz Text (one command a line; JSON when the first character is `{` or `[`) and prints the version each makes. A text is read by the workshop's tool `commands-parse`, so a mistake names its line. The `--wait` of a command that starts a job works (a local `item.compose` stopped with "job not found").
- **Views.** `ursatz show <address|file>` prints the views of Ursatz Text (`--view summary`, `structure`, `harmony`, `line:<name>`, `part:<name>`, `reading`, `full`, `json`; `--measures <first>-<last>`); an item or a piece address takes `/<section>`. `analyze --view text` is `--view reading` and its text is framed as a view.
- **Generated help.** `ursatz help commands [--format text|markdown|json]`, `ursatz help grammar`, `ursatz help <command>` (a name with a dot) and `ursatz help flags [--format text|markdown]` print the command reference, the grammar, the help of one command and the flag table from the command schemas and the flag tables. `cli/docs/flags.md` is the generated flag table (it had missed `--accompaniment-format`, `--material` and `--value-scale`) and a test compares it.

## P64: one CLI (2026-11)

- **One program.** `ursatz` replaces `ursatz-composer`, `ursatz-analyzer` and the migration of `ursatz-gui-backend` (R-658). It is a thin program over the workshop: the Composer and the Analyzer run through the workshop's tools (`composer`, `analyze`, `corpus`, `style-profile`, `corpus-passages`, `pitch-names`), and the data folder through a session that opens the folder or connects to the process that holds it (R-659).
- **Commands.** The Composer's commands keep their names; `export` is `render`. New: `analyze` (with `--view text` for the text export), `corpus`, `style-profile`, `status`, `query`, `show`, `history`, `apply`, `compose`, `migrate`, and the item forms of `regenerate` and `render`. The Analyzer's `--similar`, `--compare-styles` and `--link-motives` modes are not commands.
- **Flags.** One flag table: each flag has one meaning in every command. `--json` is `--format json` everywhere; the format of a motive input is `--input-format`; `--material` has the alias `--mode`; `--write-midi` and `--write-musicxml` are the switches of `variations` and `harmonize`. The global flags `--data`, `--port` and `--actor` stand before the subcommand. See [flags.md](flags.md).
- **Help.** Printed from the command and flag tables: `ursatz`, `ursatz help <command>`, `ursatz <command> --help`. A parse failure says `<command> takes <n> input(s)`.
- **Moved.** The command pages come from `composer/docs/cli-*.md`, rewritten for `ursatz`; `composer/docs/cli.md` is [README.md](README.md) and [workflows.md](workflows.md). New pages: [analyze.md](analyze.md), [corpus.md](corpus.md), [data-folder.md](data-folder.md), [flags.md](flags.md).
