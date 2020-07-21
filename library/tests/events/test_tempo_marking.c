#include "events/tempo_marking.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static TemporalAnchor *anchor_at(long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(time, &anchor) == TEMPORAL_ANCHOR_OK);
    return anchor;
}

static MetronomeMarking *quarter_equals(unsigned long bpm) {
    Duration *quarter = NULL;
    assert(duration_create(1, 4, &quarter) == DURATION_OK);
    MetronomeMarking *m = NULL;
    assert(metronome_marking_create(quarter, bpm, 1, &m) == METRONOME_MARKING_OK);
    duration_destroy(quarter);
    return m;
}

static void test_create_rejects_null_anchor(void) {
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(NULL, "allegro", NULL, &marking) == TEMPO_MARKING_ERR_NULL_TEMPORAL_ANCHOR);
    assert(marking == NULL);
}

static void test_create_rejects_empty(void) {
    TemporalAnchor *anchor = anchor_at(0, 1);
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor, NULL, NULL, &marking) == TEMPO_MARKING_ERR_EMPTY);
    assert(tempo_marking_create(anchor, "", NULL, &marking) == TEMPO_MARKING_ERR_EMPTY);
    assert(marking == NULL);
    /* Not taken on failure: still ours to destroy. */
    temporal_anchor_destroy(anchor);
}

static void test_text_only(void) {
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor_at(0, 1), "andante cantabile", NULL, &marking) == TEMPO_MARKING_OK);
    assert(tempo_marking_has_text(marking));
    assert(strcmp(tempo_marking_text(marking), "andante cantabile") == 0);
    assert(!tempo_marking_has_metronome_marking(marking));
    assert(tempo_marking_metronome_marking(marking) == NULL);
    tempo_marking_destroy(marking);
}

static void test_metronome_only(void) {
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor_at(0, 1), NULL, quarter_equals(60), &marking) == TEMPO_MARKING_OK);
    assert(!tempo_marking_has_text(marking));
    assert(tempo_marking_text(marking) == NULL);
    assert(tempo_marking_has_metronome_marking(marking));
    assert(metronome_marking_bpm_numerator(tempo_marking_metronome_marking(marking)) == 60);
    tempo_marking_destroy(marking);
}

/* "Allegro (quarter = 120)": both together. */
static void test_text_and_metronome(void) {
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor_at(3, 1), "allegro", quarter_equals(120), &marking) == TEMPO_MARKING_OK);
    assert(tempo_marking_has_text(marking) && tempo_marking_has_metronome_marking(marking));
    const MusicalTime *position = temporal_anchor_position(tempo_marking_temporal_anchor(marking));
    assert(musical_time_numerator(position) == 3);
    assert(musical_time_denominator(position) == 1);
    tempo_marking_destroy(marking);
}

/* The text is copied: mutating the caller's buffer does not change the marking. */
static void test_text_is_copied(void) {
    char text[] = "largo";
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor_at(0, 1), text, NULL, &marking) == TEMPO_MARKING_OK);
    text[0] = 'X';
    assert(strcmp(tempo_marking_text(marking), "largo") == 0);
    tempo_marking_destroy(marking);
}

int main(void) {
    test_create_rejects_null_anchor();
    test_create_rejects_empty();
    test_text_only();
    test_metronome_only();
    test_text_and_metronome();
    test_text_is_copied();
    return 0;
}
