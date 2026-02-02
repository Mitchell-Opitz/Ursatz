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
- the index's `analyzer_version` and `document_schema_version` match this build;
- the entry's document file exists, and with `--markdown` so does the report.

Failed pieces are tried again. Pieces no longer in the folder are dropped from the index. An index from another
analyzer or schema version reuses nothing. A missing index is a normal first run; an index that exists but cannot be
read is an error (exit 2). Without `--resume` every piece is analyzed and the index is replaced.
