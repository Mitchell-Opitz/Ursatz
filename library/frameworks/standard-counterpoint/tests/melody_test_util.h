#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_MELODY_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_MELODY_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_melody.h"
#include "identity/entity_id.h"
#include "pitch/tuning_system.h"

/*
 * A melody line from MIDI semitone numbers, 0 meaning a rest. Notes get ids "m<index>", confidence 1, no spelling, no
 * degree, no strength; a test sets those on `line.notes` through the helpers after test_melody_init (the entries are
 * rebuilt by test_melody_rebuild when the fields it copies change).
 */

#define TEST_MELODY_MAX 32

typedef struct {
    StandardCounterpointMelodyLine line;
    StandardCounterpointMelodyNote notes[TEST_MELODY_MAX]; /* what is added: edit, then test_melody_build */
    PitchNumericValue *values[TEST_MELODY_MAX];
    char ids[TEST_MELODY_MAX][16];
    size_t count;
    TuningSystem *tuning;
} TestMelody;

static inline void test_melody_init(TestMelody *m, const int *midi, size_t count) {
    assert(count <= TEST_MELODY_MAX);
    memset(m, 0, sizeof(*m));
    EntityID *id = NULL;
    assert(entity_id_create("midi-tuning", &id) == ENTITY_ID_OK);
    assert(tuning_system_create(id, "MIDI note number", &m->tuning) == TUNING_SYSTEM_OK);
    m->count = count;
    for (size_t i = 0; i < count; i++) {
        if (midi[i] == 0) {
            continue;
        }
        assert(pitch_numeric_value_create(m->tuning, midi[i], 1, &m->values[i]) == PITCH_NUMERIC_VALUE_OK);
        snprintf(m->ids[i], sizeof(m->ids[i]), "m%zu", i);
        m->notes[i].present = true;
        m->notes[i].note_id = m->ids[i];
        m->notes[i].pitch = m->values[i];
        m->notes[i].confidence = 1.0;
    }
}

/* Copies the (possibly edited) notes into m->line. */
static inline void test_melody_build(TestMelody *m) {
    standard_counterpoint_melody_line_destroy(&m->line);
    standard_counterpoint_melody_line_init(&m->line);
    for (size_t i = 0; i < m->count; i++) {
        if (m->notes[i].present) {
            assert(standard_counterpoint_melody_line_add_note(&m->line, &m->notes[i]) == STANDARD_COUNTERPOINT_MELODY_OK);
        } else {
            assert(standard_counterpoint_melody_line_add_rest(&m->line) == STANDARD_COUNTERPOINT_MELODY_OK);
        }
    }
}

static inline void test_melody_destroy(TestMelody *m) {
    standard_counterpoint_melody_line_destroy(&m->line);
    for (size_t i = 0; i < TEST_MELODY_MAX; i++) {
        pitch_numeric_value_destroy(m->values[i]);
    }
    tuning_system_destroy(m->tuning);
}

/* A line of the given pitches, built. */
static inline void test_melody_make(TestMelody *m, const int *midi, size_t count) {
    test_melody_init(m, midi, count);
    test_melody_build(m);
}

#endif
