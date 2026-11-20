# The data folder

Where the workshop keeps what a person made, which format each file has and how an older folder is brought forward. [Docs index](README.md).

## Layout 2

The folder is a plain directory; `ursatz-gui-backend --data <dir>` (or `URSATZ_DATA`, or `data_dir` of the settings) names it, and the process runs inside it. `data-folder.json` (format `ursatz-data-folder`, `format_version` 2 is the layout) marks a folder the workshop may open. The path of every file is built in `workshop/src/items/layout.c`.

```
data-folder.json                                   the layout marker, written last by a migration
ursatz.json                                        the settings (optional, format ursatz-config 1)
.ursatz.lock                                       held with flock while a workshop is open
journal/journal.jsonl                              one record per committed change (R-643)
projects/<prj>/project.json
projects/<prj>/items/<id>/item.json                header: id, kind, name, project, current version, journal_seq, times
projects/<prj>/items/<id>/op-checks.json           a motif's operation marks
projects/<prj>/items/<id>/v<n>/envelope.json       what the version is (written last: a version exists once it has one)
projects/<prj>/items/<id>/v<n>/body.json           the body; a composed version also holds request.json, plan.json, MIDI, MusicXML,
                                                   layers (realization.json, readings/composer.json) and variations-<k>/
subjects/<sub>.json
library/pieces/<id>/                               source.<ext>, piece.json, analysis.json, realization.json, readings/analyzer.json
library/collections/<id>.json
library/links/<lnk>/run.json
settings/styles/<sty>/                             style-model.json, style.json
cache/                                             motive-link-cache.json, renders/<item>/v<n>/, index/: rebuilt, never the source of truth
```

An id is 1 to 64 letters, digits, hyphens or underscores. Subjects stay global: one subject links items of any project and Library pieces.

## Formats

Every file the workshop owns has `format` and `format_version`. A reader refuses another format, a version it does not know, or a file that lacks a member its format requires (`URSATZ_ERR_FORMAT`, naming the file). There is one reader and one writer (`items/formats.c`); each format has a version constant in `workshop_api.h` and a JSON schema in `workshop/schema/` (the earlier versions stay there as history).

| Format | Version | File | Schema |
|---|---|---|---|
| `ursatz-data-folder` | 2 | `data-folder.json` | `ursatz-data-folder.v2` |
| `ursatz-config` | 1 | `ursatz.json` | `ursatz-config.v1` |
| `ursatz-project` | 1 | `project.json` | `ursatz-project.v1` |
| `ursatz-item` | 1 | `item.json` | `ursatz-item.v1` |
| `ursatz-item-version` | 1 | `envelope.json` | `ursatz-item-version.v1` |
| `ursatz-motif-body` | 1 | a motif's `body.json` | `ursatz-motif-body.v1` |
| `ursatz-theme-body` | 2 | a theme's `body.json` | `ursatz-theme-body.v2` |
| `ursatz-piece-body` | 2 | a piece's `body.json` | `ursatz-piece-body.v2` |
| `ursatz-op-checks` | 1 | `op-checks.json` | `ursatz-op-checks.v1` |
| `ursatz-subject` | 1 | `subjects/<sub>.json` | `ursatz-subject.v1` |
| `ursatz-library-piece` | 1 | `piece.json` | `ursatz-library-piece.v1` |
| `ursatz-collection` | 1 | `library/collections/<id>.json` | `ursatz-collection.v1` |
| `ursatz-style` | 1 | `style.json` | `ursatz-style.v1` |
| `ursatz-link-run` | 1 | `run.json` | `ursatz-link-run.v1` |
| `ursatz-render` | 1 | `render.json` | `ursatz-render.v1` |
| `ursatz-journal` | 1 | the first line of `journal.jsonl` | none (lines, not one document) |

Files other parts own keep their own versions: the analysis document (`analysis.json`), the style model, the motive link cache and a composed version's `request.json`.

## Items and versions

A project holds items (motifs, themes, pieces). An item has numbered versions; saving or composing adds one, nothing rewrites an old one. The header holds no versions: they are read from the envelopes. A theme or piece body (format 2) is `{uses, request, brief, result, check, seed}`: `request` is the Composer request as it is sent, with `{"$item": {"item_id", "version"[, "file"]}}` in place of a motif's motive, a theme fill's plan or the accompaniment motif, and `{"$pool": {"item_id", "pool"}}` in place of a motif's saved forms; `uses` copies what the references name; `check` and `seed` are run options beside the request. A placeholder body is `request: {}`. The store checks only what it owns (the shape, ids, uses and their versions, derived records and lineage, notes, byte caps): a draft may hold any word, and the Composer judges the request at validate and compose (R-649). A resolver (`composition/request_resolve.c`) replaces each reference before the Composer is called.

A composed version keeps the files of the compose beside its body: the request the Composer read, the plan, the MIDI and MusicXML, the layers (`realization.json` and `readings/composer.json`, R-644) and each variation set in `variations-<k>/`. A version or piece made before the layers existed has none; its answers say `layers: null`.

## The Library

A piece is `library/pieces/<id>/`: the uploaded file as `source.<ext>`, `piece.json` (filename, composer, source, tags, notation override, `source_format`, `producer_key`, upload time), `analysis.json` (the analysis document), and the layers of the analysis (`realization.json`, `readings/analyzer.json`). An analysis is stale when its producer key or document schema is not the running Analyzer's; a query then answers what is stored with `stale: true` and the page starts a re-analysis job (queries never write). A collection is an ordered list of piece ids; a style is a folder with its model and its description; a link run is `run.json` (scope, status, progress, result).

## Changes are transactions

Every command that changes a file runs as one transaction of `items/txn.c`, committed through the journal (R-643):

1. each new file or folder is written beside its final place under a `.pending-<seq>-` name and read back with its format's reader;
2. the record is appended to `journal/journal.jsonl` and fsynced (the commit): sequence, command, actor, time, target, base version, the pending and final paths, the removals and the item headers it sets;
3. the pending entries are renamed into place, the headers rewritten, the indexes updated; removals were renamed to `.removed-<seq>-` before the commit and are deleted after it.

At open, `workshop_recover` finishes the entries of committed records, deletes or restores those of uncommitted ones (a last line without its newline is uncommitted), rewrites headers older than their last record, deletes `*.tmp-<pid>` and `*.stage-*` leftovers and rebuilds the indexes; `workshop_recovery_summary` is its one line. Plain files (settings, progress writes) are written with `common_file_write_atomic` (temporary file, fsync, rename, fsync of the folder). A test build (`URSATZ_WORKSHOP_TEST_HOOKS`) can stop the process at each step; `workshop_crash_points` runs every step of every command kind it names and checks that the old or the new state comes back.

One process holds a folder: `.ursatz.lock` is taken with `flock`, and a second open fails with `URSATZ_ERR_CONFLICT` naming the holder's pid.

The lock file has two forms (R-659): `<pid>\n` while the process serves nothing, and `<pid> <port>\n` once it announced the HTTP port it serves (`workshop_announce`; the GUI backend does when its server is up). A program that finds the folder in use asks `workshop_lock_holder`: it answers the holder's pid and port (0 for none) when another open file holds the lock, `URSATZ_ERR_NOT_FOUND` when nobody does, and never keeps the lock. The command line connects to the port; with none it says the folder takes no connections.

## Older folders and migration

A folder from before the workshop (layout 0: `pieces/`, `materials/`, `subjects/`, `collections/`, `styles/`, `links/`) and a layout 1 folder are refused when opened, with the commands to run:

```
ursatz-gui-backend --data <dir> --migrate --dry-run          # reads, writes nothing, prints every planned move and rewrite
ursatz-gui-backend --data <dir> --migrate [--backup <dir>]   # the same, for real
```

A real run first runs the read-only data check (a folder the one-time migrations of the old GUI never ran on is refused with the check's message), copies the whole folder to the backup (`<dir>-backup-<UTC time>` beside it unless `--backup` names one) and compares file count and bytes, builds the new layout in `.migrate-1/`, renames a piece's `ursatz_commit` to `producer_key`, moves the motif render caches to `cache/renders` (rendered again once), rewrites every body in its current format, moves the old top-level entries to `.pre-1/` and the new ones into place, and writes `data-folder.json` last. A run that stopped before the last moves leaves `.migrate-1/` without its `.complete` marker and starts again; one that stopped during them is finished from where it stopped. A layout 1 folder takes the same backup, then each theme and piece body of format 1 is rewritten in place as format 2 and the marker last; a body already in format 2 is left, so an interrupted run is run again. Delete `.pre-1/` once the GUI works. The report is JSON (`from_layout`, `to_layout`, counts, `moves`). Readers of the old layouts live only in the migration (`items/migrate.c`, `body_migrate.c`, `body_requests.c`); the test `workshop_migrate` runs it on `workshop/tests/fixtures/data-v0` (every kind of item and body the GUI ever stored) and `migrated_routes` plays the GUI over the migrated copy.

## Settings

`workshop_settings_resolve` takes four layers, each over the one before: the defaults (`data_dir` `.`, `port` 8765, `http_threads` 16, `job_workers` 2), `ursatz.json` (format `ursatz-config` 1: `port` 1 to 65535, `http_threads` 1 to 64, `job_workers` 1 to 8; a bad file is `URSATZ_ERR_FORMAT` naming the member), the environment (`URSATZ_DATA`, `URSATZ_PORT`, `URSATZ_JOB_WORKERS`) and the flags (`--data`, `--port`).
