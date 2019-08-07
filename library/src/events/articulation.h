#ifndef MUSIC_EVENTS_ARTICULATION_H
#define MUSIC_EVENTS_ARTICULATION_H

#include "identity/entity_id.h"

/*
 * Articulation: a performance marking (staccato, tenuto, accent, etc.)
 * attached to a single Event. An Articulation is its own object
 * referencing the target Event's id -- it does not hold, mutate, or
 * embed the Event, mirroring events/tie.h's Tie and events/slur.h's
 * Slur.
 *
 * kind is an opaque, uninterpreted string, not a closed enum: the
 * Registry's "staccato, tenuto, accent, etc." is explicitly non-
 * exhaustive, so this type does not bake in a fixed vocabulary. Same
 * treatment events/control_event.h gives its payload and pitch/
 * pitch_spelling.h gives its accidental field.
 */

typedef struct Articulation Articulation;

typedef enum {
    ARTICULATION_OK = 0,
    ARTICULATION_ERR_NULL_KIND,
    ARTICULATION_ERR_NULL_EVENT_ID,
    ARTICULATION_ERR_ALLOC
} ArticulationResult;

/* kind is copied. Takes ownership of event_id on success (destroyed by articulation_destroy). */
ArticulationResult articulation_create(const char *kind, EntityID *event_id, Articulation **out);
void articulation_destroy(Articulation *articulation);

const char *articulation_kind(const Articulation *articulation);
const EntityID *articulation_event_id(const Articulation *articulation);

#endif