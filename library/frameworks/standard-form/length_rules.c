#include "length_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_LENGTH_RULE_COUNT] = {
    "length-change",
    "length-placement",
    "length-expected",
    "length-balance"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_LENGTH_RULE_COUNT);

static bool valid_rule(StandardFormLengthRule which) {
    return which >= 0 && which < STANDARD_FORM_LENGTH_RULE_COUNT;
}

const Rule *standard_form_length_rule(StandardFormLengthRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_length_constraint(StandardFormLengthRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_form_classify_length_change(size_t expected, size_t actual, PhraseLengthChange *out) {
    if (expected == 0 || actual == 0 || expected == actual) {
        return false;
    }
    *out = actual > expected ? PHRASE_LENGTH_CHANGE_EXPANSION : PHRASE_LENGTH_CHANGE_CONTRACTION;
    return true;
}

bool standard_form_placement_allowed(PhraseLengthChange change, ExpansionPlacement placement) {
    if (!phrase_length_change_valid(change) || !expansion_placement_valid(placement)) {
        return false;
    }
    return change == PHRASE_LENGTH_CHANGE_EXPANSION || placement == EXPANSION_PLACEMENT_INTERNAL;
}

bool standard_form_expected_length_available(bool cadence_can_be_anticipated, bool unexpanded_model_stated) {
    return cadence_can_be_anticipated || unexpanded_model_stated;
}

size_t standard_form_expected_ending_length(size_t beginning_length) {
    return beginning_length;
}

bool standard_form_ending_length_change(size_t beginning_length, size_t ending_length, PhraseLengthChange *out) {
    return standard_form_classify_length_change(standard_form_expected_ending_length(beginning_length), ending_length,
                                                out);
}
