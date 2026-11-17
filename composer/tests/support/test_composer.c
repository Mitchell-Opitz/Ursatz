#include "support/test_composer.h"

#include <stdlib.h>
#include <string.h>

#include "check/hearing.h"
#include "form/form_internal.h"
#include "motive/motive_internal.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

PlanResult test_read_plan_file(const char *path, PiecePlan **out, PlanJsonError *error) {
    char *text = test_must_read_file(path, NULL);
    PlanResult result = plan_from_json(test_knowledge(), text, out, error);
    free(text);
    return result;
}

MotiveResult test_load_motive_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                                   MotiveError *error) {
    size_t size = 0;
    char *data = test_must_read_file(path, &size);
    MotiveOptions with;
    memset(&with, 0, sizeof(with));
    if (options) {
        with = *options;
    }
    if (!with.knowledge) {
        with.knowledge = test_knowledge();
    }
    MotiveResult result = motive_load(data, size, format, &with, out, error);
    free(data);
    return result;
}

NodeId test_find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

NodeId test_find_named(const PiecePlan *plan, NodeId node, const char *name) {
    for (size_t i = 0; i < plan_node_child_count(plan, node); i++) {
        NodeId child = plan_node_child_at(plan, node, i);
        if (strcmp(plan_node_name(plan, child), name) == 0) {
            return child;
        }
        NodeId inner = test_find_named(plan, child, name);
        if (inner != PLAN_NODE_NONE) {
            return inner;
        }
    }
    return PLAN_NODE_NONE;
}

FormHearResult test_form_hear(const PiecePlan *plan, PlanSpan span, FormHeard *out) {
    if (!out) {
        return FORM_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return FORM_HEAR_ERR_NULL;
    }
    ComposerHearing *hearing = NULL;
    HearingResult made = hearing_create(plan, test_knowledge(), &hearing);
    FormHearResult status = FORM_HEAR_OK;
    switch (made) {
    case HEARING_OK:
        status = form_hear_in(hearing, plan, span, out);
        break;
    case HEARING_ERR_NULL:
        status = FORM_HEAR_ERR_NULL;
        break;
    case HEARING_ERR_ALLOC:
        status = FORM_HEAR_ERR_ALLOC;
        break;
    case HEARING_ERR_NOT_WRITTEN:
        status = FORM_HEAR_ERR_NOT_WRITTEN;
        break;
    case HEARING_ERR_PLAN:
        status = FORM_HEAR_ERR_METER;
        break;
    default:
        status = FORM_HEAR_ERR_ANALYSIS;
        break;
    }
    hearing_destroy(hearing);
    return status;
}

ReturnHearResult test_return_hear(const PiecePlan *plan, ReturnHeard *out) {
    ComposerHearing *hearing = NULL;
    if (!plan || !out) {
        return RETURN_HEAR_ERR_NULL;
    }
    HearingResult made = hearing_create(plan, test_knowledge(), &hearing);
    if (made != HEARING_OK) {
        memset(out, 0, sizeof(*out));
        return made == HEARING_ERR_ALLOC ? RETURN_HEAR_ERR_ALLOC : RETURN_HEAR_ERR_ANALYSIS;
    }
    ReturnHearResult status = return_hear_in(hearing, plan, out);
    hearing_destroy(hearing);
    return status;
}

bool test_piece_check_keys(const PiecePlan *plan, PieceKeyCheck *out) {
    ComposerHearing *hearing = NULL;
    if (!plan || !out || hearing_create(plan, test_knowledge(), &hearing) != HEARING_OK) {
        return false;
    }
    bool read = piece_check_keys_in(hearing, plan, out);
    hearing_destroy(hearing);
    return read;
}

AnalyzerCheckResult test_analyzer_check_piece(const PiecePlan *plan, AnalyzerCheck *out) {
    if (!out) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    ComposerHearing *hearing = NULL;
    if (!plan) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    HearingResult made = hearing_create(plan, test_knowledge(), &hearing);
    AnalyzerCheckResult result = ANALYZER_CHECK_OK;
    switch (made) {
    case HEARING_OK:
        result = analyzer_check_piece_in(hearing, out);
        break;
    case HEARING_ERR_NULL:
        result = ANALYZER_CHECK_ERR_NULL;
        break;
    case HEARING_ERR_ALLOC:
        result = ANALYZER_CHECK_ERR_ALLOC;
        break;
    case HEARING_ERR_NOT_WRITTEN:
        result = ANALYZER_CHECK_ERR_NOT_WRITTEN;
        break;
    case HEARING_ERR_PLAN:
        result = ANALYZER_CHECK_ERR_PLAN;
        break;
    default:
        result = ANALYZER_CHECK_ERR_ANALYSIS;
        break;
    }
    hearing_destroy(hearing);
    return result;
}

ComposerMeasureResult test_style_measure(const PiecePlan *plan, const ComposerStyle *style, StyleComparison *out) {
    if (!out) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (!plan || !style) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    ComposerHearing *hearing = NULL;
    HearingResult made = hearing_create(plan, test_knowledge(), &hearing);
    ComposerMeasureResult result = COMPOSER_MEASURE_OK;
    if (made != HEARING_OK) {
        result = made == HEARING_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_PLAN;
    } else {
        result = composer_style_measure_in(hearing, style, out);
    }
    hearing_destroy(hearing);
    return result;
}

const char *test_status_name(ComposerStatus status) {
    static const char *const NAMES[] = {"ok", "request", "unsatisfiable", "cancelled", "alloc", "internal"};
    return (unsigned)status < sizeof NAMES / sizeof NAMES[0] ? NAMES[status] : "unknown";
}

char *test_motive_json(const Motive *motive) {
    return motive ? motive_json_text(motive_json_object(motive)) : NULL;
}

bool test_roman_parse_applied(HarmonyMode mode, const char *roman, HarmonyAppliedChord *out) {
    int inversion;
    return harmony_roman_parse_applied_inverted(mode, roman, out, &inversion) && inversion == 0;
}

bool test_preferences_are_default(const ComposerPreferences *preferences) {
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    cJSON *written = composer_preferences_write(preferences);
    cJSON *plain = composer_preferences_write(&defaults);
    bool same = written && plain && cJSON_Compare(written, plain, true);
    cJSON_Delete(written);
    cJSON_Delete(plain);
    return same;
}
