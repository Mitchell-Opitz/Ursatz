# Analyzer architecture

How `ursatz-analyzer` is put together: what it is for, what it calls, what it offers and how one file becomes an analysis document. Back to the [docs index](README.md).

## Purpose

- `ursatz-analyzer` turns a MIDI or MusicXML file into an analysis: a Markdown report and a versioned JSON analysis document.
- It is a thin consumer of the Ursatz library: it imports the file, runs the library's analysis chain once, collects the claims and exports them.
- It holds no music theory of its own; every rule it applies is the library's.
- It also runs a corpus, style profiles, similar material search and motive links over analysis documents.

## Where it sits

- Below: the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) library, a git submodule at `external/ursatz`. `scripts/check-pins.sh` fails when a submodule pin is not on its remote's `main`. Details in [building.md](building.md#version-header-and-pin-check).
- Above: the [Composer](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/architecture.md) calls `composer_api.h`, and the [GUI](https://github.com/Mitchell-Opitz/Ursatz-GUI/blob/main/docs/architecture.md) calls the playback and score export entries and reads the analysis document. Both link `ursatz_analyzer`.

## Module map

| Directory | What it holds | Doc |
| --- | --- | --- |
| `include/ursatz_analyzer/` | The public headers, included as `<ursatz_analyzer/xxx.h>` | [library.md](library.md) |
| `src/pipeline/` | Import, the chain wrapper, one file per analysis area, the document and report exporters, corpus, playback, score export | [library.md](library.md#pipeline) |
| `src/links/` | Motive links, their cache and their JSON | [motive-links.md](motive-links.md) |
| `src/style/` | Style labels, profiles, the model file, similar material | [style-profile.md](style-profile.md) |
| `src/util/` | Private JSON, file and misc helpers | [library.md](library.md) |
| `src/main.c` | The CLI | [README](../README.md#usage) |
| `schema/` | JSON Schemas of the document and the other JSON files | [analysis-document.md](analysis-document.md) |
| `scripts/` | Pin, include, chain build, file scope and docs checks | [building.md](building.md) |
| `tests/` | Unit, snapshot and CLI tests | [building.md](building.md#tests) |
| `docs/analyses/` | What each analysis reads from the library and how | [harmony](analyses/harmony.md), [form and motive](analyses/form-and-motive.md), [rhythm, texture and voice leading](analyses/rhythm-texture-voice-leading.md) |
| `external/` | The Ursatz submodule and vendored cJSON | [building.md](building.md) |

## Public API

The CLI commands, from [the README](../README.md#usage):

| Command | What it does | Doc |
| --- | --- | --- |
| `ursatz-analyzer <file>` | Writes `<stem>.report.md` and `<stem>.analysis.json` | [analysis-document.md](analysis-document.md) |
| `--corpus <dir>` | Analyzes a folder into documents and a corpus index | [corpus.md](corpus.md) |
| `--style-profile`, `--compare-styles` | Counts and compares styles | [style-profile.md](style-profile.md) |
| `--similar <dir>` | Finds similar material by motive or progression | [similar-material.md](similar-material.md) |
| `--link-motives <dir>` | Links the motive families across pieces | [motive-links.md](motive-links.md) |

The GUI entries:

| Header | Entry | What it does | Doc |
| --- | --- | --- | --- |
| `input_pipeline.h` | `input_pipeline_import_file` | Imports a file, with its tempo map | [gui-api.md](gui-api.md#import-with-tempo) |
| `playback.h` | `analyzer_playback_build`, `analyzer_playback_to_json` | Tempo regions and notes for a player | [gui-api.md](gui-api.md#playback-timing) |
| `score_export.h` | `analyzer_score_export_musicxml`, `analyzer_score_note_xml_ids`, `analyzer_imported_meter_defaulted` | MusicXML score of the import, note ids, whether the meter was inferred | [gui-api.md](gui-api.md#score-export) |

The Composer entries:

| Header | Entry | What it does | Doc |
| --- | --- | --- | --- |
| `composer_api.h` | `analyzer_hear` | Hears borrowed events into an `AnalyzerResult` | [composer-api.md](composer-api.md#hear-events) |
| `composer_api.h` | `AnalyzerStyleReference`, `analyzer_result_style_distance` | Style distance of a result | [composer-api.md](composer-api.md#style-reference) |
| `composer_api.h` | `analyzer_result_violations` | Voice-leading faults of a result | [composer-api.md](composer-api.md#violations) |

The other public headers (analysis result, report, document, corpus, staleness, chord sequence, similar material, motive links, style profile) are listed in [library.md](library.md#library).

## Version constants and schemas

| Name | File | Current value | Doc |
| --- | --- | --- | --- |
| `URSATZ_ANALYZER_VERSION`, `ursatz_analyzer_version()` | `<ursatz_analyzer/version.h>`, generated from `src/version.h.in` at configure time | 0.6.0 (from `project()` in `CMakeLists.txt`) | [building.md](building.md#version-header-and-pin-check) |
| `URSATZ_ANALYZER_URSATZ_COMMIT` | `<ursatz_analyzer/version.h>` | the Ursatz commit the build was made against | [building.md](building.md#version-header-and-pin-check) |
| `URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION` | `include/ursatz_analyzer/analysis_document.h` | 9 | [versions.md](analysis-document/versions.md) |
| analysis document schema | `schema/analysis-document.v9.schema.json` | v9 | [analysis-document.md](analysis-document.md) |
| corpus index, motive links, motive link cache, similar material, style model schemas | `schema/*.v1.schema.json` | v1 each | [corpus.md](corpus.md), [motive-links.md](motive-links.md), [similar-material.md](similar-material.md), [style-profile.md](style-profile.md) |

A stored document is current only when its schema version, analyzer version and Ursatz commit all match the build ([corpus.md](corpus.md)).

## What it calls below

The Ursatz headers the Analyzer includes, found by `scripts/check-library-includes.sh`, which allows only staged public paths. All come from [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md).

| Entry used | From which repo | For what |
| --- | --- | --- |
| `ursatz_chain/analysis_chain.h`, `ursatz_chain/chain_inputs.h` (`analysis_chain_run` and the `analysis_chain_*` accessors) | Ursatz | The one analysis run and its results; the public headers reach them only through `result_chain.h` |
| `io/midi_importer.h`, `io/midi_key_signature.h`, `io/musicxml_importer.h` | Ursatz | Reading MIDI and MusicXML files |
| `core/`, `time/`, `pitch/`, `events/`, `identity/` | Ursatz | Rationals, meters, tempo maps, pitches, notes, ids |
| `interpretation/`, `semantics/`, `structure/`, `containers/`, `relationship/` | Ursatz | Claims as `Interpretation`s and the kernel vocabulary |
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
2. `input_pipeline` imports the file through the Ursatz MIDI or MusicXML importer into `ImportedNotes`: notes, a `MeterMap`, key signature events, the tempo map and an `ImportedSource` ([input.md](input.md#input-formats)). Notation readings (`--meter`, `--value-scale`, `--pickup`) are applied here.
3. `analyzer_analyze` runs `analysis_pipeline` as one `AnalyzerPass`: `chain_input` builds the chain's neutral input from the notes ([library.md](library.md#pipeline)).
4. `analysis_chain_run` runs once over every stage: segments, key regions, chords, Roman numerals, outer voices, cadences, form, motives, meter, texture, voice leading, melody, schemas and sequences.
5. The stage files read the chain's accessors into the `AnalysisReport`; the library calls the chain does not run (functions, six-fours, prolongations, phrase model) are made over its prepared harmony segments. See [analyses/harmony.md](analyses/harmony.md).
6. The Analyzer-only stages read the collected report: progressions, harmonic rhythm, form plan, meter check, rhythm, markings, voicing check, counterpoint check and pivot chords. See [analyses/form-and-motive.md](analyses/form-and-motive.md) and [analyses/rhythm-texture-voice-leading.md](analyses/rhythm-texture-voice-leading.md).
7. The result is an `AnalyzerResult`: the claims of every area as `Interpretation`s plus the report view ([analysis-result.md](analysis-result.md)).
8. `analysis_document` builds the analysis document once from the result, and `markdown_report` renders the report from that document ([analysis-document.md](analysis-document.md)).
9. `report_writer` writes `piece.report.md` and `piece.analysis.json` next to the input.

## Finding things

Every doc is listed in the [docs index](README.md).
