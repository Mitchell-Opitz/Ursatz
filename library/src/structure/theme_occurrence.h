#ifndef MUSIC_STRUCTURE_THEME_OCCURRENCE_H
#define MUSIC_STRUCTURE_THEME_OCCURRENCE_H

#include <stddef.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"

/*
 * ThemeOccurrence: a situated instance of a Theme.
 *
 * Composes a core/occurrence.h Occurrence (its own id, and the id of
 * the Theme it is an occurrence of, as Occurrence's target_id) with
 * region_event_ids -- EntityID pointers at the concrete events present
 * at this occurrence, referenced not duplicated (TDD Sec56, Sec152).
 * Same pattern structure/motif_occurrence.h uses for Motif.
 *
 * Key pattern (TDD Sec58): the Theme is abstract; occurrences point at
 * concrete regions of musical material without the Theme itself being
 * duplicated per occurrence. A single Theme may have many
 * ThemeOccurrences, each a distinct ThemeOccurrence instance (distinct
 * own id) sharing the same target Theme id.
 *
 * ThemeOccurrence has no mutator: a revision is a new construction, not
 * an in-place field change (Law C2).
 */

typedef struct ThemeOccurrence ThemeOccurrence;

typedef enum {
    THEME_OCCURRENCE_OK = 0,
    THEME_OCCURRENCE_ERR_NULL_OCCURRENCE,
    THEME_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS,
    THEME_OCCURRENCE_ERR_NULL_REGION_EVENT_ID,
    THEME_OCCURRENCE_ERR_ALLOC
} ThemeOccurrenceResult;

/*
 * Takes ownership of occurrence and of region_event_ids (and each
 * EntityID it points to) on success -- all destroyed by
 * theme_occurrence_destroy. region_event_ids must be non-NULL with
 * region_event_id_count > 0; no entry may be NULL. occurrence's
 * target_id is the Theme this is an occurrence of.
 */
ThemeOccurrenceResult theme_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                               size_t region_event_id_count, ThemeOccurrence **out);
void theme_occurrence_destroy(ThemeOccurrence *theme_occurrence);

const EntityID *theme_occurrence_id(const ThemeOccurrence *theme_occurrence);
/* The id of the Theme this is an occurrence of. */
const EntityID *theme_occurrence_theme_id(const ThemeOccurrence *theme_occurrence);
size_t theme_occurrence_region_event_id_count(const ThemeOccurrence *theme_occurrence);
const EntityID *theme_occurrence_region_event_id_at(const ThemeOccurrence *theme_occurrence, size_t index);

#endif