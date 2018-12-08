#include "core/provenance_reference.h"
#include "core/relationship_reference.h"
#include "core/voice_reference.h"

#include <assert.h>
#include <stddef.h>

static void test_voice_reference_rejects_null_target(void) {
    VoiceReference *reference = NULL;
    VoiceReferenceResult r = voice_reference_create(NULL, &reference);
    assert(r == VOICE_REFERENCE_ERR_NULL_TARGET_ID);
    assert(reference == NULL);
}

static void test_voice_reference_holds_target_id_only(void) {
    EntityID *target_id = NULL;
    entity_id_create("voice-1", &target_id);

    VoiceReference *reference = NULL;
    voice_reference_create(target_id, &reference);

    assert(entity_id_equals(voice_reference_target_id(reference), target_id));

    voice_reference_destroy(reference);
}

static void test_voice_reference_equals_on_target_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("voice-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("voice-1", &id_b);
    EntityID *id_c = NULL;
    entity_id_create("voice-2", &id_c);

    VoiceReference *a = NULL;
    voice_reference_create(id_a, &a);
    VoiceReference *b = NULL;
    voice_reference_create(id_b, &b);
    VoiceReference *c = NULL;
    voice_reference_create(id_c, &c);

    assert(voice_reference_equals(a, b));
    assert(!voice_reference_equals(a, c));

    voice_reference_destroy(a);
    voice_reference_destroy(b);
    voice_reference_destroy(c);
}

static void test_relationship_reference_rejects_null_target(void) {
    RelationshipReference *reference = NULL;
    RelationshipReferenceResult r = relationship_reference_create(NULL, &reference);
    assert(r == RELATIONSHIP_REFERENCE_ERR_NULL_TARGET_ID);
    assert(reference == NULL);
}

static void test_relationship_reference_equals_on_target_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("relationship-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("relationship-1", &id_b);
    EntityID *id_c = NULL;
    entity_id_create("relationship-2", &id_c);

    RelationshipReference *a = NULL;
    relationship_reference_create(id_a, &a);
    RelationshipReference *b = NULL;
    relationship_reference_create(id_b, &b);
    RelationshipReference *c = NULL;
    relationship_reference_create(id_c, &c);

    assert(relationship_reference_equals(a, b));
    assert(!relationship_reference_equals(a, c));

    relationship_reference_destroy(a);
    relationship_reference_destroy(b);
    relationship_reference_destroy(c);
}

static void test_provenance_reference_present_rejects_null_target(void) {
    ProvenanceReference *reference = NULL;
    ProvenanceReferenceResult r = provenance_reference_create_present(NULL, &reference);
    assert(r == PROVENANCE_REFERENCE_ERR_NULL_TARGET_ID);
    assert(reference == NULL);
}

static void test_provenance_reference_present_holds_target_id(void) {
    EntityID *target_id = NULL;
    entity_id_create("derivation-1", &target_id);

    ProvenanceReference *reference = NULL;
    provenance_reference_create_present(target_id, &reference);

    assert(provenance_reference_state(reference) == PROVENANCE_REFERENCE_PRESENT);
    assert(entity_id_equals(provenance_reference_target_id(reference), target_id));

    provenance_reference_destroy(reference);
}

/* Explicitly-absent is a distinct, deliberate state, not a null/omitted
   value: it has no target id and is constructed by its own function. */
static void test_provenance_reference_absent_has_no_target_id(void) {
    ProvenanceReference *reference = NULL;
    provenance_reference_create_absent(&reference);

    assert(provenance_reference_state(reference) == PROVENANCE_REFERENCE_ABSENT);
    assert(provenance_reference_target_id(reference) == NULL);

    provenance_reference_destroy(reference);
}

static void test_provenance_reference_equals_present_same_target(void) {
    EntityID *id_a = NULL;
    entity_id_create("derivation-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("derivation-1", &id_b);

    ProvenanceReference *a = NULL;
    provenance_reference_create_present(id_a, &a);
    ProvenanceReference *b = NULL;
    provenance_reference_create_present(id_b, &b);

    assert(provenance_reference_equals(a, b));

    provenance_reference_destroy(a);
    provenance_reference_destroy(b);
}

static void test_provenance_reference_absent_equals_absent(void) {
    ProvenanceReference *a = NULL;
    provenance_reference_create_absent(&a);
    ProvenanceReference *b = NULL;
    provenance_reference_create_absent(&b);

    assert(provenance_reference_equals(a, b));

    provenance_reference_destroy(a);
    provenance_reference_destroy(b);
}

static void test_provenance_reference_present_not_equal_to_absent(void) {
    EntityID *target_id = NULL;
    entity_id_create("derivation-1", &target_id);

    ProvenanceReference *present = NULL;
    provenance_reference_create_present(target_id, &present);
    ProvenanceReference *absent = NULL;
    provenance_reference_create_absent(&absent);

    assert(!provenance_reference_equals(present, absent));

    provenance_reference_destroy(present);
    provenance_reference_destroy(absent);
}

int main(void) {
    test_voice_reference_rejects_null_target();
    test_voice_reference_holds_target_id_only();
    test_voice_reference_equals_on_target_id();
    test_relationship_reference_rejects_null_target();
    test_relationship_reference_equals_on_target_id();
    test_provenance_reference_present_rejects_null_target();
    test_provenance_reference_present_holds_target_id();
    test_provenance_reference_absent_has_no_target_id();
    test_provenance_reference_equals_present_same_target();
    test_provenance_reference_absent_equals_absent();
    test_provenance_reference_present_not_equal_to_absent();
    return 0;
}