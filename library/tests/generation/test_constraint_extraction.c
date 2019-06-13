#include <assert.h>
#include <stdio.h>

#include "generation/constraint_extraction.h"
#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "theory/framework.h"

/*
 * Construction test: ConstraintExtraction is the executable constraint
 * set an Intent normalizes into -- owned arrays of Constraint/
 * Preference, independent of each other and of any 1:1 count with the
 * Intent's own raw constraints/preferences.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Constraint *make_constraint(const char *id_value, const Framework *framework) {
    Constraint *constraint;
    assert(constraint_create(make_id(id_value), framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &constraint) ==
           CONSTRAINT_OK);
    return constraint;
}

static Preference *make_preference(const char *id_value, const Framework *framework) {
    Preference *preference;
    assert(preference_create(make_id(id_value), framework, &preference) == PREFERENCE_OK);
    return preference;
}

int main(void) {
    Framework *framework;
    assert(framework_create(make_id("framework-generation-test"), "generation-test", &framework) == FRAMEWORK_OK);

    Constraint *constraints[2] = {make_constraint("constraint-vocal-range", framework),
                                   make_constraint("constraint-no-parallel-fifths", framework)};
    Preference *preferences[1] = {make_preference("preference-stepwise-motion", framework)};

    ConstraintExtraction *extraction;
    assert(constraint_extraction_create(constraints, 2, preferences, 1, &extraction) == CONSTRAINT_EXTRACTION_OK);

    assert(constraint_extraction_constraint_count(extraction) == 2);
    assert(constraint_extraction_constraint_at(extraction, 0) == constraints[0]);
    assert(constraint_extraction_constraint_at(extraction, 1) == constraints[1]);
    assert(constraint_extraction_preference_count(extraction) == 1);
    assert(constraint_extraction_preference_at(extraction, 0) == preferences[0]);

    /* Empty ConstraintExtraction: an Intent with no constraints/preferences still normalizes validly. */
    ConstraintExtraction *empty;
    assert(constraint_extraction_create(NULL, 0, NULL, 0, &empty) == CONSTRAINT_EXTRACTION_OK);
    assert(constraint_extraction_constraint_count(empty) == 0);
    assert(constraint_extraction_preference_count(empty) == 0);

    /* Rejects a NULL entry in a non-empty constraints array. */
    Constraint *bad_constraints[2] = {make_constraint("constraint-ok", framework), NULL};
    ConstraintExtraction *rejected;
    assert(constraint_extraction_create(bad_constraints, 2, NULL, 0, &rejected) ==
           CONSTRAINT_EXTRACTION_ERR_NULL_CONSTRAINT);
    constraint_destroy(bad_constraints[0]);

    constraint_extraction_destroy(extraction);
    constraint_extraction_destroy(empty);
    framework_destroy(framework);

    printf("test_constraint_extraction: ok\n");
    return 0;
}