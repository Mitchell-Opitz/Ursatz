# ursatz knowledge

Finds, shows or checks the knowledge entries (R-765, [workshop/docs/knowledge.md](../../workshop/docs/knowledge.md)). Back to the [docs index](README.md).

```
ursatz knowledge find <words> [--kind <kind>]
ursatz knowledge show <id> [--format text|json]
ursatz knowledge check <file>
```

The entries are the ones built into the program (materials, chord types, vocabulary, schemas, patterns, voicings, rules and the rest) and, with a data folder (`--data <dir>` before the subcommand, or `URSATZ_DATA`), the files of its `knowledge/` folder, whose entries start `user.`. Without a data folder only the built-in ones are read.

## find

One line for each entry whose id, name or an alias holds every word, case ignored, in id order, at most 50:

```
omt.schema.aeolian-cadence  schema/progression  Aeolian cadence
```

Quote several words (`find "dorian cadence"`). `--kind <kind>` keeps one kind (`schema`, `vocabulary`, `material`, `pattern`, ...); a kind no entry has is exit 1. No match prints nothing. When more than 50 match the last line says so.

## show

The entry as Ursatz Text (the default) or, with `--format json`, as loaded. An id no entry has is exit 1.

## check

Reads the knowledge document in `<file>` and builds the knowledge with it beside the built-in entries and the data folder's files, as the workshop does when it opens. It prints `ok: <n> entries`, or the loader's refusal (exit 2): the file, the entry id and the field. Check a file before it goes into the data folder's `knowledge/` folder; an id that folder already holds is a duplicate.

## Exit status

0 success; 1 usage, or an id or kind no entry has; 2 a document the loader refuses, a file that is not a JSON object, or any other failure.
