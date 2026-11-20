# corpus and style-profile commands

The two commands that turn a folder of pieces into a corpus of analysis documents and a style model. Back to the [cli docs](README.md).

## corpus

```sh
ursatz corpus <dir> [--out <dir>] [--resume]
```

Analyzes every MIDI and MusicXML file of `<dir>` (found recursively, in path order) with the Ursatz Analyzer and writes one analysis document for each, and the corpus index, into the output folder: `<dir>/.ursatz-corpus` unless `--out` names another. A piece that cannot be read or analyzed is counted as failed; that does not fail the run. Each piece is printed as analyzed, skipped or failed with its path (and, for a failed one, why), the importer's messages and warnings go to stderr, then the three counts.

With `--resume` a piece whose file did not change since the last run is skipped. See [corpus.md](../../analyzer/docs/corpus.md) and the [index schema](../../analyzer/schema/corpus-index.v2.schema.json).

Exit status: 0 the run finished (also with failed pieces); 1 usage or an unknown option; 2 the run could not be made (a folder that does not read, an output that cannot be written).

```sh
ursatz corpus sonatinas --out sonatinas-analyzed
ursatz corpus sonatinas --out sonatinas-analyzed --resume
```

## style-profile

```sh
ursatz style-profile <corpus output> --name <name> [--out <file>]
```

Reads the output folder of a corpus run (its `corpus-index.json` and the documents) and writes a style model: the harmony, rhythm, melody and form the corpus uses, counted, with four categories for motive degrees, intervals, rhythms and non-chord-tone figures. The model is written to `--out`, by default `<corpus output>/style-model.json`, under the name given. The style commands (`grow`, `plan`, `fill`, `listen`, `random-motive`) read it with `--style`. A document another version of the Analyzer wrote is used as it is, and a warning says how many there are. See [style-profile.md](../../analyzer/docs/style-profile.md) and the [model schema](../../analyzer/schema/style-model.v1.schema.json).

Exit status: 0 success; 1 usage (the name is required); 2 the corpus cannot be read or the model cannot be written.

```sh
ursatz style-profile sonatinas-analyzed --name Sonatinas --out sonatinas.style.json
```

The comparison of two style models, the search for similar passages and the motive links are not commands: the GUI and the workshop's API call them ([similar-material.md](../../analyzer/docs/similar-material.md), [motive-links.md](../../analyzer/docs/motive-links.md)).
