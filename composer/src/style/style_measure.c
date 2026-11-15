#include <ursatz_composer/style.h>

#include "style_internal.h"

#include <string.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>

#include "check/hearing_result.h"

ComposerMeasureResult composer_style_measure(const PiecePlan *plan, const ComposerStyle *style, StyleComparison *out) {
    if (!out) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (!plan || !style) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    ComposerHearing *hearing = NULL;
    HearingResult made = hearing_create(plan, &hearing);
    ComposerMeasureResult result = COMPOSER_MEASURE_OK;
    if (made != HEARING_OK) {
        result = made == HEARING_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_PLAN;
    } else {
        result = composer_style_measure_in(hearing, style, out);
    }
    hearing_destroy(hearing);
    return result;
}

ComposerMeasureResult composer_style_measure_in(ComposerHearing *hearing, const ComposerStyle *style,
                                                StyleComparison *out) {
    if (!out) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (!hearing || !style) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    if (hearing_rendered(hearing)->count == 0) {
        return COMPOSER_MEASURE_ERR_NOT_WRITTEN;
    }
    const AnalyzerResult *heard = NULL;
    switch (hearing_piece(hearing, &heard)) {
    case HEARING_OK:
        break;
    case HEARING_ERR_ALLOC:
        return COMPOSER_MEASURE_ERR_ALLOC;
    case HEARING_ERR_NOT_WRITTEN:
        return COMPOSER_MEASURE_ERR_NOT_WRITTEN;
    case HEARING_ERR_PLAN:
        return COMPOSER_MEASURE_ERR_PLAN;
    default:
        return COMPOSER_MEASURE_ERR_ANALYSIS;
    }
    const AnalyzerStyleReference *reference = composer_style_reference(style);
    AnalyzerStatus compared = reference ? analyzer_result_style_distance(heard, reference, out) : ANALYZER_ERR_ARGUMENT;
    if (compared != ANALYZER_OK) {
        memset(out, 0, sizeof(*out));
        return compared == ANALYZER_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_ANALYSIS;
    }
    return COMPOSER_MEASURE_OK;
}
