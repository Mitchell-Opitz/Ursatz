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

`AnalyzerOptions.stages` holds the `ANALYSIS_CHAIN_STAGE_*` bits of `chain/analysis_chain.h` (bits 0 to 11) and
the `ANALYZER_STAGE_*` bits for the Analyzer-only stages: progressions, harmonic rhythm, meter check, rhythm, markings
and form plan. `analyzer_options_default()` selects all of them. A chain stage pulls in the stages
it needs (see the chain header).

Voicing stays on its own entry (`voicing_check.h`); counterpoint is checked with standard-counterpoint, not by the
Analyzer.

`AnalyzerOptions.harmonic_spans` (default true) reads the chords on harmonic spans; false reads one chord per segment, as before
Ursatz P39. The Composer's hearing sets it false until P41 (R-378). Nothing records it in the document, report or schema.

The report view, and so the document, is built only when every chain stage ran. With fewer,
the result holds the chain's results of the stages that ran; `analyzer_result_report` is an empty report and
`analysis_document_build_from_result` returns NULL.

## Library results

`analyzer_result_chain(result)` (`<ursatz_analyzer/result_chain.h>`) is the `AnalysisChain` of `chain/analysis_chain.h`,
read through its accessors. It is NULL for a piece with no notes. The header and the chain's headers reach only a target that
links `ursatz_analyzer::chain`; a target linking `ursatz_analyzer` alone sees no chain or framework header.

## No claim layer

The result has no claims (R-530): it is the chain's results plus the report view. There is no per-area list of
`Interpretation`s to read: a caller reads the chain's own results through `analyzer_result_chain` and the document's
entries through `analyzer_result_report` or the exported document.

## Exports

`analysis_document_build_from_result` and `analysis_document_render_result` (`analysis_document.h`) render from the
result: the same bytes as `analysis_document_build` over the same notes and report. `analysis_document_build` takes the knowledge
snapshot the report was made with (the labels of a notated key signature are spelled with its major and natural minor materials).

`run_analysis_pipeline` is a wrapper over `analyzer_analyze`: it moves the result's report out into the caller's
`ChainResult`, with the same ownership as before. `ChainResult` and the fragment API stay until the Composer and
the GUI have switched to the result.
