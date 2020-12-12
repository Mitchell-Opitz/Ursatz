#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "minor_scale_degree_rules.h"
#include "scale_degree_rules.h"
#include "solfege_rules.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SOLFEGE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_solfege_rule((StandardTonalitySolfegeRule)i);
        const Constraint *constraint = standard_tonality_solfege_constraint((StandardTonalitySolfegeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_solfege_rule(STANDARD_TONALITY_SOLFEGE_RULE_COUNT) == NULL);
}

/* SYLLABLE: the whole table. */
static void test_syllables(void) {
    const char *expected[7][3] = {
        {NULL, "do", "di"},  {"ra", "re", "ri"}, {"me", "mi", NULL}, {NULL, "fa", "fi"},
        {"se", "sol", "si"}, {"le", "la", "li"}, {"te", "ti", NULL},
    };
    for (int degree = 1; degree <= 7; degree++) {
        for (int alteration = -1; alteration <= 1; alteration++) {
            const char *syllable = standard_tonality_chromatic_solfege(degree, alteration);
            const char *want = expected[degree - 1][alteration + 1];
            if (want == NULL) {
                assert(syllable == NULL);
            } else {
                assert(syllable != NULL && strcmp(syllable, want) == 0);
            }
        }
    }
}

static void test_invalid_degree_or_alteration(void) {
    assert(standard_tonality_chromatic_solfege(0, 0) == NULL);
    assert(standard_tonality_chromatic_solfege(8, 0) == NULL);
    assert(standard_tonality_chromatic_solfege(-1, 0) == NULL);
    assert(standard_tonality_chromatic_solfege(1, 2) == NULL);
    assert(standard_tonality_chromatic_solfege(1, -2) == NULL);
}

/* The chromatic table agrees with the major and minor solfege already
   defined: unaltered degrees are major's, and the lowered 3, 6, 7 are
   minor's me, le, te. */
static void test_agrees_with_major_and_minor_solfege(void) {
    for (int degree = 1; degree <= 7; degree++) {
        assert(strcmp(standard_tonality_chromatic_solfege(degree, 0), standard_tonality_solfege(degree)) == 0);
    }
    assert(strcmp(standard_tonality_chromatic_solfege(3, -1),
                  standard_tonality_minor_solfege(STANDARD_TONALITY_MINOR_NATURAL, 3)) == 0);
    assert(strcmp(standard_tonality_chromatic_solfege(6, -1),
                  standard_tonality_minor_solfege(STANDARD_TONALITY_MINOR_NATURAL, 6)) == 0);
    assert(strcmp(standard_tonality_chromatic_solfege(7, -1),
                  standard_tonality_minor_solfege(STANDARD_TONALITY_MINOR_NATURAL, 7)) == 0);
}

/* RAISED_ENDING / LOWERED_ENDING: raised syllables end in -i, lowered in
   -e, ra being the exception. mi and ti end in -i but are unaltered. */
static void test_ending_rules(void) {
    int raised = 0;
    int lowered = 0;
    for (int degree = 1; degree <= 7; degree++) {
        for (int alteration = -1; alteration <= 1; alteration += 2) {
            if (standard_tonality_chromatic_solfege(degree, alteration) == NULL) {
                assert(!standard_tonality_solfege_follows_ending_rule(degree, alteration));
                continue;
            }
            assert(standard_tonality_solfege_follows_ending_rule(degree, alteration));
            if (alteration > 0) {
                raised++;
            } else {
                lowered++;
            }
        }
    }
    assert(raised == 5); /* di ri fi si li */
    assert(lowered == 5); /* ra me se le te */

    assert(strcmp(standard_tonality_chromatic_solfege(2, -1), "ra") == 0); /* the exception */
    assert(!standard_tonality_solfege_follows_ending_rule(1, 0));          /* unaltered has no ending rule */
    assert(!standard_tonality_solfege_follows_ending_rule(0, 1));
    assert(!standard_tonality_solfege_follows_ending_rule(1, 2));
}

static void test_parse(void) {
    for (int degree = 1; degree <= 7; degree++) {
        for (int alteration = -1; alteration <= 1; alteration++) {
            const char *syllable = standard_tonality_chromatic_solfege(degree, alteration);
            if (syllable == NULL) {
                continue;
            }
            int parsed_degree = 0;
            int parsed_alteration = 99;
            assert(standard_tonality_solfege_parse(syllable, &parsed_degree, &parsed_alteration));
            assert(parsed_degree == degree && parsed_alteration == alteration);
        }
    }

    int degree = 42;
    int alteration = 42;
    assert(!standard_tonality_solfege_parse("xx", &degree, &alteration));
    assert(!standard_tonality_solfege_parse(NULL, &degree, &alteration));
    assert(!standard_tonality_solfege_parse("do", NULL, &alteration));
    assert(!standard_tonality_solfege_parse("do", &degree, NULL));
    assert(degree == 42 && alteration == 42); /* untouched */
}

int main(void) {
    test_rules_reference_framework();
    test_syllables();
    test_invalid_degree_or_alteration();
    test_agrees_with_major_and_minor_solfege();
    test_ending_rules();
    test_parse();
    printf("test_solfege_rules (standard-tonality): ok\n");
    return 0;
}
