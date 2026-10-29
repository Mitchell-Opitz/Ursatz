#include "name_rules.h"

#include <ctype.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_NAME_RULE_COUNT] = {"note-names"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_NAME_RULE_COUNT);

static bool valid_rule(StandardRhythmNameRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_NAME_RULE_COUNT;
}

const Rule *standard_rhythm_name_rule(StandardRhythmNameRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_rhythm_name_constraint(StandardRhythmNameRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

/* Indexed by exponent + 1 (breve first). */
#define NAMED_COUNT (STANDARD_RHYTHM_MAX_NAMED_EXPONENT + 2)

static const char *const AMERICAN[NAMED_COUNT] = {"double whole note", "whole note",         "half note",
                                                  "quarter note",      "eighth note",        "sixteenth note",
                                                  "thirty-second note", "sixty-fourth note"};

static const char *const BRITISH[NAMED_COUNT] = {"breve", "semibreve",     "minim",         "crotchet",
                                                 "quaver", "semiquaver",   "demisemiquaver", "hemidemisemiquaver"};

const char *standard_rhythm_note_name(const NoteValue *note_value, StandardRhythmNaming naming) {
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

static bool equals_ignoring_case(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

bool standard_rhythm_note_exponent_from_name(const char *name, int *out_exponent) {
    if (name == NULL) {
        return false;
    }
    for (int i = 0; i < NAMED_COUNT; i++) {
        if (equals_ignoring_case(name, AMERICAN[i]) || equals_ignoring_case(name, BRITISH[i])) {
            *out_exponent = i - 1;
            return true;
        }
    }
    return false;
}
