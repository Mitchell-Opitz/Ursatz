# Analysis result

`analyzer_analyze` (`<ursatz_analyzer/analysis_result.h>`) runs the analysis once over `ImportedNotes` and returns an
`AnalyzerResult`: the library's results, the claims of every area as `Interpretation`s, and the report view the
document and the Markdown report are exported from (R-217, R-220, R-252). The analysis document is an export of the
result, not its source.

```c
AnalyzerResult *result = NULL;
AnalyzerOptions options = analyzer_options_default();
if (analyzer_analyze(&notes, &options, &result) == ANALYZER_OK) {
    size_t cadences = analyzer_result_claim_count(result, ANALYZER_AREA_CADENCES);
    const Interpretation *first = analyzer_result_claim(result, ANALYZER_AREA_CADENCES, 0);
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
| `ANALYZER_ERR_CANCELLED` | The `cancel` callback returned true. |

On any status but `ANALYZER_OK`, `*out` is NULL and nothing is kept.

`AnalyzerOptions.stages` holds the `ANALYSIS_CHAIN_STAGE_*` bits of `ursatz_chain/analysis_chain.h` (bits 0 to 11) and
the `ANALYZER_STAGE_*` bits for the Analyzer-only stages: progressions, harmonic rhythm, meter check, rhythm, markings
and form plan. `analyzer_options_default()` selects all of them, with no callback. A chain stage pulls in the stages
it needs (see the chain header).

`ANALYZER_STAGE_VOICING` and `ANALYZER_STAGE_COUNTERPOINT` are reserved. Those two checks need a species and a voice
choice the options do not carry. Voicing stays on its own entry (`voicing_check.h`); counterpoint is checked with
standard-counterpoint, not by the Analyzer.

`cancel(cancel_context)` is asked before the chain run and before each selected Analyzer-only stage. When it returns
true the run stops with `ANALYZER_ERR_CANCELLED`.

The report view, and so the document and the Markdown report, is built only when every chain stage ran. With fewer,
the result holds the chain and the claims of the stages that ran; `analyzer_result_report` is an empty report and
`analysis_document_build_from_result` and `markdown_report_render_result` return NULL.

## Library results

`analyzer_result_chain(result)` is the `AnalysisChain` of `ursatz_chain/analysis_chain.h`, read through its accessors.
It is NULL for a piece with no notes.

## Claims

`analyzer_result_claim_count(result, area)` and `analyzer_result_claim(result, area, index)`. A claim is borrowed from
the result. Each targets the id of its note, segment or shell and keeps the library's scalar confidence. They are built
once, in the order the library returns them.

| `AnalyzerArea` | Claims | Source |
| --- | --- | --- |
| `KEYS` | the key of each key region | `common-practice-minimal` |
| `CHORDS` | the best chord of each segment that has one | `common-practice-minimal` |
| `NUMERALS` | the Roman numeral of each segment that has one | `standard-tonality` |
| `CADENCES` | each cadence's primary reading | `standard-harmony` |
| `PHRASES`, `THEMES`, `MOTIVES` | each phrase, theme and motive family | `standard-form` |
| `METER` | syncopations, hemiolas, hypermeasures, reinterpretations, densities, and the meter check | `standard-meter` |
| `TEXTURE` | each texture passage and each change between passages | `standard-texture` |
| `VOICE_LEADING` | each motion, perfect-interval violation and resolution | `standard-counterpoint`, `standard-harmony` |
| `MELODY` | the soprano's intervals, contour summaries, climaxes and leap recoveries | `standard-counterpoint` |
| `SCHEMAS` | each galant schema and scale sequence | `galant-schemas`, `scale-harmonization` |
| `PROGRESSIONS` | each recurring progression | the Analyzer's count, as a `standard-harmony` claim |
| `RHYTHM` | each tuplet run | the Analyzer's, as a `standard-meter` claim |

The progression and tuplet claims are made with the kernel's `claim_interpretation_create` (R-236): they have no
library owner, and carry no uncertainty of their own. The meter check targets the piece's first chord segment, since
the claim has no notes.

How the claims relate to the document's sections:

- `KEYS`, `CADENCES`, `PHRASES`, `THEMES`, `MOTIVES`, `TEXTURE`, `SCHEMAS` and `PROGRESSIONS` equal the entries of the
  section of the same name (`keys.regions`, `cadences`, ...).
- `CHORDS` and `NUMERALS` equal the `chords` entries with a chord reading and with a labelled Roman numeral.
- `METER` and `RHYTHM` together are the document's `rhythm` entries, except that a hypermeasure and a reinterpretation
  are one claim each where the document has one entry per phrase.
- `VOICE_LEADING` and `MELODY` are the chain's own results. The document aggregates them per chord and per phrase, so
  the counts differ.

## Exports

`analysis_document_build_from_result` and `analysis_document_render_result` (`analysis_document.h`), and
`markdown_report_render_result` (`markdown_report.h`), render from the result. The document's content and
`URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION` are unchanged: the same bytes as `analysis_document_build` over the same
notes and report.

`run_analysis_pipeline` is a wrapper over `analyzer_analyze`: it moves the result's report out into the caller's
`AnalysisReport`, with the same ownership as before. `AnalysisReport` and the fragment API stay until the Composer and
the GUI have switched to the result.
