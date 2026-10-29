#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "core/entity.h"
#include "core/framework_reference.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "interpretation/uncertainty.h"

static Interpretation *make(double uncertainty) {
    int content = 7;
    return claim_interpretation_create("framework-demo", "demo", "demo-claim-1", "target-1", "DemoClaim", &content,
                                       sizeof(content), uncertainty, "demo-method");
}

static void check_confidence(double uncertainty, unsigned long numerator) {
    Interpretation *interpretation = make(uncertainty);
    assert(interpretation != NULL);
    const Uncertainty *confidence = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(confidence) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(confidence) == numerator);
    assert(uncertainty_scalar_confidence_denominator(confidence) == 1000000UL);
    interpretation_destroy(interpretation);
}

static void test_confidence_rounding(void) {
    check_confidence(0.0, 1000000UL);
    check_confidence(0.25, 750000UL);
    check_confidence(1.0 / 3.0, 666667UL);
    check_confidence(1.0, 0UL);
}

static void test_fields(void) {
    Interpretation *interpretation = make(0.25);
    assert(interpretation != NULL);
    assert(strcmp(entity_id_value(interpretation_id(interpretation)), "demo-claim-1") == 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "target-1") == 0);
    assert(strcmp(entity_id_value(framework_reference_target_id(interpretation_framework_reference(interpretation))),
                  "framework-demo") == 0);
    assert(strcmp(interpretation_author(interpretation), "demo-v1") == 0);
    assert(strcmp(interpretation_method(interpretation), "demo-method") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);
    interpretation_destroy(interpretation);
}

static void test_invalid_arguments(void) {
    int content = 7;
    assert(claim_interpretation_create(NULL, "demo", "i", "t", "C", &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("", "demo", "i", "t", "C", &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", NULL, "i", "t", "C", &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", NULL, "t", "C", &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "", "C", &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", NULL, &content, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", "C", NULL, sizeof(content), 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", "C", &content, 0, 0.5, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", "C", &content, sizeof(content), 0.5, "") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", "C", &content, sizeof(content), -0.1, "m") == NULL);
    assert(claim_interpretation_create("f", "demo", "i", "t", "C", &content, sizeof(content), 1.1, "m") == NULL);
}

static void test_note_group(void) {
    const char *notes[] = {"n1", "n2", "n1"};
    Structure *group = claim_interpretation_note_group("demo", "demo-notes-a", notes, 3);
    assert(group != NULL);
    assert(strcmp(entity_id_value(entity_id(structure_entity(group))), "demo-notes-a") == 0);
    assert(strcmp(entity_type(structure_entity(group)), "demo/note-group") == 0);
    assert(structure_event_id_count(group) == 2);
    assert(strcmp(entity_id_value(structure_event_id_at(group, 0)), "n1") == 0);
    assert(strcmp(entity_id_value(structure_event_id_at(group, 1)), "n2") == 0);
    structure_destroy(group);

    const char *with_empty[] = {"n1", ""};
    assert(claim_interpretation_note_group("demo", "s", with_empty, 2) == NULL);
    assert(claim_interpretation_note_group("demo", "s", notes, 0) == NULL);
    assert(claim_interpretation_note_group("demo", "", notes, 3) == NULL);
    assert(claim_interpretation_note_group(NULL, "s", notes, 3) == NULL);
}

static void test_relationship(void) {
    Relationship *relationship = claim_interpretation_relationship("demo", "link", "k1", "part-a", "part-b", "follows");
    assert(relationship != NULL);
    assert(strcmp(entity_id_value(relationship_id(relationship)), "demo-link-k1") == 0);
    assert(strcmp(entity_id_value(relationship_source_id(relationship)), "part-a") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationship)), "part-b") == 0);
    assert(strcmp(relationship_predicate_name(relationship), "follows") == 0);
    relationship_destroy(relationship);

    assert(claim_interpretation_relationship("demo", "link", "k1", "a", "b", "") == NULL);
    assert(claim_interpretation_relationship("demo", NULL, "k1", "a", "b", "p") == NULL);
    assert(claim_interpretation_relationship(NULL, "link", "k1", "a", "b", "p") == NULL);
}

int main(void) {
    test_confidence_rounding();
    test_fields();
    test_invalid_arguments();
    test_note_group();
    test_relationship();
    return 0;
}
