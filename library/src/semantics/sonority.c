#include "semantics/sonority.h"

#include <stdlib.h>

struct Sonority {
    TimeSpan *time_window;
    EntityID **event_ids;
    size_t event_id_count;
    EntityID **voice_ids;
    size_t voice_id_count;
    SonorityEventDomain event_domain;
};

SonorityResult sonority_create(TimeSpan *time_window, EntityID **event_ids, size_t event_id_count,
                                EntityID **voice_ids, size_t voice_id_count, SonorityEventDomain event_domain,
                                Sonority **out) {
    if (time_window == NULL) {
        return SONORITY_ERR_NULL_TIME_WINDOW;
    }
    if (event_ids == NULL || event_id_count == 0) {
        return SONORITY_ERR_NULL_EVENT_IDS;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return SONORITY_ERR_NULL_EVENT_ID;
        }
    }
    if (voice_id_count > 0) {
        if (voice_ids == NULL) {
            return SONORITY_ERR_NULL_VOICE_ID;
        }
        for (size_t i = 0; i < voice_id_count; i++) {
            if (voice_ids[i] == NULL) {
                return SONORITY_ERR_NULL_VOICE_ID;
            }
        }
    }

    Sonority *sonority = (Sonority *)malloc(sizeof(Sonority));
    if (sonority == NULL) {
        return SONORITY_ERR_ALLOC;
    }

    EntityID **owned_event_ids = (EntityID **)malloc(sizeof(EntityID *) * event_id_count);
    if (owned_event_ids == NULL) {
        free(sonority);
        return SONORITY_ERR_ALLOC;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        owned_event_ids[i] = event_ids[i];
    }

    EntityID **owned_voice_ids = NULL;
    if (voice_id_count > 0) {
        owned_voice_ids = (EntityID **)malloc(sizeof(EntityID *) * voice_id_count);
        if (owned_voice_ids == NULL) {
            free(owned_event_ids);
            free(sonority);
            return SONORITY_ERR_ALLOC;
        }
        for (size_t i = 0; i < voice_id_count; i++) {
            owned_voice_ids[i] = voice_ids[i];
        }
    }

    sonority->time_window = time_window;
    sonority->event_ids = owned_event_ids;
    sonority->event_id_count = event_id_count;
    sonority->voice_ids = owned_voice_ids;
    sonority->voice_id_count = voice_id_count;
    sonority->event_domain = event_domain;

    *out = sonority;
    return SONORITY_OK;
}

void sonority_destroy(Sonority *sonority) {
    if (sonority == NULL) {
        return;
    }
    time_span_destroy(sonority->time_window);
    for (size_t i = 0; i < sonority->event_id_count; i++) {
        entity_id_destroy(sonority->event_ids[i]);
    }
    free(sonority->event_ids);
    for (size_t i = 0; i < sonority->voice_id_count; i++) {
        entity_id_destroy(sonority->voice_ids[i]);
    }
    free(sonority->voice_ids);
    free(sonority);
}

const TimeSpan *sonority_time_window(const Sonority *sonority) {
    return sonority->time_window;
}

size_t sonority_event_id_count(const Sonority *sonority) {
    return sonority->event_id_count;
}

const EntityID *sonority_event_id_at(const Sonority *sonority, size_t index) {
    return sonority->event_ids[index];
}

size_t sonority_voice_id_count(const Sonority *sonority) {
    return sonority->voice_id_count;
}

const EntityID *sonority_voice_id_at(const Sonority *sonority, size_t index) {
    return sonority->voice_ids[index];
}

SonorityEventDomain sonority_event_domain(const Sonority *sonority) {
    return sonority->event_domain;
}