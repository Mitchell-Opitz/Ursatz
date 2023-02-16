#include <assert.h>
#include <stdio.h>

#include "cadential_bass_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_cadential_bass_rule, standard_form_cadential_bass_constraint,
                                    STANDARD_FORM_CADENTIAL_BASS_RULE_COUNT);
}

/* The core bass line is mi fa sol do. */
static void test_core_line(void) {
    assert(standard_form_cadential_core_length() == 4);
    assert(standard_form_cadential_core_degree(0) == 3);
    assert(standard_form_cadential_core_degree(1) == 4);
    assert(standard_form_cadential_core_degree(2) == 5);
    assert(standard_form_cadential_core_degree(3) == 1);
    assert(standard_form_cadential_core_degree(4) == 0);
}

static void test_bass_is_cadential(void) {
    StandardFormBassNote core[4] = {{3, 0}, {4, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote with_fi[5] = {{3, 0}, {4, 0}, {4, 1}, {5, 0}, {1, 0}};
    StandardFormBassNote no_fi_raise[5] = {{3, 0}, {4, 0}, {4, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote wrong_degree[4] = {{3, 0}, {4, 0}, {6, 0}, {1, 0}};
    StandardFormBassNote raised_core[4] = {{3, 0}, {4, 1}, {5, 0}, {1, 0}};
    StandardFormBassNote short_line[3] = {{3, 0}, {5, 0}, {1, 0}};

    assert(standard_form_bass_is_cadential(core, 4));
    assert(standard_form_bass_is_cadential(with_fi, 5));
    assert(!standard_form_bass_is_cadential(no_fi_raise, 5));
    assert(!standard_form_bass_is_cadential(wrong_degree, 4));
    assert(!standard_form_bass_is_cadential(raised_core, 4));
    assert(!standard_form_bass_is_cadential(short_line, 3));
    assert(!standard_form_bass_is_cadential(NULL, 4));
    assert(!standard_form_bass_is_cadential(core, 0));
}

/* fi tonicizes the dominant: mi fa fi sol do. */
static void test_fi_embellishment(void) {
    StandardFormBassNote core[4] = {{3, 0}, {4, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote with_fi[5] = {{3, 0}, {4, 0}, {4, 1}, {5, 0}, {1, 0}};

    assert(standard_form_bass_has_fi_embellishment(with_fi, 5));
    assert(!standard_form_bass_has_fi_embellishment(core, 4));
    assert(!standard_form_bass_has_fi_embellishment(NULL, 5));
}

static void test_harmonization(void) {
    assert(standard_form_cadential_chord_allowed(0, STANDARD_FORM_CADENTIAL_CHORD_I6));
    assert(!standard_form_cadential_chord_allowed(0, STANDARD_FORM_CADENTIAL_CHORD_I));

    assert(standard_form_cadential_chord_allowed(1, STANDARD_FORM_CADENTIAL_CHORD_II6));
    assert(standard_form_cadential_chord_allowed(1, STANDARD_FORM_CADENTIAL_CHORD_IV));
    assert(!standard_form_cadential_chord_allowed(1, STANDARD_FORM_CADENTIAL_CHORD_V));

    assert(standard_form_cadential_chord_allowed(2, STANDARD_FORM_CADENTIAL_CHORD_V));
    assert(standard_form_cadential_chord_allowed(2, STANDARD_FORM_CADENTIAL_CHORD_V7));
    assert(!standard_form_cadential_chord_allowed(2, STANDARD_FORM_CADENTIAL_CHORD_IV));

    assert(standard_form_cadential_chord_allowed(3, STANDARD_FORM_CADENTIAL_CHORD_I));
    assert(!standard_form_cadential_chord_allowed(3, STANDARD_FORM_CADENTIAL_CHORD_I6));

    assert(!standard_form_cadential_chord_allowed(4, STANDARD_FORM_CADENTIAL_CHORD_I));

    assert(standard_form_cadential_six_four_elaborates(2));
    assert(!standard_form_cadential_six_four_elaborates(0));
    assert(!standard_form_cadential_six_four_elaborates(3));
}

static void test_clearest(void) {
    assert(standard_form_cadential_is_clearest(4, 4));
    assert(!standard_form_cadential_is_clearest(8, 4));
    assert(!standard_form_cadential_is_clearest(4, 5));
    assert(!standard_form_cadential_is_clearest(4, 2));
}

/* di-re-sol-do (a song) and fi-sol-do (a codetta) are variants of the core line. */
static void test_variants(void) {
    StandardFormBassNote di_re_sol_do[4] = {{1, 1}, {2, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote fi_sol_do[3] = {{4, 1}, {5, 0}, {1, 0}};
    StandardFormBassNote core[4] = {{3, 0}, {4, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote with_fi[5] = {{3, 0}, {4, 0}, {4, 1}, {5, 0}, {1, 0}};
    StandardFormBassNote do_unraised[4] = {{1, 0}, {2, 0}, {5, 0}, {1, 0}};
    StandardFormBassNote sol_do[2] = {{5, 0}, {1, 0}};

    assert(standard_form_bass_is_cadential_variant(di_re_sol_do, 4, STANDARD_FORM_CADENTIAL_VARIANT_DI_RE_SOL_DO));
    assert(!standard_form_bass_is_cadential_variant(di_re_sol_do, 4, STANDARD_FORM_CADENTIAL_VARIANT_FI_SOL_DO));
    assert(standard_form_bass_is_cadential_variant(fi_sol_do, 3, STANDARD_FORM_CADENTIAL_VARIANT_FI_SOL_DO));
    assert(!standard_form_bass_is_cadential_variant(fi_sol_do, 3, STANDARD_FORM_CADENTIAL_VARIANT_DI_RE_SOL_DO));
    assert(!standard_form_bass_is_cadential_variant(di_re_sol_do, 4, STANDARD_FORM_CADENTIAL_VARIANT_COUNT));
    assert(!standard_form_bass_is_cadential_variant(NULL, 4, STANDARD_FORM_CADENTIAL_VARIANT_DI_RE_SOL_DO));

    assert(standard_form_bass_is_any_cadential_variant(di_re_sol_do, 4));
    assert(standard_form_bass_is_any_cadential_variant(fi_sol_do, 3));

    /* the core line and its fi embellishment are not variants, and variants are not the core line */
    assert(!standard_form_bass_is_any_cadential_variant(core, 4));
    assert(!standard_form_bass_is_any_cadential_variant(with_fi, 5));
    assert(!standard_form_bass_is_any_cadential_variant(do_unraised, 4));
    assert(!standard_form_bass_is_any_cadential_variant(sol_do, 2));
    assert(!standard_form_bass_is_cadential(di_re_sol_do, 4));
    assert(!standard_form_bass_is_cadential(fi_sol_do, 3));
}

int main(void) {
    test_rules_reference_framework();
    test_core_line();
    test_bass_is_cadential();
    test_fi_embellishment();
    test_harmonization();
    test_clearest();
    test_variants();
    printf("test_cadential_bass_rules (standard-form): ok\n");
    return 0;
}
