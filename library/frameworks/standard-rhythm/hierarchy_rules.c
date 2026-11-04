#include "hierarchy_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_HIERARCHY_RULE_COUNT] = {"hierarchy-halving", "hierarchy-count"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_HIERARCHY_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_rhythm_hierarchy, StandardRhythmHierarchyRule, STANDARD_RHYTHM_HIERARCHY_RULE_COUNT, g_table)

bool standard_rhythm_halved(const NoteValue *note_value, NoteValue **out) {
    if (note_value == NULL || note_value_dots(note_value) != 0) {
        return false;
    }
    return note_value_create(note_value_exponent(note_value) + 1, out) == NOTE_VALUE_OK;
}

bool standard_rhythm_count_within(const NoteValue *larger, const NoteValue *smaller, unsigned long *out_count) {
    if (larger == NULL || smaller == NULL || note_value_dots(larger) != 0 || note_value_dots(smaller) != 0) {
        return false;
    }
    int steps = note_value_exponent(smaller) - note_value_exponent(larger);
    if (steps < 0) {
        return false;
    }
    *out_count = 1UL << steps;
    return true;
}
