#include "events/dynamics.h"

#include <stdlib.h>
#include <string.h>

struct Dynamics {
    char *marking;
    DynamicsTargetKind target_kind;
    EntityID *target_event_id;
    TimeSpan *target_time_span;
};

static char *copy_marking(const char *marking) {
    size_t len = strlen(marking);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, marking, len + 1);
    }
    return copy;
}

DynamicsResult dynamics_create_for_event(const char *marking, EntityID *event_id, Dynamics **out) {
    if (marking == NULL || marking[0] == '\0') {
        return DYNAMICS_ERR_NULL_MARKING;
    }
    if (event_id == NULL) {
        return DYNAMICS_ERR_NULL_EVENT_ID;
    }

    char *marking_copy = copy_marking(marking);
    if (marking_copy == NULL) {
        return DYNAMICS_ERR_ALLOC;
    }

    Dynamics *dynamics = (Dynamics *)malloc(sizeof(Dynamics));
    if (dynamics == NULL) {
        free(marking_copy);
        return DYNAMICS_ERR_ALLOC;
    }

    dynamics->marking = marking_copy;
    dynamics->target_kind = DYNAMICS_TARGET_EVENT;
    dynamics->target_event_id = event_id;
    dynamics->target_time_span = NULL;

    *out = dynamics;
    return DYNAMICS_OK;
}

DynamicsResult dynamics_create_for_time_span(const char *marking, TimeSpan *time_span, Dynamics **out) {
    if (marking == NULL || marking[0] == '\0') {
        return DYNAMICS_ERR_NULL_MARKING;
    }
    if (time_span == NULL) {
        return DYNAMICS_ERR_NULL_TIME_SPAN;
    }

    char *marking_copy = copy_marking(marking);
    if (marking_copy == NULL) {
        return DYNAMICS_ERR_ALLOC;
    }

    Dynamics *dynamics = (Dynamics *)malloc(sizeof(Dynamics));
    if (dynamics == NULL) {
        free(marking_copy);
        return DYNAMICS_ERR_ALLOC;
    }

    dynamics->marking = marking_copy;
    dynamics->target_kind = DYNAMICS_TARGET_TIME_SPAN;
    dynamics->target_event_id = NULL;
    dynamics->target_time_span = time_span;

    *out = dynamics;
    return DYNAMICS_OK;
}

void dynamics_destroy(Dynamics *dynamics) {
    if (dynamics == NULL) {
        return;
    }
    free(dynamics->marking);
    entity_id_destroy(dynamics->target_event_id);
    time_span_destroy(dynamics->target_time_span);
    free(dynamics);
}

const char *dynamics_marking(const Dynamics *dynamics) {
    return dynamics->marking;
}

DynamicsTargetKind dynamics_target_kind(const Dynamics *dynamics) {
    return dynamics->target_kind;
}

const EntityID *dynamics_target_event_id(const Dynamics *dynamics) {
    return dynamics->target_event_id;
}

const TimeSpan *dynamics_target_time_span(const Dynamics *dynamics) {
    return dynamics->target_time_span;
}