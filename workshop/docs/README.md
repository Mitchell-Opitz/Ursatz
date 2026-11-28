# Workshop docs

The workshop is the process that holds the data folder, the jobs and the events, and answers every query and command of the GUI and of the [`ursatz` command line](../../cli/docs/data-folder.md). Back to the [README](../../README.md).

## Start here

- [architecture.md](architecture.md): what the workshop is, how its source is laid out, its layers and its locks.

## The data

- [harmony-plans.md](harmony-plans.md): harmony plans as items (`hrm-`): the body, compose, files, references in a theme's chords and a section's harmony, `item.apply-harmony`.
- [settings.md](settings.md): the setting keys, the scope files, how a request's settings resolve and where each comes from, and the `settings` commands.
- [ratings.md](ratings.md): listening sessions with batches, ratings and picks, ratings of item versions, and keep as default.
- [data-folder.md](data-folder.md): the folder layout, every format with its version and schema, transactions and recovery, the migration with its dry run and backup, the settings.

## The calls

- [commands.md](commands.md): `workshop_query` and `workshop_command`, the 66 commands, the queries, base versions, actors and failures.
- [command-reference.md](command-reference.md): every command with its target, params and summary, generated from `workshop/commands/*.json`.
- [knowledge.md](knowledge.md): the knowledge queries: the palette of a key, find, show and check.
- [tools.md](tools.md): `workshop_tool`, the tools workshop with no data folder, and each tool's params, reply and failures.
- [jobs-and-events.md](jobs-and-events.md): the job queue, the job kinds, cancel, and the event bus with its events.

## Behind the GUI

- [variants.md](variants.md): motif analysis on save, the variant tree, comparing and linking motifs, motive forms and random motifs.

## Changes

- [changelog.md](changelog.md): what changed in the workshop, newest first.
