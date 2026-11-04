#include "solfege_rules.h"

#include <stddef.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_SOLFEGE_RULE_COUNT] = {"solfege-syllable", "solfege-raised-ending",
                                                                         "solfege-lowered-ending"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_SOLFEGE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_solfege, StandardTonalitySolfegeRule, STANDARD_TONALITY_SOLFEGE_RULE_COUNT, g_table)

/* Indexed [degree - 1][alteration + 1]. */
static const char *const SYLLABLES[7][3] = {
    {NULL, "do", "di"},   {"ra", "re", "ri"}, {"me", "mi", NULL}, {NULL, "fa", "fi"},
    {"se", "sol", "si"},  {"le", "la", "li"}, {"te", "ti", NULL},
};

const char *standard_tonality_chromatic_solfege(int degree, int alteration) {
    if (degree < 1 || degree > 7 || alteration < -1 || alteration > 1) {
        return NULL;
    }
    return SYLLABLES[degree - 1][alteration + 1];
}

bool standard_tonality_solfege_follows_ending_rule(int degree, int alteration) {
    const char *syllable = standard_tonality_chromatic_solfege(degree, alteration);
    if (syllable == NULL || alteration == 0) {
        return false;
    }
    size_t length = strlen(syllable);
    char last = syllable[length - 1];
    if (alteration > 0) {
        return last == 'i';
    }
    return last == 'e' || strcmp(syllable, "ra") == 0;
}

bool standard_tonality_solfege_parse(const char *syllable, int *degree, int *alteration) {
    if (syllable == NULL || degree == NULL || alteration == NULL) {
        return false;
    }
    for (int d = 0; d < 7; d++) {
        for (int a = 0; a < 3; a++) {
            if (SYLLABLES[d][a] != NULL && strcmp(SYLLABLES[d][a], syllable) == 0) {
                *degree = d + 1;
                *alteration = a - 1;
                return true;
            }
        }
    }
    return false;
}
