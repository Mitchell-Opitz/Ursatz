#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "semantics/sonority.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Golden test: a known event set, voice set, time window, and event
 * domain produce a known Sonority shape -- every field comes back
 * exactly as given, nothing implicit added or dropped.
 */

static TimeSpan *make_window(void) {
    MusicalTime *start;
    assert(musical_time_create(0, 1, &start) == MUSICAL_TIME_OK);
    MusicalTime *end;
    assert(musical_time_create(1, 1, &end) == MUSICAL_TIME_OK);

    TimeSpan *span;
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_CLOSED, &span) == TIME_SPAN_OK);
    return span;
}

int main(void) {
    TimeSpan *window = make_window();

    EntityID *event_a;
    assert(entity_id_create("note-a", &event_a) == ENTITY_ID_OK);
    EntityID *event_b;
    assert(entity_id_create("note-b", &event_b) == ENTITY_ID_OK);
    EntityID *event_c;
    assert(entity_id_create("note-c", &event_c) == ENTITY_ID_OK);
    EntityID *event_ids[3] = {event_a, event_b, event_c};

    EntityID *voice_soprano;
    assert(entity_id_create("voice-soprano", &voice_soprano) == ENTITY_ID_OK);
    EntityID *voice_alto;
    assert(entity_id_create("voice-alto", &voice_alto) == ENTITY_ID_OK);
    EntityID *voice_ids[2] = {voice_soprano, voice_alto};

    Sonority *sonority;
    SonorityResult result =
        sonority_create(window, event_ids, 3, voice_ids, 2, SONORITY_EVENT_DOMAIN_SOUNDING, &sonority);
    assert(result == SONORITY_OK);

    assert(sonority_time_window(sonority) == window);

    assert(sonority_event_id_count(sonority) == 3);
    EntityID *expected_a;
    assert(entity_id_create("note-a", &expected_a) == ENTITY_ID_OK);
    EntityID *expected_b;
    assert(entity_id_create("note-b", &expected_b) == ENTITY_ID_OK);
    EntityID *expected_c;
    assert(entity_id_create("note-c", &expected_c) == ENTITY_ID_OK);
    assert(entity_id_equals(sonority_event_id_at(sonority, 0), expected_a));
    assert(entity_id_equals(sonority_event_id_at(sonority, 1), expected_b));
    assert(entity_id_equals(sonority_event_id_at(sonority, 2), expected_c));
    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);
    entity_id_destroy(expected_c);

    assert(sonority_voice_id_count(sonority) == 2);
    EntityID *expected_soprano;
    assert(entity_id_create("voice-soprano", &expected_soprano) == ENTITY_ID_OK);
    EntityID *expected_alto;
    assert(entity_id_create("voice-alto", &expected_alto) == ENTITY_ID_OK);
    assert(entity_id_equals(sonority_voice_id_at(sonority, 0), expected_soprano));
    assert(entity_id_equals(sonority_voice_id_at(sonority, 1), expected_alto));
    entity_id_destroy(expected_soprano);
    entity_id_destroy(expected_alto);

    assert(sonority_event_domain(sonority) == SONORITY_EVENT_DOMAIN_SOUNDING);

    sonority_destroy(sonority);

    /* Empty voice set is a valid, explicit "no restriction" selection. */
    TimeSpan *window_2 = make_window();
    EntityID *event_solo;
    assert(entity_id_create("note-solo", &event_solo) == ENTITY_ID_OK);
    EntityID *solo_ids[1] = {event_solo};

    Sonority *unrestricted;
    SonorityResult unrestricted_result =
        sonority_create(window_2, solo_ids, 1, NULL, 0, SONORITY_EVENT_DOMAIN_SYMBOLIC, &unrestricted);
    assert(unrestricted_result == SONORITY_OK);
    assert(sonority_voice_id_count(unrestricted) == 0);
    assert(sonority_event_domain(unrestricted) == SONORITY_EVENT_DOMAIN_SYMBOLIC);
    sonority_destroy(unrestricted);

    /* An empty event set is not a Sonority. */
    TimeSpan *window_3 = make_window();
    Sonority *empty;
    SonorityResult empty_result =
        sonority_create(window_3, NULL, 0, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING, &empty);
    assert(empty_result == SONORITY_ERR_NULL_EVENT_IDS);
    time_span_destroy(window_3);

    printf("test_sonority: ok\n");
    return 0;
}