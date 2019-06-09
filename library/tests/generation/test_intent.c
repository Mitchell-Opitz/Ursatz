#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/intent.h"
#include "identity/entity_id.h"

/*
 * Construction test: Intent compiles a natural-language request into
 * structured fields -- purpose/target are required, style/context/
 * meter/length/constraints/preferences/source_material are optional.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Value *make_constraint(const char *content) {
    Value *value;
    assert(value_create("RawConstraint", content, strlen(content) + 1, &value) == VALUE_OK);
    return value;
}

int main(void) {
    EntityID *id = make_id("intent-counter-melody");
    EntityID *context_id = make_id("context-verse-1");
    EntityID *source_material_id = make_id("structure-theme-a");
    Value *constraints[2] = {make_constraint("stay within the vocal range"), make_constraint("avoid parallel fifths")};
    Value *preferences[1] = {make_constraint("prefer stepwise motion")};

    Intent *intent;
    assert(intent_create(id, "counter-melody", "8-measure phrase in the soprano voice", "lyrical", context_id, "4/4",
                          "8 measures", constraints, 2, preferences, 1, source_material_id, make_absent_provenance(),
                          &intent) == INTENT_OK);

    assert(strcmp(intent_purpose(intent), "counter-melody") == 0);
    assert(strcmp(intent_target(intent), "8-measure phrase in the soprano voice") == 0);
    assert(strcmp(intent_style(intent), "lyrical") == 0);
    assert(entity_id_equals(intent_context_id(intent), context_id));
    assert(strcmp(intent_meter(intent), "4/4") == 0);
    assert(strcmp(intent_length(intent), "8 measures") == 0);
    assert(intent_constraint_count(intent) == 2);
    assert(intent_constraint_at(intent, 0) == constraints[0]);
    assert(intent_constraint_at(intent, 1) == constraints[1]);
    assert(intent_preference_count(intent) == 1);
    assert(intent_preference_at(intent, 0) == preferences[0]);
    assert(entity_id_equals(intent_source_material_id(intent), source_material_id));

    /* Minimal Intent: only the required fields, everything optional omitted. */
    EntityID *minimal_id = make_id("intent-minimal");
    Intent *minimal;
    assert(intent_create(minimal_id, "warm-up exercise", "4-measure scalar passage", NULL, NULL, NULL, NULL, NULL, 0,
                          NULL, 0, NULL, make_absent_provenance(), &minimal) == INTENT_OK);
    assert(intent_style(minimal) == NULL);
    assert(intent_context_id(minimal) == NULL);
    assert(intent_meter(minimal) == NULL);
    assert(intent_length(minimal) == NULL);
    assert(intent_constraint_count(minimal) == 0);
    assert(intent_preference_count(minimal) == 0);
    assert(intent_source_material_id(minimal) == NULL);

    assert(intent_identity_equals(intent, intent));
    assert(!intent_identity_equals(intent, minimal));

    EntityID *no_purpose_id = make_id("intent-rejects-no-purpose");
    Intent *no_purpose;
    assert(intent_create(no_purpose_id, NULL, "target", NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL,
                          make_absent_provenance(), &no_purpose) == INTENT_ERR_NULL_PURPOSE);
    entity_id_destroy(no_purpose_id);

    EntityID *no_target_id = make_id("intent-rejects-no-target");
    Intent *no_target;
    assert(intent_create(no_target_id, "purpose", NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL,
                          make_absent_provenance(), &no_target) == INTENT_ERR_NULL_TARGET);
    entity_id_destroy(no_target_id);

    EntityID *null_entry_id = make_id("intent-rejects-null-constraint-entry");
    Value *bad_constraints[2] = {make_constraint("ok"), NULL};
    Intent *null_entry;
    assert(intent_create(null_entry_id, "purpose", "target", NULL, NULL, NULL, NULL, bad_constraints, 2, NULL, 0,
                          NULL, make_absent_provenance(), &null_entry) == INTENT_ERR_NULL_CONSTRAINT);
    entity_id_destroy(null_entry_id);
    value_destroy(bad_constraints[0]);

    intent_destroy(intent);
    intent_destroy(minimal);

    printf("test_intent: ok\n");
    return 0;
}