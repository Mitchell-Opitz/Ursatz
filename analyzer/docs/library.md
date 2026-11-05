# Analyzer library and pipeline

The public headers of the `ursatz_analyzer` library, its CMake targets and the pipeline stages behind them. Back to the [docs index](README.md). The GUI entries are in [gui-api.md](gui-api.md) and the Composer entries in [composer-api.md](composer-api.md).

## Library

The pipeline builds as the `ursatz_analyzer` static library. The CLI and the tests link it.
Its public include directory is `include/`, and consumers include headers as
`<ursatz_analyzer/xxx.h>`. Everything under `src/` is private.

| Header | Purpose |
| --- | --- |
| `input_pipeline.h` | The common input interface: `input_pipeline_import_file` reads a MIDI or MusicXML file (chosen by `InputFormat`, or detected) into `ImportedNotes`; `input_pipeline_import_memory` reads the same from bytes. |
| `imported_notes.h` | `ImportedNotes`, what every input produces (notes, meter, key signatures, and an `ImportedSource` saying what the input states); `imported_notes_destroy` frees it. |
| `midi_pipeline.h` | The MIDI backend of the input interface. |
| `musicxml_pipeline.h` | The MusicXML backend of the input interface. |
| `analysis_result.h` | `analyzer_analyze` turns `ImportedNotes` into an `AnalyzerResult`: the library chain's results, the claims of every area as `Interpretation`s, and the report view the exports render from. See [analysis-result.md](analysis-result.md). |
| `result_chain.h` | `analyzer_result_chain`, the `AnalysisChain` behind an `AnalyzerResult`. The one public header that includes a chain header; needs the `ursatz_analyzer::chain` target. |
| `analysis_pipeline.h` | `run_analysis_pipeline`, a wrapper over `analyzer_analyze` that moves the report out as an `AnalysisReport`. |
| `analysis_report.h` | `AnalysisReport` and its entries (segment span, chord readings, non-chord tones), key regions and key changes. |
| `analysis_document.h` | Builds the `AnalysisDocument` from an `AnalyzerResult` (or an `AnalysisReport`), serializes it as JSON, and derives `<stem>.analysis.json`. |
| `markdown_report.h` | Renders an `AnalysisDocument`, or an `AnalyzerResult`, as the Markdown report, section by section. |
| `report_writer.h` | Derives the `<stem>.report.md` path and writes the file. |
| `corpus.h` | `corpus_run` analyzes every piece in a folder into documents and a corpus index, resumable. |
| `style_labels.h` | The labels a style profile counts (numerals, progressions, intervals, durations) and the category names, shared with the Composer. |
| `style_profile.h` | `StyleProfile` (counts over documents or a corpus), the style model file reader and writer, and `style_profile_compare`. |
| `staleness.h` | `analyzer_document_status` tells whether a stored analysis document is current (schema version, analyzer version and Ursatz commit all match this build) or stale; `analyzer_document_status_of` compares keys already read. See [corpus.md](corpus.md). |
| `chord_sequence.h` | `analyzer_chord_sequence` and `analyzer_chord_sequence_from_document` give the joined chord sequence (consecutive entries with one numeral, key region and root are one chord) of a result or of a stored document; `analyzer_chord_sequence_free` releases it. |
| `playback.h` | Playback timing for an import as the GUI player needs it. See [gui-api.md](gui-api.md). |
| `score_export.h` | MusicXML score export of the imported notes, with the base `xml:id` of each note. See [gui-api.md](gui-api.md). |
| `text_export.h` | `analyzer_text_export_piece` writes an analyzed piece as measure-addressed text: the notes of the score MusicXML and the claims of the analysis document, each claim in the measure where it starts. `analyzer_text_export_material` does the same for store material: sections of MusicXML with facts and segment marks the caller passes. See [text-export.md](text-export.md). |
| `similar_material.h` | `similar_material_find` scores the passages of a corpus against a motive or a progression query. |
| `motive_links.h` | `MotiveLinkScope` (analysis documents), `motive_links_run` links the motive families of its pieces, `MotiveLinkCache` keeps the pairs compared, `motive_links_render` writes the links as JSON. |
| `composer_api.h` | Hearing borrowed events into an `AnalyzerResult`, style distance and voice-leading faults, for the Composer. See [composer-api.md](composer-api.md). |

The Composer calls `composer_api.h` to analyze a fragment it generated, with no file: see [composer-api.md](composer-api.md).

Ownership: the caller owns the `AnalysisReport` and frees it with `analysis_report_destroy`,
which also frees every entry's labels. Labels are heap strings with no length limit; "no match"
marks a group or key that was not identified. It also frees each entry's `note_ids`. The caller
frees an `AnalysisDocument` with `analysis_document_destroy`; it keeps no reference to the notes
or the report, so those can be destroyed as soon as it is built. Strings returned by
`markdown_report_render`, `analysis_document_to_json`, `analysis_document_render`,
`derive_report_path` and `derive_analysis_document_path` are freed by the caller with `free`.

```cmake
add_subdirectory(external/ursatz-analyzer)
target_link_libraries(my_app PRIVATE ursatz_analyzer)        # the pipeline, and the Ursatz kernel its headers use
target_link_libraries(my_tool PRIVATE ursatz_analyzer::chain) # also reads the library chain (result_chain.h)
```

| Target | Exposes |
| --- | --- |
| `ursatz_analyzer` | Every public header except `result_chain.h`. Links `ursatz` PUBLIC (kernel types in the headers); the chain and the frameworks are linked PRIVATE, so a consumer cannot include their headers. |
| `ursatz_analyzer::chain` | INTERFACE target (`ursatz_analyzer_chain`): `ursatz_analyzer` plus `ursatz_chain`, and `result_chain.h` with `analyzer_result_chain`. Link it only to read chain claims. |

Private helper modules, each the one home of what several files used to copy:

| Module | Holds |
| --- | --- |
| `src/util/json_util.h` | `json_member`, `json_string_member`, `json_number_member`, `json_add`, `json_append`. |
| `src/util/file_util.h` | `file_read_all`, `file_write_atomic` (temporary name, then rename). |
| `src/util/misc_util.h` | `text_copy`, `size_compare`, `fraction_compare`, `entity_id_copy`, `notes_last_end`, `fnv1a_bytes`. |
| `src/pipeline/corpus_walk.h` | `corpus_walk_documents`: opens `corpus-index.json` and visits the document of every analyzed piece with its `AnalyzerDocumentStatus`; `corpus_walk_stale_count`. |
| `src/pipeline/run_file.h` | `analyzer_run_file` (import, optional reading, `analyzer_analyze`) and `analyzer_run_file_document`. |

The tests share `tests/support/test_support.h` (the `analyzer_test_support` library): a MIDI builder, JSON accessors,
file helpers and a document analyzer.

## Pipeline

`main.c` runs `input_pipeline` (MIDI or MusicXML import through Ursatz, both into `ImportedNotes`), then `analysis_pipeline`,
which runs the analysis as one `AnalyzerPass` (`chain.h`) inside `analyzer_analyze` (`analysis_result.h`). The Analyzer orchestrates the library's analysis chain
(`analysis_chain_run` of `ursatz_chain`, R-217, R-241) and collects its results; it adds no analysis of its own to them.

1. `chain_input`: builds the chain's neutral input from the imported notes: one `AnalysisNote` per event, sorted, and the
   file's key signature events.
2. `analysis_chain_run` runs once, over every stage: segments, key regions (a second pass on the chords and phrase ends of
   the first, so the Analyzer runs no pass of its own), chords, Roman numerals, outer voices, cadences, form, motives,
   meter, texture, voice leading, melody, schemas and sequences.
3. The stage files read the chain's accessors into the `AnalysisReport` and nothing more: `key_region_pipeline`,
   `chord_pipeline` (with the respelling of a notated score), `roman_numeral_pipeline`, `outer_voice_pipeline`,
   `harmony_pipeline` (cadences; the functions, six-fours, prolongations and phrase model, which the chain does not
   run, are library calls over the chain's prepared harmony segments), `form_pipeline`, `motive_pipeline`, `melody_pipeline` (each
   phrase's part of the chain's soprano line), `texture_pipeline`, `voice_leading_pipeline` and `schema_pipeline`.
   The stages read the chain's stage inputs (`analysis_chain_harmony_inputs`, `_prepared_harmony`, `_form_input`,
   `_motive_input`, `_voice_grid`, `_galant_segments`) and never call `chain_build_*`; `scripts/check-chain-builds.sh`
   fails CI when `src/` or `include/` does.
4. The Analyzer-only stages read the collected report and the chain's results: `progression_pipeline` and
   `harmonic_rhythm_pipeline`, `form_plan_pipeline` (sections, thematic returns), `meter_check_pipeline`,
   `rhythm_pipeline` (density, syncopation, hemiola, hypermeter, tuplets), `markings_pipeline`, `voicing_check` and
   `counterpoint_pipeline`. `identify_pivot_chords` also stays: the chain has no pivot reading.

`analysis_document` then builds the analysis document once, from the `AnalyzerResult`. The document is an export of the
result: the JSON file is its serialization, and `markdown_report` renders the report from it, never from the pipeline
directly. `report_writer` writes each file.
