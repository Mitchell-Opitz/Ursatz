# Analyzer library and pipeline

The public headers of the `ursatz_analyzer` library, its CMake targets and the pipeline stages behind them. Back to the [docs index](README.md). The GUI entries are in [gui-api.md](gui-api.md) and the corpus passage entries in [corpus-passages.md](corpus-passages.md).

## Library

The pipeline builds as the `ursatz_analyzer` library. The CLI and the tests link its engine (`ursatz_analyzer_engine`, the full archive); the GUI, the workshop and the consumer projects link `ursatz_analyzer`, which exports only the API (below).
Its public include directory is `include/`: a consumer includes one header, `<ursatz_analyzer/analyzer_api.h>` (R-640). The headers of the Analyzer's own parts are internal: they are in `src/include/ursatz_analyzer_internal/` and only the CLI, the tests and the Analyzer's own files include them.

| Entries | Purpose |
| --- | --- |
| `analyzer_import_file` | The thin import entry: a MIDI or MusicXML file into an `ImportedScore`, with its tempo map and the text of a failed import (`NOT_FOUND` for a file that cannot be read, `FORMAT` for every other failure). `imported_score.h` and `ImportedFormat` are the library's (`io/imported_score.h`). |
| `analyzer_analyze`, `analyzer_options_default`, `analyzer_result_destroy` | Turn an `ImportedScore` into an `AnalyzerResult`: the library chain's results and the report view the document is exported from (no claim layer, R-530). See [analysis-result.md](analysis-result.md). |
| `analyzer_result_realization`, `analyzer_result_reading` | The realization of the piece and the Analyzer's reading of it (the layers the workshop stores beside `analysis.json`). |
| `analyzer_document_build`, `_to_json`, `_destroy` | Build the `AnalysisDocument` from an `AnalyzerResult`, serialize it as JSON, free it. |
| `analyzer_document_status`, `analyzer_document_status_of`, `analyzer_version`, `analyzer_producer_key` | Whether a stored document is current (schema version, analyzer version and producer key all match this build) or stale; `_of` compares the keys already read (`written_by`, `written_against`). See [corpus.md](corpus.md). |
| `analyzer_playback_build`, `_to_json`, `_destroy` | Playback timing for an import as the GUI player needs it. See [gui-api.md](gui-api.md). |
| `analyzer_score_export_musicxml`, `analyzer_score_note_xml_ids`, `analyzer_imported_meter_defaulted` | MusicXML score export of the imported notes with the title and composer of an `AnalyzerScoreMetadata`, the base `xml:id` of each note, and whether the meter was inferred. See [gui-api.md](gui-api.md). |
| `analyzer_text_export_piece`, `analyzer_text_export_material` | Write an analyzed piece or store material as measure-addressed text. See [text-export.md](text-export.md). |
| `analyzer_style_add_document_json` | Counts an analysis document into a `StyleProfile` (the library's style headers, see the [theory README](../../library/theory/README.md#style)). |
| `analyzer_links_*` | Motive links: `MotiveLinkScope`, `analyzer_links_run` (with a `cancelled` callback checked between pairs), `MotiveLinkCache` load and save, `analyzer_links_render`. See [motive-links.md](motive-links.md). |
| `analyzer_corpus_passages_*` | The passages of a corpus like a passage or a progression, as the Composer API takes them inline (R-619). See [corpus-passages.md](corpus-passages.md). |

The API is 38 names (`src/exports.txt`). Every failure is the common `UrsatzStatus` with an `UrsatzError` (`<common/status.h>`, R-639); `AnalyzerStatus` and the public result enums are gone, `AnalyzerDocumentStatus` stays (a staleness value, not an error). The public version constants of the persisted formats are in the header: `ANALYZER_DOCUMENT_SCHEMA_VERSION`, `ANALYZER_TEXT_EXPORT_VERSION`, `ANALYZER_CORPUS_INDEX_VERSION`, `ANALYZER_LINK_CACHE_VERSION`, `ANALYZER_MOTIVE_LINKS_VERSION`.

On Linux with GNU objcopy the library is one relocatable object whose only global symbols are those 38 names (`cmake/UrsatzExports.cmake`: `ld -r` over the engine archive, then `objcopy --keep-global-symbols`); the test `app_exports` lists the symbols with `nm` and fails on any outside the API. Never link the library and its engine into one executable.

The internal headers (`src/include/ursatz_analyzer_internal/`):

| Header | Purpose |
| --- | --- |
| `input_pipeline.h` | The import entries behind `analyzer_import_file` and `analyzer_import_buffer`. |
| `analysis_result.h` | The `AnalyzerResult` and its report view. |
| `analysis_document.h` | The document's builder and serializer (`analysis_document_render_result`; the path of a document is `analyzer_document_path_for`, in the API). |
| `chord_sequence.h` | `analyzer_chord_sequence_from_document` gives the joined chord sequence of a stored document. |
| `similar_material.h` | `similar_material_find` scores the passages of a corpus against a motive or a progression query. |
| `motive_links.h`, `style_profile.h` | The links and the style profile behind their API entries. |
| `version.h`, `producer_key.h` | Generated at configure time (`URSATZ_ANALYZER_VERSION` and the producer key, see [building.md](building.md#version-header-and-producer-key)). |

The Composer does not call the Analyzer (R-620): it hears a fragment it generated through the library chain (`analysis_chain_run_score` on an `imported_score_view`). The Composer CLI calls `corpus_passages.h`.

Ownership: the caller owns the `ChainResult` and frees it with `chain_result_destroy`,
which also frees every entry's labels. Labels are heap strings with no length limit; "no match"
marks a group or key that was not identified. It also frees each entry's `note_ids`. The caller
frees an `AnalysisDocument` with `analyzer_document_destroy`; it keeps no reference to the notes
or the report, so those can be destroyed as soon as it is built. Strings returned by
`analyzer_document_to_json`, `analysis_document_render_result` and `analyzer_document_path_for` are freed by the
caller with `free`.

```cmake
add_subdirectory(ursatz/analyzer)             # after ursatz/third_party and ursatz/library, as the root does
target_link_libraries(my_app PRIVATE ursatz_analyzer)        # the pipeline, and the Ursatz kernel its headers use
```

| Target | Exposes |
| --- | --- |
| `ursatz_analyzer` | The API header. Links `ursatz` PUBLIC (kernel types in the header) and the chain (its result header `chain/chain_result.h` is staged for the report); the chain's internals are invisible to a consumer. |

Private helper modules, each the one home of what several files used to copy:

| Module | Holds |
| --- | --- |
| `common/` (`<common/json.h>`, `files.h`, `strings.h`) | The JSON member readers, `common_json_add` and `_append`, the rational reader, whole-file read and atomic write, folder creation and the `CommonText` builder; the Analyzer keeps no copy. |
| `src/util/misc_util.h` | `text_copy`, `size_compare`, `fraction_compare`, `entity_id_copy`, `notes_last_end`, `fnv1a_bytes`. |
| `src/pipeline/corpus_walk.h` | `corpus_walk_documents`: opens `corpus-index.json` and visits the document of every analyzed piece with its `AnalyzerDocumentStatus`; `analyzer_corpus_stale_count` counts the stale ones (API). |
| `src/result/run_file.c` | `analyzer_run_file` (import, optional reading, `analyzer_analyze`) and `analyzer_run_file_document`, declared in the API header. |

The tests share `tests/support/test_support.h` (the `analyzer_test_support` library): a MIDI builder, JSON accessors,
file helpers and a document analyzer.

## Pipeline

`main.c` runs `input_pipeline` (MIDI or MusicXML import through Ursatz, both into `ImportedScore`), then `analyzer_analyze` (`analysis_result.h`),
which runs the library's analysis chain on the score (`analysis_chain_run_score` of `ursatz_chain`, R-217, R-241, R-615) and holds its collected `ChainResult`; the Analyzer adds no analysis of its own to it.

1. `chain_input`: builds the chain's neutral input from the imported notes: one `AnalysisNote` per event, sorted, and the
   file's key signature events.
2. `analysis_chain_run` runs once, over every stage: segments, key regions (a second pass on the chords and phrase ends of
   the first, so the Analyzer runs no pass of its own), chords, Roman numerals, outer voices, cadences, form, motives,
   meter, texture, voice leading, melody, schemas and sequences.
3. The stage files read the chain's accessors into the `ChainResult` and nothing more: `key_region_pipeline`,
   `chord_pipeline` (with the respelling of a notated score), `roman_numeral_pipeline`, `outer_voice_pipeline`,
   `harmony_pipeline` (cadences; the functions, six-fours, prolongations and phrase model, which the chain does not
   run, are library calls over the chain's prepared harmony segments), `form_pipeline`, `motive_pipeline`, `melody_pipeline` (each
   phrase's part of the chain's soprano line), `texture_pipeline`, `voice_leading_pipeline` and `schema_pipeline`.
   The stages read the chain's stage inputs (`analysis_chain_harmony_inputs`, `_prepared_harmony`, `_form_input`,
   `_motive_input`, `_voice_grid`, `_galant_segments`) and never call `chain_build_*`; the layer table
   (`cmake/layer-table.txt`) fails when `src/` or `include/` does.
4. The Analyzer-only stages read the collected report and the chain's results: `progression_pipeline` and
   `harmonic_rhythm_pipeline`, `form_plan_pipeline` (sections, thematic returns), `meter_check_pipeline`,
   `rhythm_pipeline` (density, syncopation, hemiola, hypermeter, tuplets), `markings_pipeline` and `voicing_check`.
   `identify_pivot_chords` also stays: the chain has no pivot reading.

`analysis_document` then builds the analysis document once, from the `AnalyzerResult`. The document is an export of the
result: the JSON file is its serialization, built from the result and never from the pipeline directly. `main.c` writes it.
