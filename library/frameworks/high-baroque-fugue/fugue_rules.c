#include "fugue_rules.h"

#include <stddef.h>

#include "fugue_answer_rules.h"
#include "fugue_countersubject_rules.h"
#include "fugue_entry_rules.h"

static const char *const NAMES[FUGUE_GROUP_COUNT] = {"entries", "answer", "countersubject"};

const char *fugue_group_name(FugueGroup group) {
    return group >= 0 && group < FUGUE_GROUP_COUNT ? NAMES[group] : NULL;
}

static void record(FugueResult *result, unsigned group, unsigned hard, unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

bool fugue_check(const FugueExposition *exposition, FugueResult *out) {
    if (out == NULL || !fugue_exposition_valid(exposition)) {
        return false;
    }
    FugueResult result = {0, 0};
    unsigned hard, soft;

    if (!fugue_entry_check(exposition, &hard, &soft)) {
        return false;
    }
    record(&result, FUGUE_GROUP_ENTRIES, hard, soft);

    if (!fugue_answer_check(exposition, &hard, &soft)) {
        return false;
    }
    record(&result, FUGUE_GROUP_ANSWER, hard, soft);

    if (!fugue_countersubject_check(exposition, &hard, &soft)) {
        return false;
    }
    record(&result, FUGUE_GROUP_COUNTERSUBJECT, hard, soft);

    *out = result;
    return true;
}

bool fugue_valid(const FugueExposition *exposition) {
    FugueResult result;
    return fugue_check(exposition, &result) && result.hard_violations == 0;
}
