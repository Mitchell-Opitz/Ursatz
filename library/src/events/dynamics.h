#ifndef MUSIC_EVENTS_DYNAMICS_H
#define MUSIC_EVENTS_DYNAMICS_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "time/time_span.h"

/*
 * Dynamics: a dynamics marking (p, mf, f, cresc./dim., etc.) attaching
 * to either a single Event or a TimeSpan.
 *
 * marking is an opaque, uninterpreted string, not a closed enum: the
 * Registry's "p, mf, f, cresc./dim., etc." is explicitly non-exhaustive,
 * the same treatment events/articulation.h gives its kind field.
 *
 * Two explicit constructors, not one nullable-field constructor,
 * mirroring core/provenance_reference.h's present/absent split: an
 * Event has identity, so it is referenced by EntityID only (never
 * embedded, matching events/tie.h and events/slur.h). A TimeSpan has no
 * identity to reference by -- it is an immutable Value
 * (time/time_span.h) -- so the TimeSpan-target constructor takes
 * ownership of the TimeSpan itself rather than referencing it by id;
 * since TimeSpan is immutable, holding it directly carries none of the
 * embedding/mutation risk that holding a NoteEvent or other Entity
 * directly would.
 */

typedef struct Dynamics Dynamics;

typedef enum {
    DYNAMICS_TARGET_EVENT,
    DYNAMICS_TARGET_TIME_SPAN
} DynamicsTargetKind;

typedef enum {
    DYNAMICS_OK = 0,
    DYNAMICS_ERR_NULL_MARKING,
    DYNAMICS_ERR_NULL_EVENT_ID,
    DYNAMICS_ERR_NULL_TIME_SPAN,
    DYNAMICS_ERR_ALLOC
} DynamicsResult;

/* marking is copied. Takes ownership of event_id on success (destroyed by dynamics_destroy). */
DynamicsResult dynamics_create_for_event(const char *marking, EntityID *event_id, Dynamics **out);
/* marking is copied. Takes ownership of time_span on success (destroyed by dynamics_destroy). */
DynamicsResult dynamics_create_for_time_span(const char *marking, TimeSpan *time_span, Dynamics **out);
void dynamics_destroy(Dynamics *dynamics);

const char *dynamics_marking(const Dynamics *dynamics);
DynamicsTargetKind dynamics_target_kind(const Dynamics *dynamics);
/* NULL unless dynamics_target_kind is DYNAMICS_TARGET_EVENT. */
const EntityID *dynamics_target_event_id(const Dynamics *dynamics);
/* NULL unless dynamics_target_kind is DYNAMICS_TARGET_TIME_SPAN. */
const TimeSpan *dynamics_target_time_span(const Dynamics *dynamics);

#endif