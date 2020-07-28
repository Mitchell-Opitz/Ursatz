#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dynamics_rules.h"
#include "framework.h"

static int rank_of(const char *marking) {
    int rank = 0;
    assert(standard_markings_dynamic_rank(marking, &rank));
    return rank;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_MARKINGS_DYNAMICS_RULE_COUNT; i++) {
        const Rule *rule = standard_markings_dynamics_rule((StandardMarkingsDynamicsRule)i);
        const Constraint *constraint = standard_markings_dynamics_constraint((StandardMarkingsDynamicsRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_markings_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_markings_dynamics_rule(STANDARD_MARKINGS_DYNAMICS_RULE_COUNT) == NULL);
}

/* Quietest to loudest: ppp pp p mp mf f ff fff. */
static void test_loudness_order(void) {
    const char *order[] = {"ppp", "pp", "p", "mp", "mf", "f", "ff", "fff"};
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            assert(standard_markings_dynamic_is_quieter(order[i], order[j]) == (i < j));
        }
    }
    assert(rank_of("ppp") == -4 && rank_of("pp") == -3 && rank_of("p") == -2 && rank_of("mp") == -1);
    assert(rank_of("mf") == 1 && rank_of("f") == 2 && rank_of("ff") == 3 && rank_of("fff") == 4);
}

static void test_full_names_match_abbreviations(void) {
    assert(rank_of("pianississimo") == rank_of("ppp"));
    assert(rank_of("pianissimo") == rank_of("pp"));
    assert(rank_of("piano") == rank_of("p"));
    assert(rank_of("mezzo piano") == rank_of("mp"));
    assert(rank_of("mezzo forte") == rank_of("mf"));
    assert(rank_of("forte") == rank_of("f"));
    assert(rank_of("fortissimo") == rank_of("ff"));
    assert(rank_of("fortississimo") == rank_of("fff"));
}

/* -issimo stacks without limit: pppppp and ffffff are legal. */
static void test_issimo_stacking(void) {
    assert(rank_of("pppp") == -5);
    assert(rank_of("pppppp") == -7);
    assert(rank_of("ffff") == 5);
    assert(rank_of("ffffff") == 7);
    assert(standard_markings_dynamic_is_quieter("pppppp", "ppp"));
    assert(standard_markings_dynamic_is_quieter("fff", "ffffff"));
}

/* mezzo means "moderately": between the quiet and loud sides. */
static void test_mezzo_is_moderate(void) {
    assert(standard_markings_dynamic_is_quieter("p", "mp"));
    assert(standard_markings_dynamic_is_quieter("mp", "mf"));
    assert(standard_markings_dynamic_is_quieter("mf", "f"));
    assert(!standard_markings_dynamic_is_quieter("mf", "mp"));
}

static void test_case_and_unrecognized(void) {
    assert(rank_of("FF") == 3);
    assert(rank_of("Mezzo Forte") == 1);
    assert(rank_of("mezzo-piano") == -1);

    int rank = 99;
    assert(!standard_markings_dynamic_rank("loud", &rank));
    assert(!standard_markings_dynamic_rank("", &rank));
    assert(!standard_markings_dynamic_rank(NULL, &rank));
    assert(!standard_markings_dynamic_rank("mpp", &rank));
    assert(!standard_markings_dynamic_rank("pf", &rank));
    assert(!standard_markings_dynamic_rank("mezzo", &rank));
    assert(rank == 99); /* untouched on failure */

    assert(standard_markings_is_dynamic_level("mf"));
    assert(!standard_markings_is_dynamic_level("cresc."));
    assert(!standard_markings_dynamic_is_quieter("loud", "ff"));
}

static void test_abbreviation(void) {
    char buffer[16];
    assert(standard_markings_dynamic_abbreviation("pianissimo", buffer, sizeof(buffer)));
    assert(strcmp(buffer, "pp") == 0);
    assert(standard_markings_dynamic_abbreviation("Mezzo Forte", buffer, sizeof(buffer)));
    assert(strcmp(buffer, "mf") == 0);
    assert(standard_markings_dynamic_abbreviation("fortississimo", buffer, sizeof(buffer)));
    assert(strcmp(buffer, "fff") == 0);
    assert(standard_markings_dynamic_abbreviation("pppppp", buffer, sizeof(buffer)));
    assert(strcmp(buffer, "pppppp") == 0);

    char small[3];
    assert(!standard_markings_dynamic_abbreviation("fff", small, sizeof(small))); /* needs 4 */
    assert(!standard_markings_dynamic_abbreviation("nonsense", buffer, sizeof(buffer)));
    assert(!standard_markings_dynamic_abbreviation("f", NULL, 0));
}

static void test_dynamic_change(void) {
    assert(standard_markings_dynamic_change("cresc.") == DYNAMIC_CHANGE_LOUDER);
    assert(standard_markings_dynamic_change("crescendo") == DYNAMIC_CHANGE_LOUDER);
    assert(standard_markings_dynamic_change("cresc. . . .") == DYNAMIC_CHANGE_LOUDER);
    assert(standard_markings_dynamic_change("<") == DYNAMIC_CHANGE_LOUDER);

    /* decrescendo and diminuendo both mean getting quieter. */
    assert(standard_markings_dynamic_change("decresc.") == DYNAMIC_CHANGE_QUIETER);
    assert(standard_markings_dynamic_change("decrescendo") == DYNAMIC_CHANGE_QUIETER);
    assert(standard_markings_dynamic_change("dim.") == DYNAMIC_CHANGE_QUIETER);
    assert(standard_markings_dynamic_change("diminuendo") == DYNAMIC_CHANGE_QUIETER);
    assert(standard_markings_dynamic_change(">") == DYNAMIC_CHANGE_QUIETER);

    assert(standard_markings_dynamic_change("f") == DYNAMIC_CHANGE_NONE);
    assert(standard_markings_dynamic_change("") == DYNAMIC_CHANGE_NONE);
    assert(standard_markings_dynamic_change(NULL) == DYNAMIC_CHANGE_NONE);
}

int main(void) {
    test_rules_reference_framework();
    test_loudness_order();
    test_full_names_match_abbreviations();
    test_issimo_stacking();
    test_mezzo_is_moderate();
    test_case_and_unrecognized();
    test_abbreviation();
    test_dynamic_change();
    printf("test_dynamics_rules (standard-markings): ok\n");
    return 0;
}
