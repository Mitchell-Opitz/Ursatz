#ifndef URSATZ_ANALYZER_ANALYSIS_RESULT_H
#define URSATZ_ANALYZER_ANALYSIS_RESULT_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

#include "interpretation/interpretation.h"

/*
 * The analysis as library results (R-220): one run over imported notes gives the library chain's results, the claims
 * (Interpretations) of every area, and the report view the document and the Markdown report are exported from. The
 * document JSON (analysis_document.h) and the Markdown report (markdown_report.h) are exports of an AnalyzerResult; they
 * hold nothing the result does not.
 */

typedef struct AnalyzerResult AnalyzerResult;

typedef enum {
    ANALYZER_OK = 0,
    ANALYZER_ERR_ARGUMENT,
    ANALYZER_ERR_ALLOC,
    ANALYZER_ERR_LIBRARY,
    ANALYZER_ERR_CANCELLED
} AnalyzerStatus;

/*
 * The stages of a run. `stages` holds ANALYSIS_CHAIN_STAGE_* bits (bits 0 to 11, the library chain, from
 * ursatz_chain/analysis_chain.h) and the bits below for the Analyzer-only stages. The chain stages pull in the stages they need (ursatz_chain/analysis_chain.h).
 *
 * The report view, and so the document and the Markdown report, is collected only when every chain stage is selected.
 * With fewer, the result holds the chain and the claims its stages give, and analyzer_result_report is an empty report.
 * The Analyzer-only stages read the chain's results: a bit set without the chain stage it reads gives no entries.
 * ANALYZER_STAGE_VOICING and ANALYZER_STAGE_COUNTERPOINT are reserved: those checks need a species and a voice choice
 * the options do not carry. Voicing stays on its own entry (voicing_check.h); counterpoint is checked with
 * standard-counterpoint, not by the Analyzer.
 */
#define ANALYZER_STAGE_PROGRESSIONS    (1u << 12)
#define ANALYZER_STAGE_HARMONIC_RHYTHM (1u << 13)
#define ANALYZER_STAGE_METER_CHECK     (1u << 14)
#define ANALYZER_STAGE_RHYTHM          (1u << 15)
#define ANALYZER_STAGE_MARKINGS        (1u << 16)
#define ANALYZER_STAGE_FORM_PLAN       (1u << 17)
#define ANALYZER_STAGE_VOICING         (1u << 18)
#define ANALYZER_STAGE_COUNTERPOINT    (1u << 19)
#define ANALYZER_STAGE_ALL             ((1u << 20) - 1u) /* every chain stage (bits 0 to 11) and the above */

typedef struct {
    unsigned stages;
    /* Asked before the chain run and between the Analyzer stages; true stops the run. May be NULL. */
    bool (*cancel)(void *context);
    void *cancel_context;
} AnalyzerOptions;

/* All stages, no cancel callback. */
AnalyzerOptions analyzer_options_default(void);

/*
 * Analyzes notes. options may be NULL for the defaults. On ANALYZER_OK *out is the result, which the caller releases with
 * analyzer_result_destroy and which borrows notes: notes must outlive it. On any other status *out is left NULL and
 * nothing is kept: ANALYZER_ERR_ARGUMENT for a NULL notes, options with no stages or out, ANALYZER_ERR_ALLOC,
 * ANALYZER_ERR_LIBRARY when the chain refuses the notes, ANALYZER_ERR_CANCELLED when the callback asked to stop.
 */
AnalyzerStatus analyzer_analyze(const ImportedNotes *notes, const AnalyzerOptions *options, AnalyzerResult **out);

/* NULL is a no-op. */
void analyzer_result_destroy(AnalyzerResult *result);

/* The library chain is read through result_chain.h (analyzer_result_chain), served by the ursatz_analyzer::chain target. */

/*
 * The claims of an area. Each is an Interpretation of the owning framework's claim (or the Analyzer's, for progressions
 * and tuplets, through the framework's claim helper) that targets the id of its note, segment or shell and keeps the
 * library's confidence. They are built once per result, in the order the library returns them (time order within each
 * result list) and borrowed from the result.
 *
 *   KEYS            the key of each key region
 *   CHORDS          the best chord of each chord segment that has one
 *   NUMERALS        the Roman numeral of each chord segment that has one
 *   CADENCES        each cadence's primary reading
 *   PHRASES         each phrase
 *   THEMES          each theme
 *   MOTIVES         each motive family
 *   METER           the meter check, syncopations, hemiolas, hypermeasures, reinterpretations and densities
 *   TEXTURE         each texture passage and each change between passages
 *   VOICE_LEADING   each voice-pair motion, perfect-interval violation and resolution
 *   MELODY          the soprano's melodic intervals, contour summaries, climaxes and leap recoveries
 *   SCHEMAS         each galant schema and each scale sequence
 *   PROGRESSIONS    each recurring chord progression
 *   RHYTHM          each tuplet run
 */
typedef enum {
    ANALYZER_AREA_KEYS,
    ANALYZER_AREA_CHORDS,
    ANALYZER_AREA_NUMERALS,
    ANALYZER_AREA_CADENCES,
    ANALYZER_AREA_PHRASES,
    ANALYZER_AREA_THEMES,
    ANALYZER_AREA_MOTIVES,
    ANALYZER_AREA_METER,
    ANALYZER_AREA_TEXTURE,
    ANALYZER_AREA_VOICE_LEADING,
    ANALYZER_AREA_MELODY,
    ANALYZER_AREA_SCHEMAS,
    ANALYZER_AREA_PROGRESSIONS,
    ANALYZER_AREA_RHYTHM,
    ANALYZER_AREA_COUNT
} AnalyzerArea;

/* 0 for a NULL result or an area outside the enum, or an area whose stage did not run. */
size_t analyzer_result_claim_count(const AnalyzerResult *result, AnalyzerArea area);

/* NULL for a NULL result, an area outside the enum or an index past the count. */
const Interpretation *analyzer_result_claim(const AnalyzerResult *result, AnalyzerArea area, size_t index);

/* The report view the document and the Markdown report are exported from (R-252). Never NULL for a result. */
const AnalysisReport *analyzer_result_report(const AnalyzerResult *result);

#endif
