#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_ANALYSIS_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_grid.h"
#include "identity/entity_id.h"
#include "pitch/tuning_system.h"

/*
 * A grid built from MIDI semitone numbers, one row per segment: rows[segment][voice], 0 meaning the voice rests. Voice 0
 * is the top. Notes get ids "n<segment>-<voice>"; a note that repeats the previous segment's pitch in the same voice is
 * marked held (not attacked) and keeps the previous note's id, as a held note does.
 */

#define TEST_GRID_MAX_SEGMENTS 16
#define TEST_GRID_MAX_VOICES 4

typedef struct {
    StandardCounterpointVoiceGrid grid;
    StandardCounterpointVoiceCell cells[TEST_GRID_MAX_SEGMENTS * TEST_GRID_MAX_VOICES];
    PitchNumericValue *values[TEST_GRID_MAX_SEGMENTS * TEST_GRID_MAX_VOICES];
    char note_ids[TEST_GRID_MAX_SEGMENTS * TEST_GRID_MAX_VOICES][16];
    char segment_names[TEST_GRID_MAX_SEGMENTS][16];
    const char *segment_ids[TEST_GRID_MAX_SEGMENTS];
    TuningSystem *tuning;
} TestGrid;

static inline void test_grid_init(TestGrid *g, size_t segments, size_t voices, const int rows[][TEST_GRID_MAX_VOICES]) {
    assert(segments <= TEST_GRID_MAX_SEGMENTS && voices <= TEST_GRID_MAX_VOICES);
    memset(g, 0, sizeof(*g));
    EntityID *id = NULL;
    assert(entity_id_create("midi-tuning", &id) == ENTITY_ID_OK);
    assert(tuning_system_create(id, "MIDI note number", &g->tuning) == TUNING_SYSTEM_OK);
    for (size_t s = 0; s < segments; s++) {
        snprintf(g->segment_names[s], sizeof(g->segment_names[s]), "seg-%zu", s);
        g->segment_ids[s] = g->segment_names[s];
        for (size_t v = 0; v < voices; v++) {
            size_t i = s * voices + v;
            StandardCounterpointVoiceCell *cell = &g->cells[i];
            int midi = rows[s][v];
            if (midi == 0) {
                continue;
            }
            assert(pitch_numeric_value_create(g->tuning, midi, 1, &g->values[i]) == PITCH_NUMERIC_VALUE_OK);
            cell->present = true;
            cell->pitch = g->values[i];
            cell->confidence = 1.0;
            bool held = s > 0 && rows[s - 1][v] == midi;
            cell->attacked = !held;
            if (held) {
                snprintf(g->note_ids[i], sizeof(g->note_ids[i]), "%s", g->note_ids[(s - 1) * voices + v]);
            } else {
                snprintf(g->note_ids[i], sizeof(g->note_ids[i]), "n%zu-%zu", s, v);
            }
            cell->note_id = g->note_ids[i];
        }
    }
    g->grid.segment_count = segments;
    g->grid.voice_count = voices;
    g->grid.segment_ids = g->segment_ids;
    g->grid.cells = g->cells;
}

static inline void test_grid_destroy(TestGrid *g) {
    for (size_t i = 0; i < TEST_GRID_MAX_SEGMENTS * TEST_GRID_MAX_VOICES; i++) {
        pitch_numeric_value_destroy(g->values[i]);
    }
    tuning_system_destroy(g->tuning);
}

#endif
