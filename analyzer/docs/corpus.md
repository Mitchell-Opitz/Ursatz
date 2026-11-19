# Corpus mode

Corpus mode, which analyzes every piece in a folder into documents and a corpus index. Back to the [docs index](README.md).

```sh
ursatz-analyzer --corpus <dir> [--out <dir>] [--resume]
```

Library: `corpus_run` in `src/include/ursatz_analyzer_internal/corpus.h`.

## Folder walk

- `<dir>` is walked recursively. Files with the extensions `.mid`, `.midi`, `.musicxml` and `.xml` (any case) are
  pieces; everything else is ignored. The input format is detected per file.
- Pieces are sorted by their path relative to `<dir>` (`strcmp` order), so a run is reproducible.
- The output directory is never walked. Symbolic links to directories are not followed.

## Outputs

`--out` defaults to `<dir>/.ursatz-corpus`. For a piece at the relative path `a/b.mid`:

- `<out>/a/b.mid.analysis.json`: the analysis document, byte for byte what `ursatz-analyzer --json` writes for that
  file. The whole file name is kept, so `b.mid` and `b.xml` do not collide.
- `<out>/corpus-index.json`: the index, below.

A piece that cannot be read or analyzed is `failed` in the index, gets no document, and does not stop the run. The
CLI prints one line per piece (`analyzed|skipped|failed <path>[: error]`) and a summary line, and exits 0 when the run
completes, even with failed pieces. It exits 1 on a usage error and 2 when the run cannot proceed (missing input
directory, output directory that cannot be created, unreadable index on `--resume`, a file that cannot be written).

## Corpus index

`corpus-index.json` follows [schema/corpus-index.v2.schema.json](../schema/corpus-index.v2.schema.json).

| Field | Meaning |
| --- | --- |
| `schema`, `schema_version` | `"ursatz-corpus-index"`, `2`. |
| `analyzer_version` | The analyzer's project version. |
| `producer_key` | The producer key of the build that wrote the index (`analyzer_producer_key()`). An index of schema version 1 holds it as `ursatz_commit`. |
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
- the index's `analyzer_version`, `producer_key` and `document_schema_version` match this build;
- the entry's document file exists.

Failed pieces are tried again. Pieces no longer in the folder are dropped from the index. An index from another
analyzer version, producer key or schema version (an index of schema version 1 has `ursatz_commit` instead of `producer_key`) reuses nothing and is written again. A missing index is a normal first run; an index that exists but cannot be
read is an error (exit 2). Without `--resume` every piece is analyzed and the index is replaced.

## Staleness keys

A stored analysis is current only when its schema version, the analyzer version that wrote it and the producer key
it was analyzed against all match the build that reads it. A document holds them as `schema_version`,
`metadata.analyzer_version` and `metadata.producer_key`; the index holds the same keys for the whole run.

`<ursatz_analyzer/analyzer_api.h>` gives the check (the first mismatch, in the order schema, analyzer, Ursatz, names the
status):

| Status | Meaning |
| --- | --- |
| `ANALYZER_DOCUMENT_CURRENT` | All three keys match. |
| `ANALYZER_DOCUMENT_STALE_SCHEMA` | Another document schema version (a document of version 11 or older, with a `metadata.analyzer_version` string, reads stale whatever else it holds). |
| `ANALYZER_DOCUMENT_STALE_ANALYZER` | Another analyzer version. |
| `ANALYZER_DOCUMENT_STALE_URSATZ` | Another producer key. |
| `ANALYZER_DOCUMENT_UNREADABLE` | Not JSON, or the keys are missing or have the wrong type. |

`analyzer_document_status(document_json)` reads `schema_version` and `metadata` of a document;
`analyzer_document_status_of(schema_version, analyzer_version, producer_key)` compares keys the caller already holds.

What the readers do with another schema version: the GUI and the Composer re-analyze or ignore a stale document, and `--corpus --resume` re-analyzes a stale index. Style profiles, similar-material search and motive links keep reading stale documents as they are (the front ends refuse a corpus output of an older document schema, P64). The commands
that read a corpus output (`--style-profile`, `--similar`, `--link-motives`) print the number of stale documents on
stderr; `--corpus --resume` re-analyzes every piece when the index is stale.
