# Analysis result

The `AnalyzerResult` the analysis returns and the exports built from it. Back to the [docs index](README.md).

`analyzer_analyze` (`<ursatz_analyzer/analyzer_api.h>`) runs the analysis once over `ImportedScore` and returns an
`AnalyzerResult`: the library chain's results, the report view the document is exported from, the realization of the notes and the Analyzer's
reading of it (R-217, R-220, R-252, R-530, R-630). The analysis document is an export of the result, not its source.

```c
AnalyzerResult *result = NULL;
AnalyzerOptions options = analyzer_options_default();
if (analyzer_analyze(&notes, &options, &result) == URSATZ_OK) {
    const ChainResult *report = analyzer_result_report(result);
    const Work *realization = analyzer_result_realization(result);
    const Reading *reading = analyzer_result_reading(result);
    char *json = analysis_document_render_result("piece.mid", result);
    ...
    analyzer_result_destroy(result);
}
```

The result borrows `notes`, which must outlive it. `analyzer_result_destroy(NULL)` does nothing.

## Options and status

| `UrsatzStatus` | Meaning |
| --- | --- |
| `URSATZ_OK` | `*out` is the result. |
| `URSATZ_ERR_REQUEST` | NULL `notes` or `out`, or options with no chain stage. |
| `URSATZ_ERR_ALLOC` | Out of memory. |
| `URSATZ_ERR_INTERNAL` | The library chain refused the notes. |

On any status but `URSATZ_OK`, `*out` is NULL and nothing is kept.

`AnalyzerOptions.stages` holds the `ANALYSIS_CHAIN_STAGE_*` bits of `chain/analysis_chain.h`: bits 0 to 11 are the chain
stages, bits 12 to 17 the stages of the collected result only (progressions, harmonic rhythm, meter check, rhythm,
markings and form plan). `analyzer_options_default()` selects all of them (`ANALYSIS_CHAIN_STAGE_ALL`).

The result is the library's: `analyzer_analyze` runs `analysis_chain_run_score` and holds the `AnalysisChain`;
`analyzer_result_report` returns its `ChainResult` (`chain/chain_result.h`, R-615), the collected view the document is
exported from. The collectors and the result-only stages live in `library/chain` ([chain README](../../library/chain/README.md)). A chain stage pulls in the stages
it needs (see the chain header).

Voicing stays on its own entry (`voicing_check.h`); counterpoint is checked with standard-counterpoint, not by the
Analyzer.

The report view, and so the document, is built only when every chain stage ran. With fewer,
the result holds the chain's results of the stages that ran; `analyzer_result_report` is an empty report and
`analyzer_document_build` returns NULL.

## Library results

The `AnalysisChain` behind a result is the Analyzer's own: `analyzer_result_run_chain` (`result/analysis_result_internal.h`) gives it
to the Analyzer's exports and tests. No public header returns it (R-620: the Composer hears through `library/chain` itself), and a target
linking `ursatz_analyzer` sees the chain's result header (`chain/chain_result.h`) but no chain internal.

## The layers

After the chain run `analyzer_analyze` makes the two objects a reader of the piece shares with others (R-630):

- `analyzer_result_realization`: the realization of the notes (`imported_score_realization`, R-628): work `w1`, movement `m1`, one performer part for each part of the import, every note with an id (`library/docs/modules/realization.md`).
- `analyzer_result_reading`: the Analyzer's reading of it (`reading_build`, R-629): regions, chords, cadences, groups, motive occurrences, meters, lines and voice faults, each with its performer note ids as evidence. Its source is `ursatz-analyzer` with the producer key, the stages of the run and the knowledge hash; its target is `w1`, `m1` and `realization_digest` of the realization. The reading's detail is the chain, so the document can be printed from it.

Both are owned by the result (the reading is freed before the chain). A failure to build either is `URSATZ_ERR_ALLOC` or `URSATZ_ERR_INTERNAL`, as for the chain. With a partial stage set the reading holds the claims the chain can give (the meters and the voice faults).

`ursatz-analyzer --layers <dir> <input>` writes them: `<dir>/realization.json` (`realization_work_to_json`) and `<dir>/readings/analyzer.json` (`reading_to_json`), making the folders (a write failure exits 2 with `cannot write '<path>'`). Another reader's file sits beside the Analyzer's in `readings/`: `tools/annotations/annotation_to_reading.py` turns a verified annotation (`tests/fixtures/annotations/*_v1_expected.txt`) into Michelle's reading of the same realization, and `test_layers_two_readings` holds both readings of Mozart 545. `ursatz-analyzer --model-plan <file> <input>` also writes the minimal plan of the Analyzer's reading (`reading_to_plan`, R-635): the form over the piece, its meters, one section for each top-level section claim, the keys of the region claims and the chords with a numeral, with `derived_from` naming the reading and the realization digest, and no notes. The `.analysis.json` output is unchanged.

## Exports

`analyzer_document_build` and `analysis_document_render_result` (`analysis_document.h`) render from the
result: they call `analysis_document_build_from_reading` over the result's reading, notes and knowledge snapshot (the labels of a notated key signature are spelled with its major and natural minor materials). The report form `analysis_document_build(path, notes, report, knowledge)` is gone (P62 B10).

The wrapper `run_analysis_pipeline` (and `analysis_pipeline.h`) is gone (P61 B9): the GUI calls `analyzer_analyze` and `analyzer_document_build`, and the Analyzer's tests call `analyzer_analyze` through `test_analyze_report` (`tests/support/test_support.h`: it keeps the result and gives the test a shallow copy of its report view, released with `test_release_report`).
