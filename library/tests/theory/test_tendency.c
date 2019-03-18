#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/tendency.h"

/*
 * Construction test: a Tendency holds identity, label, frequency
 * fraction, and optional observed-data.
 *
 * Structural test: TendencyResult shares no values with RuleResult
 * (RULE_OK == 0) or ConstraintResult/ConstraintEvalResult
 * (CONSTRAINT_OK == 0, CONSTRAINT_EVAL_PASS == 0) beyond the
 * coincidental integer 0 every such enum starts at -- there is no
 * function anywhere in tendency.h that returns a RuleResult,
 * ConstraintResult, or ConstraintEvalResult, and no function in
 * rule.h/constraint.h that accepts or returns a Tendency or
 * TendencyResult. A Tendency cannot be coerced into a Rule/Constraint
 * result type because no such conversion path exists in either
 * direction.
 */

int main(void) {
    Value *observed;
    assert(value_create("corpus-frequency-sample", "bach-chorales-v1", 17, &observed) == VALUE_OK);

    EntityID *id;
    assert(entity_id_create("tendency-leading-tone-resolves-up", &id) == ENTITY_ID_OK);

    Tendency *tendency;
    assert(tendency_create(id, "leading tone resolves upward", 9, 10, observed, &tendency) == TENDENCY_OK);
    assert(strcmp(tendency_label(tendency), "leading tone resolves upward") == 0);
    assert(tendency_frequency_numerator(tendency) == 9);
    assert(tendency_frequency_denominator(tendency) == 10);
    assert(tendency_observed_data(tendency) == observed);

    EntityID *bad_id;
    assert(entity_id_create("tendency-bad-frequency", &bad_id) == ENTITY_ID_OK);
    Tendency *bad;
    assert(tendency_create(bad_id, "bad", 11, 10, NULL, &bad) == TENDENCY_ERR_FREQUENCY_OUT_OF_RANGE);
    entity_id_destroy(bad_id);

    EntityID *other_id;
    assert(entity_id_create("tendency-other", &other_id) == ENTITY_ID_OK);
    Tendency *other;
    assert(tendency_create(other_id, "other", 1, 2, NULL, &other) == TENDENCY_OK);

    assert(tendency_identity_equals(tendency, tendency));
    assert(!tendency_identity_equals(tendency, other));

    tendency_destroy(tendency);
    tendency_destroy(other);

    printf("test_tendency: ok\n");
    return 0;
}