# Analyzer architecture

How `ursatz-analyzer` is put together: what it is for, what it calls, what it offers and how one file becomes an analysis document. Back to the [docs index](README.md).

## Purpose

- `ursatz-analyzer` turns a MIDI or MusicXML file into an analysis: a versioned JSON analysis document.
- It is a thin consumer of the Ursatz library: it imports the file, runs the library's analysis chain once, collects the claims and exports them.
- It holds no music theory of its own; every rule it applies is the library's.
- It also runs a corpus, style profiles, similar material search and motive links over analysis documents.

## Where it sits

- Below: the [Ursatz library](../../library/docs/architecture.md) in `library/`, built by the same root. the layer table (`cmake/layer-table.txt`, test `layer_table`) allows only its staged public headers (see [building.md](building.md#library-include-check)).
- Beside: the [Composer](../../composer/docs/architecture.md) is a sibling on the library (R-620): it links no Analyzer target and hears through `library/chain`. The CLIs and the [GUI](../../gui/docs/architecture.md) are above both: the Composer CLI finds corpus passages with `corpus_passages.h` and gives them to the Composer; the GUI calls the playback and score export entries and reads the analysis document.

## Module map

| Directory | What it holds | Doc |
| --- | --- | --- |
| `include/ursatz_analyzer/` | The one API header, `<ursatz_analyzer/analyzer_api.h>`; the internal headers are in `src/include/ursatz_analyzer_internal/` | [library.md](library.md) |
| `src/util/` | Private JSON, file and misc helpers; the bottom of the stack | [library.md](library.md) |
| `src/store/` | The corpus walk, paths and the stale check (`corpus_walk`, `path_util`, `staleness`) | [corpus.md](corpus.md) |
| `src/result/` | The analysis result (`analyzer_analyze`), the file entry (`analyzer_run_file`) and the thin import entries | [analysis-result.md](analysis-result.md) |
| `src/export/` | The analysis document (an export of the reading, built by area in `document_*.c`) and its text, material, score and playback exports | [analysis-document.md](analysis-document.md), [text-export.md](text-export.md) |
| `src/style/` | Style profiles read from documents and the corpus | [style-profile.md](style-profile.md) |
| `src/corpus/` | The corpus run, similar material, the chord sequence and the corpus passages | [corpus.md](corpus.md), [similar-material.md](similar-material.md), [corpus-passages.md](corpus-passages.md) |
| `src/links/` | Motive links, their cache and their JSON | [motive-links.md](motive-links.md) |
| `src/cli/` | The `ursatz-analyzer` command (`main.c`) | [README](../README.md#usage) |

The folders are modules of `cmake/layer-table.txt` (test `layer_table`): `util` includes nothing of the Analyzer, `store` only `util`, `result` `util` and `store`, `export`, `style`, `corpus` and `links` those three, and `cli` all of them. The one edge into another folder's `*_internal.h` is `corpus.c` reading the document builder's entry, named in the table.

| `schema/` | JSON Schemas of the document and the other JSON files | [analysis-document.md](analysis-document.md) |
| `../tools/ci/` (repository root) | Include, chain build, file scope and docs checks, shared by every part | [building.md](building.md) |
| `tests/` | Unit, snapshot and CLI tests | [building.md](building.md#tests) |
| `docs/analyses/` | What each analysis reads from the library and how | [harmony](analyses/harmony.md), [form and motive](analyses/form-and-motive.md), [rhythm, texture and voice leading](analyses/rhythm-texture-voice-leading.md) |

## Public API

The CLI commands, from [the README](../README.md#usage):

| Command | What it does | Doc |
| --- | --- | --- |
| `ursatz-analyzer <file>` | Writes `<stem>.analysis.json` | [analysis-document.md](analysis-document.md) |
| `export <file>` | Prints the measure-addressed text export of a piece | [text-export.md](text-export.md) |
| `--corpus <dir>` | Analyzes a folder into documents and a corpus index | [corpus.md](corpus.md) |
| `--style-profile`, `--compare-styles` | Counts and compares styles | [style-profile.md](style-profile.md) |
| `--similar <dir>` | Finds similar material by motive or progression | [similar-material.md](similar-material.md) |
| `--link-motives <dir>` | Links the motive families across pieces | [motive-links.md](motive-links.md) |

The GUI entries:

| Header | Entry | What it does | Doc |
| --- | --- | --- | --- |
| `input_pipeline.h` | `analyzer_import_file`, `analyzer_import_buffer` | Thin entries over the library import (`io/imported_score.h`): a file or bytes into an `ImportedScore`, with its tempo map and the failure message | [gui-api.md](gui-api.md#import-with-tempo) |
| `playback.h` | `analyzer_playback_build`, `analyzer_playback_to_json` | Tempo regions and notes for a player | [gui-api.md](gui-api.md#playback-timing) |
| `score_export.h` | `analyzer_score_export_musicxml`, `analyzer_score_note_xml_ids`, `analyzer_imported_meter_defaulted` | MusicXML score of the import, note ids, whether the meter was inferred | [gui-api.md](gui-api.md#score-export) |

The run and corpus entries the workshop's tools call (`analyze`, `corpus`, `style-profile`; R-656): `analyzer_run_file` (import, optional reading, analysis; `AnalyzerRunFileResult`) and `analyzer_run_file_document`, `analyzer_document_path_for`, `analyzer_corpus_run` (`AnalyzerCorpusOptions`, `AnalyzerCorpusPieceReport`), `analyzer_corpus_stale_count` and `analyzer_style_profile_add_corpus`.

The corpus passage entries a caller gives to the Composer ([corpus-passages.md](corpus-passages.md)): `analyzer_corpus_passages_like` and `analyzer_corpus_passages_numerals` find the passages of a corpus, `analyzer_corpus_passages_to_json` writes the Composer API's `corpus_passages`.

The other public headers (analysis result, report, document, corpus, staleness, chord sequence, similar material, motive links, style profile) are listed in [library.md](library.md#library).

## Version constants and schemas

| Name | File | Current value | Doc |
| --- | --- | --- | --- |
| `URSATZ_ANALYZER_VERSION`, `analyzer_version()` | `<ursatz_analyzer_internal/version.h>`, generated from `src/version.h.in` at configure time | 0.7.0 (from `project()` in `CMakeLists.txt`) | [building.md](building.md#version-header-and-producer-key) |
| `analyzer_producer_key()` | `<ursatz_analyzer_internal/version.h>`, the key from the generated `producer_key.h` | a hash of the sources that produce an analysis, 16 hex digits | [building.md](building.md#version-header-and-producer-key) |
| `ANALYZER_TEXT_EXPORT_VERSION` | `include/ursatz_analyzer/analyzer_api.h` | 2 | [text-export.md](text-export.md) |
| `ANALYZER_DOCUMENT_SCHEMA_VERSION` | `include/ursatz_analyzer/analyzer_api.h` | 13 | [versions.md](analysis-document/versions.md) |
| `ANALYZER_CORPUS_INDEX_VERSION`, `ANALYZER_LINK_CACHE_VERSION` | `include/ursatz_analyzer/analyzer_api.h` | 2 each | [corpus.md](corpus.md) |
| `ANALYZER_MOTIVE_LINKS_VERSION`, `ANALYZER_SIMILAR_VERSION` | `include/ursatz_analyzer/analyzer_api.h` | 1 each | [motive-links.md](motive-links.md), [similar-material.md](similar-material.md) |
| analysis document schema | `schema/analysis-document.v13.schema.json` | v13 | [analysis-document.md](analysis-document.md) |
| corpus index, motive link cache schemas | `schema/corpus-index.v2.schema.json`, `schema/motive-link-cache.v2.schema.json` | v2 each |
| motive links, similar material, style model schemas | `schema/*.v1.schema.json` | v1 each | [corpus.md](corpus.md), [motive-links.md](motive-links.md), [similar-material.md](similar-material.md), [style-profile.md](style-profile.md) |

A stored document is current only when its schema version, analyzer version and producer key all match the build ([corpus.md](corpus.md)).

## What it calls below

The Ursatz headers the Analyzer includes, checked by the layer table, which allows only staged public paths. All come from the [library](../../library/docs/architecture.md).

| Entry used | From which repo | For what |
| --- | --- | --- |
| `chain/analysis_chain.h`, `chain/chain_inputs.h` (`analysis_chain_run` and the `analysis_chain_*` accessors) | Ursatz | The one analysis run and its results; the public headers reach them only through `result_chain.h` |
| `io/midi_importer.h`, `io/midi_key_signature.h`, `io/musicxml_importer.h` | Ursatz | Reading MIDI and MusicXML files |
| `core/`, `time/`, `pitch/`, `events/`, `identity/` | Ursatz | Rationals, meters, tempo maps, pitches, notes, ids |
| `interpretation/`, `semantics/`, `structure/`, `containers/`, `relationship/` | Ursatz | The chain's `Interpretation`s and the kernel vocabulary |
| `common-practice-minimal/`, `standard-tonality/` | Ursatz | Key regions, weighted chord identification, Roman numerals |
| `standard-harmony/analysis/` | Ursatz | Cadences, functions, six-fours, prolongations, the phrase model |
| `standard-form/analysis/`, `analysis/` (form structure, thematic returns, motive link and recognition, voice separation, outer voices, progression patterns) | Ursatz | Phrases, themes, the form plan, motives, motive links, voices |
| `standard-meter/analysis/`, `standard-texture/`, `standard-counterpoint/analysis/`, `standard-voicing/`, `standard-markings/` | Ursatz | Rhythm, texture, melody, voice leading, doublings, markings |
| `galant-schemas/`, `scale-harmonization/` | Ursatz | Schemas and sequences |
| `theory/`, `query/`, `export/` | Ursatz | Style features and models, similarity scoring, MusicXML score export |

The theory the Analyzer takes from the library is listed in [building.md](building.md#theory-the-analyzer-takes-from-the-library).

## Data flow

One file, `piece.mid` or `piece.musicxml`, through `ursatz-analyzer piece.mid`:

1. `main.c` reads the flags and calls `input_pipeline` ([input.md](input.md)).
2. `analyzer_run_file` imports the file with the library (`imported_score_import_file`, `io/imported_score.h`, R-614) into an `ImportedScore`: notes, a `MeterMap`, key signature events, the tempo map and an `ImportedSource` ([input.md](input.md#input-formats)). It prints the import's error or warnings, as the command line always did. Notation readings (`--meter`, `--value-scale`, `--pickup`, `imported_reading_apply`) are applied here.
3. `analyzer_analyze` calls the library's `analysis_chain_run_score` (R-615), which builds the chain's neutral input from the score ([library.md](library.md#pipeline)).
4. The chain runs once over every stage: segments, key regions, chords, Roman numerals, outer voices, cadences, form, motives, meter, texture, voice leading, melody, schemas and sequences.
5. The collectors of `library/chain` (`chain_result_<area>.c`) read the chain's accessors into the `ChainResult`; the library calls the chain does not run (functions, six-fours, prolongations, phrase model) are made over its prepared harmony segments. See [analyses/harmony.md](analyses/harmony.md).
6. The Analyzer-only stages read the collected report: progressions, harmonic rhythm, form plan, meter check, rhythm, markings, voicing check and pivot chords. See [analyses/form-and-motive.md](analyses/form-and-motive.md) and [analyses/rhythm-texture-voice-leading.md](analyses/rhythm-texture-voice-leading.md).
7. The result is an `AnalyzerResult`: the chain's results plus the report view ([analysis-result.md](analysis-result.md)).
8. `analysis_document` builds the analysis document once from the result ([analysis-document.md](analysis-document.md)).
9. `main.c` writes `piece.analysis.json` next to the input.

## Finding things

Every doc is listed in the [docs index](README.md).
