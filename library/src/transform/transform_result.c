#include "transform/transform_result.h"

#include <stdlib.h>

struct TransformResult {
    NoteEvent **events;
    size_t event_count;
};

static NoteEvent **copy_event_pointers(NoteEvent **events, size_t count) {
    NoteEvent **copy = (NoteEvent **)malloc(sizeof(NoteEvent *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = events[i];
    }
    return copy;
}

static void destroy_events(NoteEvent **events, size_t count) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

TransformResultResult transform_result_create(NoteEvent **events, size_t count, TransformResult **out) {
    if (count > 0) {
        if (events == NULL) {
            return TRANSFORM_RESULT_ERR_NULL_EVENT;
        }
        for (size_t i = 0; i < count; i++) {
            if (events[i] == NULL) {
                return TRANSFORM_RESULT_ERR_NULL_EVENT;
            }
        }
    }

    TransformResult *result = (TransformResult *)malloc(sizeof(TransformResult));
    if (result == NULL) {
        return TRANSFORM_RESULT_ERR_ALLOC;
    }

    if (count > 0) {
        result->events = copy_event_pointers(events, count);
        if (result->events == NULL) {
            free(result);
            return TRANSFORM_RESULT_ERR_ALLOC;
        }
    } else {
        result->events = NULL;
    }
    result->event_count = count;

    *out = result;
    return TRANSFORM_RESULT_OK;
}

void transform_result_destroy(TransformResult *result) {
    if (result == NULL) {
        return;
    }
    destroy_events(result->events, result->event_count);
    free(result);
}

size_t transform_result_event_count(const TransformResult *result) {
    return result->event_count;
}

const NoteEvent *transform_result_event_at(const TransformResult *result, size_t index) {
    return result->events[index];
}

NoteEvent *transform_result_take_event_at(TransformResult *result, size_t index) {
    NoteEvent *event = result->events[index];
    result->events[index] = NULL;
    return event;
}