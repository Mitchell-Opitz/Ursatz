#include "hierarchy_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_HIERARCHY_RULE_COUNT] = {"hierarchy-halving", "hierarchy-count"};

static StandardRhythmRulePair g_pairs[STANDARD_RHYTHM_HIERARCHY_RULE_COUNT];
static bool g_built[STANDARD_RHYTHM_HIERARCHY_RULE_COUNT];

static bool valid_rule(StandardRhythmHierarchyRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_HIERARCHY_RULE_COUNT;
}

static void ensure_built(StandardRhythmHierarchyRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_rhythm_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_rhythm_hierarchy_rule(StandardRhythmHierarchyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_rhythm_hierarchy_constraint(StandardRhythmHierarchyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

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
