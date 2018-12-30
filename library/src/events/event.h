#ifndef MUSIC_EVENTS_EVENT_H
#define MUSIC_EVENTS_EVENT_H

#include "core/occurrence.h"
#include "time/temporal_anchor.h"

/*
 * Event: an Occurrence associated with a temporal domain. Base type for
 * NoteEvent, RestEvent, and ControlEvent.
 *
 * Composes an Occurrence (L1) with a TemporalAnchor (L2) without
 * collapsing into either: an Event is not itself an Occurrence and not
 * itself a TemporalAnchor, it holds one of each.
 *
 * The Entity/Occurrence/Event category boundary is contested and
 * unresolved upstream (Domain Spec §9, §16.1). This type does not assume
 * a resolution; code built on it should do the same.
 */

typedef struct Event Event;

typedef enum {
    EVENT_OK = 0,
    EVENT_ERR_NULL_OCCURRENCE,
    EVENT_ERR_NULL_TEMPORAL_ANCHOR,
    EVENT_ERR_ALLOC
} EventResult;

/* Takes ownership of occurrence and temporal_anchor on success (destroyed by event_destroy). */
EventResult event_create(Occurrence *occurrence, TemporalAnchor *temporal_anchor, Event **out);
void event_destroy(Event *event);

const Occurrence *event_occurrence(const Event *event);
const TemporalAnchor *event_temporal_anchor(const Event *event);

#endif