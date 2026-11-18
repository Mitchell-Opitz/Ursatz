#include <ursatz_composer/style.h>

#include "style_internal.h"

#include <string.h>

#include <chain/chain_style.h>

#include "check/hearing_result.h"

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
    const AnalysisChain *heard = NULL;
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
    const StyleReference *reference = composer_style_reference(style);
    StyleReferenceResult compared = reference ? chain_style_distance(heard, reference, out) : STYLE_REFERENCE_ERR_ARGUMENT;
    if (compared != STYLE_REFERENCE_OK) {
        memset(out, 0, sizeof(*out));
        return compared == STYLE_REFERENCE_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_ANALYSIS;
    }
    return COMPOSER_MEASURE_OK;
}
