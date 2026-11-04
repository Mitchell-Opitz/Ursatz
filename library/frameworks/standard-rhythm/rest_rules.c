#include "rest_rules.h"

#include "hierarchy_rules.h"
#include "notehead_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_REST_RULE_COUNT] = {"rest-names", "rest-flags", "rest-hierarchy"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_REST_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_rhythm_rest, StandardRhythmRestRule, STANDARD_RHYTHM_REST_RULE_COUNT, g_table)

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
