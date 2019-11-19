#ifndef MUSIC_STRUCTURE_FIGURE_H
#define MUSIC_STRUCTURE_FIGURE_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Figure (Structure): a small structural unit.
 *
 * Fields: id (EntityID) and event refs -- EntityID pointers at the
 * NoteEvents that make up the figure, referenced not duplicated (TDD
 * Sec56, Sec152). Figure does not embed the Structure/Entity wrapper
 * types directly -- it flattens to the same field shape (id plus
 * event-id references), the same approach Motif takes
 * (structure/motif.h).
 *
 * Registry marks Figure "No (example only)": this is minimal,
 * non-normative scaffolding consistent with Motif's existing pattern,
 * not a finished authoritative design.
 *
 * Figure has no mutator: a revision is a new Figure construction, not
 * an in-place field change (Law C2).
 */

typedef struct Figure Figure;

typedef enum {
    FIGURE_OK = 0,
    FIGURE_ERR_NULL_ID,
    FIGURE_ERR_NULL_EVENT_IDS,
    FIGURE_ERR_NULL_EVENT_ID,
    FIGURE_ERR_ALLOC
} FigureResult;

/*
 * Takes ownership of id and of event_ids (and each EntityID it points
 * to) on success -- all destroyed by figure_destroy. event_ids must be
 * non-NULL with event_id_count > 0; no entry may be NULL.
 */
FigureResult figure_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Figure **out);
void figure_destroy(Figure *figure);

const EntityID *figure_id(const Figure *figure);
size_t figure_event_id_count(const Figure *figure);
const EntityID *figure_event_id_at(const Figure *figure, size_t index);

/* Identity equality: same EntityID, regardless of instance. */
bool figure_identity_equals(const Figure *a, const Figure *b);

#endif