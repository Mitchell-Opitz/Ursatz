#include "rest_rules.h"

#include "hierarchy_rules.h"
#include "notehead_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_REST_RULE_COUNT] = {"rest-names", "rest-flags", "rest-hierarchy"};

static StandardRhythmRulePair g_pairs[STANDARD_RHYTHM_REST_RULE_COUNT];
static bool g_built[STANDARD_RHYTHM_REST_RULE_COUNT];

static bool valid_rule(StandardRhythmRestRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_REST_RULE_COUNT;
}

static void ensure_built(StandardRhythmRestRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_rhythm_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_rhythm_rest_rule(StandardRhythmRestRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_rhythm_rest_constraint(StandardRhythmRestRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

/* Indexed by exponent + 1 (breve first). */
#define NAMED_COUNT (STANDARD_RHYTHM_MAX_NAMED_EXPONENT + 2)

static const char *const AMERICAN[NAMED_COUNT] = {"breve rest",        "whole rest",         "half rest",
                                                  "quarter rest",      "eighth rest",        "sixteenth rest",
                                                  "thirty-second rest", "sixty-fourth rest"};

static const char *const BRITISH[NAMED_COUNT] = {"breve rest",   "semibreve rest",      "minim rest",
                                                 "crotchet rest", "quaver rest",        "semiquaver rest",
                                                 "demisemiquaver rest", "hemidemisemiquaver rest"};

const char *standard_rhythm_rest_name(const NoteValue *note_value, StandardRhythmNaming naming) {
    if (note_value == NULL) {
        return NULL;
    }
    int exponent = note_value_exponent(note_value);
    if (exponent > STANDARD_RHYTHM_MAX_NAMED_EXPONENT) {
        return NULL;
    }
    switch (naming) {
    case STANDARD_RHYTHM_NAMING_AMERICAN:
        return AMERICAN[exponent + 1];
    case STANDARD_RHYTHM_NAMING_BRITISH:
        return BRITISH[exponent + 1];
    }
    return NULL;
}

int standard_rhythm_rest_flag_count(const NoteValue *note_value) {
    return standard_rhythm_flag_count(note_value);
}

bool standard_rhythm_rest_count_within(const NoteValue *larger, const NoteValue *smaller, unsigned long *out_count) {
    return standard_rhythm_count_within(larger, smaller, out_count);
}
