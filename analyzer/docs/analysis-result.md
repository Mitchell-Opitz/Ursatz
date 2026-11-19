# Analysis result

The `AnalyzerResult` the analysis returns and the exports built from it. Back to the [docs index](README.md).

`analyzer_analyze` (`<ursatz_analyzer/analysis_result.h>`) runs the analysis once over `ImportedScore` and returns an
`AnalyzerResult`: the library chain's results and the report view the document is exported from (R-217, R-220, R-252,
R-530). The analysis document is an export of the
result, not its source.

```c
AnalyzerResult *result = NULL;
AnalyzerOptions options = analyzer_options_default();
if (analyzer_analyze(&notes, &options, &result) == ANALYZER_OK) {
    const ChainResult *report = analyzer_result_report(result);
    char *json = analysis_document_render_result("piece.mid", result);
    ...
    analyzer_result_destroy(result);
}
```

The result borrows `notes`, which must outlive it. `analyzer_result_destroy(NULL)` does nothing.

## Options and status

| `AnalyzerStatus` | Meaning |
| --- | --- |
| `ANALYZER_OK` | `*out` is the result. |
| `ANALYZER_ERR_ARGUMENT` | NULL `notes` or `out`, or options with no chain stage. |
| `ANALYZER_ERR_ALLOC` | Out of memory. |
| `ANALYZER_ERR_LIBRARY` | The library chain refused the notes. |

On any status but `ANALYZER_OK`, `*out` is NULL and nothing is kept.

`AnalyzerOptions.stages` holds the `ANALYSIS_CHAIN_STAGE_*` bits of `chain/analysis_chain.h`: bits 0 to 11 are the chain
stages, bits 12 to 17 the stages of the collected result only (progressions, harmonic rhythm, meter check, rhythm,
markings and form plan). `analyzer_options_default()` selects all of them (`ANALYSIS_CHAIN_STAGE_ALL`).

The result is the library's: `analyzer_analyze` runs `analysis_chain_run_score` and holds the `AnalysisChain`;
`analyzer_result_report` returns its `ChainResult` (`chain/chain_result.h`, R-615), the collected view the document is
exported from. The collectors and the result-only stages live in `library/chain` ([chain README](../../library/chain/README.md)). A chain stage pulls in the stages
it needs (see the chain header).

Voicing stays on its own entry (`voicing_check.h`); counterpoint is checked with standard-counterpoint, not by the
Analyzer.

`AnalyzerOptions.harmonic_spans` (default true) reads the chords on harmonic spans; false reads one chord per segment, as before
Ursatz P39. The Composer's hearing sets it false until P41 (R-378). Nothing records it in the document, report or schema.

The report view, and so the document, is built only when every chain stage ran. With fewer,
the result holds the chain's results of the stages that ran; `analyzer_result_report` is an empty report and
`analysis_document_build_from_result` returns NULL.

## Library results

The `AnalysisChain` behind a result is the Analyzer's own: `analyzer_result_run_chain` (`pipeline/analysis_result_internal.h`) gives it
to the Analyzer's exports and tests. No public header returns it (R-620: the Composer hears through `library/chain` itself), and a target
linking `ursatz_analyzer` sees the chain's result header (`chain/chain_result.h`) but no chain internal.

## No claim layer

The result has no claims (R-530): it is the chain's results plus the report view. There is no per-area list of
`Interpretation`s to read: a caller reads the chain's results through the report and the document's
entries through `analyzer_result_report` or the exported document.

## Exports

`analysis_document_build_from_result` and `analysis_document_render_result` (`analysis_document.h`) render from the
result: the same bytes as `analysis_document_build` over the same notes and report. `analysis_document_build` takes the knowledge
snapshot the report was made with (the labels of a notated key signature are spelled with its major and natural minor materials).

The wrapper `run_analysis_pipeline` (and `analysis_pipeline.h`) is gone (P61 B9): the GUI calls `analyzer_analyze` and `analysis_document_build_from_result`, and the Analyzer's tests call `analyzer_analyze` through `test_analyze_report` (`tests/support/test_support.h`).
