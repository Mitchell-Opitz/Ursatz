#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "theory/heuristic.h"

/*
 * Construction test: a Heuristic holds identity and a label.
 *
 * Structural test: heuristic.h exposes no evaluate/pass-fail API --
 * confirming (by absence) that a Heuristic cannot be treated as
 * authoritative the way Rule/Constraint are (TDD §98).
 */

int main(void) {
    EntityID *id;
    assert(entity_id_create("heuristic-prefer-stepwise-motion", &id) == ENTITY_ID_OK);

    Heuristic *heuristic;
    assert(heuristic_create(id, "prefer stepwise motion", &heuristic) == HEURISTIC_OK);
    assert(strcmp(heuristic_label(heuristic), "prefer stepwise motion") == 0);

    EntityID *other_id;
    assert(entity_id_create("heuristic-other", &other_id) == ENTITY_ID_OK);
    Heuristic *other;
    assert(heuristic_create(other_id, "other", &other) == HEURISTIC_OK);

    assert(heuristic_identity_equals(heuristic, heuristic));
    assert(!heuristic_identity_equals(heuristic, other));

    heuristic_destroy(heuristic);
    heuristic_destroy(other);

    printf("test_heuristic: ok\n");
    return 0;
}