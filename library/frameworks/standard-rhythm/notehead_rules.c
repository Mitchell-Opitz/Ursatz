#include "notehead_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_NOTEHEAD_RULE_COUNT] = {"notehead-filled", "notehead-stems",
                                                                        "notehead-flags", "notehead-halving"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_NOTEHEAD_RULE_COUNT);

static bool valid_rule(StandardRhythmNoteheadRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_NOTEHEAD_RULE_COUNT;
}

const Rule *standard_rhythm_notehead_rule(StandardRhythmNoteheadRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_rhythm_notehead_constraint(StandardRhythmNoteheadRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

#define QUARTER_EXPONENT 2

bool standard_rhythm_notehead_filled(const NoteValue *note_value) {
    return note_value != NULL && note_value_exponent(note_value) >= QUARTER_EXPONENT;
}

StemRequirement standard_rhythm_stem_requirement(const NoteValue *note_value) {
    return standard_rhythm_notehead_filled(note_value) ? STEM_REQUIREMENT_REQUIRED : STEM_REQUIREMENT_OPTIONAL;
}

bool standard_rhythm_stem_allowed(const NoteValue *note_value, bool has_stem) {
    if (note_value == NULL) {
        return false;
    }
    return has_stem || standard_rhythm_stem_requirement(note_value) == STEM_REQUIREMENT_OPTIONAL;
}

int standard_rhythm_flag_count(const NoteValue *note_value) {
    if (note_value == NULL || note_value_exponent(note_value) <= QUARTER_EXPONENT) {
        return 0;
    }
    return note_value_exponent(note_value) - QUARTER_EXPONENT;
}

bool standard_rhythm_flags_allowed(const NoteValue *note_value, bool has_stem, int flags) {
    if (note_value == NULL) {
        return false;
    }
    if (!has_stem) {
        return flags == 0;
    }
    return flags == standard_rhythm_flag_count(note_value);
}

HalvingStep standard_rhythm_halving_step(const NoteValue *note_value) {
    if (note_value == NULL) {
        return HALVING_STEP_NONE;
    }
    int exponent = note_value_exponent(note_value);
    if (exponent < 0) {
        return HALVING_STEP_NONE;
    }
    if (exponent == 0) {
        return HALVING_STEP_ADD_STEM;
    }
    if (exponent == 1) {
        return HALVING_STEP_FILL_NOTEHEAD;
    }
    return HALVING_STEP_ADD_FLAG;
}
