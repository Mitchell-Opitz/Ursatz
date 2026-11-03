# Corpus mode

```sh
ursatz-analyzer --corpus <dir> [--out <dir>] [--resume] [--markdown]
```

Library: `corpus_run` in `include/ursatz_analyzer/corpus.h`.

## Folder walk

- `<dir>` is walked recursively. Files with the extensions `.mid`, `.midi`, `.musicxml` and `.xml` (any case) are
  pieces; everything else is ignored. The input format is detected per file.
- Pieces are sorted by their path relative to `<dir>` (`strcmp` order), so a run is reproducible.
- The output directory is never walked. Symbolic links to directories are not followed.

## Outputs

`--out` defaults to `<dir>/.ursatz-corpus`. For a piece at the relative path `a/b.mid`:

- `<out>/a/b.mid.analysis.json`: the analysis document, byte for byte what `ursatz-analyzer --json` writes for that
  file. The whole file name is kept, so `b.mid` and `b.xml` do not collide.
- `<out>/a/b.mid.report.md`: the Markdown report, only with `--markdown`.
- `<out>/corpus-index.json`: the index, below.

A piece that cannot be read or analyzed is `failed` in the index, gets no document, and does not stop the run. The
CLI prints one line per piece (`analyzed|skipped|failed <path>[: error]`) and a summary line, and exits 0 when the run
completes, even with failed pieces. It exits 1 on a usage error and 2 when the run cannot proceed (missing input
directory, output directory that cannot be created, unreadable index on `--resume`, a file that cannot be written).

## Corpus index

`corpus-index.json` follows [schema/corpus-index.v1.schema.json](../schema/corpus-index.v1.schema.json).

| Field | Meaning |
| --- | --- |
| `schema`, `schema_version` | `"ursatz-corpus-index"`, `1`. |
| `analyzer_version` | The analyzer's project version. |
| `ursatz_commit` | The Ursatz commit the analyzer was built against (`ursatz_analyzer_ursatz_commit()`). Absent from indexes written before 0.4.0. |
| `document_schema_version` | The analysis document schema the documents follow. |
| `input_dir` | The directory as given on the command line. |
| `pieces[].path` | Input path relative to `input_dir`. |
| `pieces[].document` | Document path relative to the output directory; `null` when failed. |
| `pieces[].status` | `"analyzed"` or `"failed"`. A piece skipped on resume stays `"analyzed"`. |
| `pieces[].error` | Why a piece failed; otherwise `null`. |
| `pieces[].content_hash` | FNV-1a 64 of the input file's bytes, 16 lowercase hex characters. |
| `pieces[].measures` | The meter section's `measure_count`; `null` when failed. |
| `pieces[].home_key` | The home key label, for example `"G major"`; `null` when failed or none was found. |

The index is rewritten after every piece (to `corpus-index.json.tmp`, then renamed), so a killed run leaves a valid
index of the pieces done so far.

## Resume

With `--resume` the existing index is read and a piece is skipped when all of these hold:

- the index has an `analyzed` entry for its path with the same `content_hash`;
- the index's `analyzer_version`, `ursatz_commit` and `document_schema_version` match this build;
- the entry's document file exists, and with `--markdown` so does the report.

Failed pieces are tried again. Pieces no longer in the folder are dropped from the index. An index from another
analyzer version, Ursatz commit or schema version (or one with no `ursatz_commit`) reuses nothing. A missing index is a normal first run; an index that exists but cannot be
read is an error (exit 2). Without `--resume` every piece is analyzed and the index is replaced.

## Staleness keys

A stored analysis is current only when its schema version, the analyzer version that wrote it and the Ursatz commit
it was analyzed against all match the build that reads it. A document holds them as `schema_version`,
`metadata.analyzer_version` and `metadata.ursatz_commit`; the index holds the same keys for the whole run.

`<ursatz_analyzer/staleness.h>` gives the check (the first mismatch, in the order schema, analyzer, Ursatz, names the
status):

| Status | Meaning |
| --- | --- |
| `ANALYZER_DOCUMENT_CURRENT` | All three keys match. |
| `ANALYZER_DOCUMENT_STALE_SCHEMA` | Another document schema version. |
| `ANALYZER_DOCUMENT_STALE_ANALYZER` | Another analyzer version. |
| `ANALYZER_DOCUMENT_STALE_URSATZ` | Another Ursatz commit. |
| `ANALYZER_DOCUMENT_UNREADABLE` | Not JSON, or the keys are missing or have the wrong type. |

`analyzer_document_status(document_json)` reads `schema_version` and `metadata` of a document;
`analyzer_document_status_of(schema_version, analyzer_version, ursatz_commit)` compares keys the caller already holds.

Style profiles, similar-material search and motive links keep reading stale documents as they are. The commands
that read a corpus output (`--style-profile`, `--similar`, `--link-motives`) print the number of stale documents on
stderr; `--corpus --resume` re-analyzes every piece when the index is stale.
