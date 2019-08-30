#include "containers/section.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Section *section = NULL;
    SectionResult r = section_create(NULL, NULL, 0, &section);
    assert(r == SECTION_ERR_NULL_ID);
    assert(section == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("section-1", &id);

    Section *section = NULL;
    SectionResult r = section_create(id, NULL, 0, &section);
    assert(r == SECTION_OK);
    assert(section != NULL);
    assert(section_phrase_id_count(section) == 0);

    section_destroy(section);
}

static void test_create_rejects_null_phrase_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("section-1", &id);

    EntityID *phrase_ids[1] = {NULL};

    Section *section = NULL;
    SectionResult r = section_create(id, phrase_ids, 1, &section);
    assert(r == SECTION_ERR_NULL_PHRASE_ID);
    assert(section == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_phrase_references(void) {
    EntityID *id = NULL;
    entity_id_create("section-1", &id);

    EntityID *phrase_id_1 = NULL;
    entity_id_create("phrase-1", &phrase_id_1);
    EntityID *phrase_id_2 = NULL;
    entity_id_create("phrase-2", &phrase_id_2);
    EntityID *phrase_ids[2] = {phrase_id_1, phrase_id_2};

    Section *section = NULL;
    SectionResult r = section_create(id, phrase_ids, 2, &section);
    assert(r == SECTION_OK);
    assert(section != NULL);

    assert(section_phrase_id_count(section) == 2);
    assert(entity_id_equals(section_phrase_id_at(section, 0), phrase_id_1));
    assert(entity_id_equals(section_phrase_id_at(section, 1), phrase_id_2));

    section_destroy(section);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("section-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("section-1", &id_b);

    Section *a = NULL;
    section_create(id_a, NULL, 0, &a);
    Section *b = NULL;
    section_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(section_identity_equals(a, b));

    section_destroy(a);
    section_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("section-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("section-2", &id_b);

    Section *a = NULL;
    section_create(id_a, NULL, 0, &a);
    Section *b = NULL;
    section_create(id_b, NULL, 0, &b);

    assert(!section_identity_equals(a, b));

    section_destroy(a);
    section_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_empty_references();
    test_create_rejects_null_phrase_id_entry();
    test_create_accepts_phrase_references();
    test_identity_equals_same_id();
    test_identity_not_equal_with_different_id();
    return 0;
}