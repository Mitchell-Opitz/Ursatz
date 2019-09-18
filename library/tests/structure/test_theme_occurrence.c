#include <assert.h>
#include <stdio.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"
#include "structure/theme_occurrence.h"

/*
 * Relationship test: a ThemeOccurrence resolves back to its Theme (by
 * id, via Occurrence's target_id) and to its own concrete event
 * region, and multiple occurrences of the same Theme remain distinct
 * Occurrence instances (distinct own id) while pointing at the same
 * Theme identity.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static ThemeOccurrence *make_occurrence(const char *own_id, const char *theme_id, const char *region_event_id) {
    Occurrence *occurrence;
    assert(occurrence_create(dup_id(own_id), dup_id(theme_id), &occurrence) == OCCURRENCE_OK);

    EntityID *region_event_ids[1] = {dup_id(region_event_id)};
    ThemeOccurrence *theme_occurrence;
    assert(theme_occurrence_create(occurrence, region_event_ids, 1, &theme_occurrence) == THEME_OCCURRENCE_OK);
    return theme_occurrence;
}

int main(void) {
    ThemeOccurrence *first = make_occurrence("occurrence-1", "theme-1", "event-a");
    ThemeOccurrence *second = make_occurrence("occurrence-2", "theme-1", "event-b");

    EntityID *expected_theme_id = dup_id("theme-1");
    assert(entity_id_equals(theme_occurrence_theme_id(first), expected_theme_id));
    assert(entity_id_equals(theme_occurrence_theme_id(second), expected_theme_id));
    entity_id_destroy(expected_theme_id);

    /* Both occurrences share the same Theme identity ... */
    assert(entity_id_equals(theme_occurrence_theme_id(first), theme_occurrence_theme_id(second)));
    /* ... but remain distinct Occurrence instances. */
    assert(!entity_id_equals(theme_occurrence_id(first), theme_occurrence_id(second)));

    assert(theme_occurrence_region_event_id_count(first) == 1);
    EntityID *expected_region_a = dup_id("event-a");
    assert(entity_id_equals(theme_occurrence_region_event_id_at(first, 0), expected_region_a));
    entity_id_destroy(expected_region_a);

    EntityID *expected_region_b = dup_id("event-b");
    assert(entity_id_equals(theme_occurrence_region_event_id_at(second, 0), expected_region_b));
    entity_id_destroy(expected_region_b);

    theme_occurrence_destroy(first);
    theme_occurrence_destroy(second);

    printf("test_theme_occurrence: ok\n");
    return 0;
}