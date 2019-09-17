#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "structure/theme.h"

/*
 * Relationship test: a Theme holds its referenced Phrases as EntityID
 * references only, never duplicated Phrase data, and its identity is
 * stable independent of instance.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *phrase_ids[2] = {dup_id("phrase-a"), dup_id("phrase-b")};
    Theme *theme;
    assert(theme_create(dup_id("theme-1"), phrase_ids, 2, &theme) == THEME_OK);

    assert(theme_phrase_id_count(theme) == 2);

    EntityID *expected_a = dup_id("phrase-a");
    EntityID *expected_b = dup_id("phrase-b");
    assert(entity_id_equals(theme_phrase_id_at(theme, 0), expected_a));
    assert(entity_id_equals(theme_phrase_id_at(theme, 1), expected_b));
    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);

    EntityID *other_phrase_ids[2] = {dup_id("phrase-a"), dup_id("phrase-b")};
    Theme *same_theme;
    assert(theme_create(dup_id("theme-1"), other_phrase_ids, 2, &same_theme) == THEME_OK);
    assert(theme_identity_equals(theme, same_theme));

    EntityID *diff_phrase_ids[2] = {dup_id("phrase-a"), dup_id("phrase-b")};
    Theme *different_theme;
    assert(theme_create(dup_id("theme-2"), diff_phrase_ids, 2, &different_theme) == THEME_OK);
    assert(!theme_identity_equals(theme, different_theme));

    theme_destroy(theme);
    theme_destroy(same_theme);
    theme_destroy(different_theme);

    printf("test_theme: ok\n");
    return 0;
}