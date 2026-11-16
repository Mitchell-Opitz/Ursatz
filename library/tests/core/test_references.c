#include "core/framework_reference.h"
#include "core/provenance_reference.h"
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

static void test_framework_reference_rejects_null_target(void) {
    FrameworkReference *reference = NULL;
    FrameworkReferenceResult r = framework_reference_create(NULL, &reference);
    assert(r == FRAMEWORK_REFERENCE_ERR_NULL_TARGET_ID);
    assert(reference == NULL);
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

int main(void) {
    test_voice_reference_rejects_null_target();
    test_voice_reference_holds_target_id_only();
    test_framework_reference_rejects_null_target();
    test_provenance_reference_present_rejects_null_target();
    test_provenance_reference_present_holds_target_id();
    test_provenance_reference_absent_has_no_target_id();
    return 0;
}