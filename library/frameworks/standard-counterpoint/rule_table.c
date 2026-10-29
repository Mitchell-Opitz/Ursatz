#include "rule_table.h"

#include <stdio.h>
#include <string.h>

bool standard_counterpoint_rule_table_valid(const StandardCounterpointRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool standard_counterpoint_rule_table_is_hard(const StandardCounterpointRuleTable *table, int which) {
    if (!standard_counterpoint_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool standard_counterpoint_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

typedef struct {
    const StandardCounterpointRuleTable *table;
    int which;
} BuildPairArg;

static void build_pair(void *arg) {
    const StandardCounterpointRuleTable *table = ((const BuildPairArg *)arg)->table;
    int which = ((const BuildPairArg *)arg)->which;
    char name[96];
    if (table->prefix != NULL) {
        snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    } else {
        snprintf(name, sizeof(name), "%s", table->names[which]);
    }
    standard_counterpoint_build_rule_pair(
        name, standard_counterpoint_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        table->evaluate, table->score, &table->pairs[which]);
}

static void ensure_built(const StandardCounterpointRuleTable *table, int which) {
    if (!standard_counterpoint_rule_table_valid(table, which)) {
        return;
    }
    BuildPairArg arg = {table, which};
    ursatz_once(&table->built[which], build_pair, &arg);
}

const Rule *standard_counterpoint_rule_table_rule(const StandardCounterpointRuleTable *table, int which) {
    ensure_built(table, which);
    return standard_counterpoint_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_rule_table_constraint(const StandardCounterpointRuleTable *table, int which) {
    ensure_built(table, which);
    return standard_counterpoint_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const Preference *standard_counterpoint_rule_table_preference(const StandardCounterpointRuleTable *table, int which) {
    ensure_built(table, which);
    return standard_counterpoint_rule_table_valid(table, which) ? table->pairs[which].preference : NULL;
}

const char *standard_counterpoint_rule_table_name(const StandardCounterpointRuleTable *table, int which) {
    return standard_counterpoint_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void standard_counterpoint_rule_table_check(const StandardCounterpointRuleTable *table,
                                            StandardCounterpointRuleHolds holds, const void *subject,
                                            unsigned *hard_violations, unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (standard_counterpoint_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}

bool standard_counterpoint_rule_table_candidate_type(const StandardCounterpointRuleTable *table, char *out,
                                                     size_t capacity) {
    if (table == NULL || table->prefix == NULL || out == NULL) {
        return false;
    }
    int written = snprintf(out, capacity, "standard-counterpoint/%s", table->prefix);
    return written > 0 && (size_t)written < capacity;
}

ConstraintEvalResult standard_counterpoint_rule_table_evaluate(const StandardCounterpointRuleTable *table,
                                                               StandardCounterpointRuleHolds holds,
                                                               size_t subject_size, const Constraint *constraint,
                                                               const Value *candidate) {
    if (table == NULL || holds == NULL || constraint == NULL || candidate == NULL) {
        return CONSTRAINT_EVAL_ERR_NULL_CANDIDATE;
    }
    int which = -1;
    for (int i = 0; i < table->count; i++) {
        if (ursatz_once_is_done(&table->built[i]) && table->pairs[i].constraint == constraint) {
            which = i;
            break;
        }
    }
    if (which < 0) {
        return CONSTRAINT_EVAL_NOT_IMPLEMENTED;
    }

    char tag[96];
    if (!standard_counterpoint_rule_table_candidate_type(table, tag, sizeof(tag))) {
        return CONSTRAINT_EVAL_NOT_IMPLEMENTED;
    }
    if (strcmp(value_type(candidate), tag) != 0) {
        return CONSTRAINT_EVAL_NOT_APPLICABLE;
    }
    size_t size = 0;
    const void *subject = value_content(candidate, &size);
    if (subject == NULL || size != subject_size) {
        return CONSTRAINT_EVAL_ERR_NULL_CANDIDATE;
    }
    return holds(which, subject) ? CONSTRAINT_EVAL_PASS : CONSTRAINT_EVAL_FAIL;
}

PreferenceScore standard_counterpoint_rule_table_score(const StandardCounterpointRuleTable *table,
                                                       StandardCounterpointRuleHolds holds, size_t subject_size,
                                                       const Preference *preference, const Value *candidate) {
    PreferenceScore unscorable = {0, 0};
    if (table == NULL || holds == NULL || preference == NULL || candidate == NULL) {
        return unscorable;
    }
    int which = -1;
    for (int i = 0; i < table->count; i++) {
        if (ursatz_once_is_done(&table->built[i]) && table->pairs[i].preference == preference) {
            which = i;
            break;
        }
    }
    char tag[96];
    if (which < 0 || !standard_counterpoint_rule_table_candidate_type(table, tag, sizeof(tag)) ||
        strcmp(value_type(candidate), tag) != 0) {
        return unscorable;
    }
    size_t size = 0;
    const void *subject = value_content(candidate, &size);
    if (subject == NULL || size != subject_size) {
        return unscorable;
    }
    PreferenceScore score = {holds(which, subject) ? 1UL : 0UL, 1UL};
    return score;
}
