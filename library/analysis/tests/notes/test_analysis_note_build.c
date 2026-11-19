#include "analysis/notes/analysis_note_build.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"

/*
 * The one note set builder (R-614): the notes come out sorted by analysis_note_compare, order maps a sorted note to its
 * source index, spelling off leaves every spelling NULL, tracks are read when given, the map is the source's, and an empty
 * source gives an empty set. A zero-length note or a pitch outside 0 to 127 fails the build.
 */

typedef struct {
    NoteEvent *event;
    PitchNumericValue *pitch;
} Built;

static TuningSystem *tuning(void) {
    EntityID *id = NULL;
    TuningSystem *system = NULL;
    assert(entity_id_create("midi-note-number", &id) == ENTITY_ID_OK);
    assert(tuning_system_create(id, "MIDI note number", &system) == TUNING_SYSTEM_OK);
    return system;
}

/* A note starting at start_num/start_den whole notes, lasting length_num/length_den, with a spelling of C octave 4. */
static Built make_note(const char *name, long long start_num, long long start_den, long long length_num,
                       long long length_den, int midi, bool voice) {
    EntityID *id = NULL;
    MusicalTime time;
    Duration length;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    VoiceReference *voice_reference = NULL;
    ProvenanceReference *provenance = NULL;
    Built built;
    assert(entity_id_create(name, &id) == ENTITY_ID_OK);
    assert(musical_time_make(start_num, start_den, &time) == MUSICAL_TIME_OK);
    assert(duration_make(length_num, length_den, &length) == DURATION_OK);
    assert(pitch_identity_create("c4", 2, &pitch) == PITCH_IDENTITY_OK);
    assert(pitch_spelling_create(PITCH_LETTER_C, 0, 4, &spelling) == PITCH_SPELLING_OK);
    if (voice) {
        EntityID *voice_id = NULL;
        assert(entity_id_create("voice-1", &voice_id) == ENTITY_ID_OK);
        assert(voice_reference_create(voice_id, &voice_reference) == VOICE_REFERENCE_OK);
    }
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(id, time, length, pitch, spelling, voice_reference, provenance, &built.event) ==
           NOTE_EVENT_OK);
    TuningSystem *system = tuning();
    assert(pitch_numeric_value_create(system, midi, 1, &built.pitch) == PITCH_NUMERIC_VALUE_OK);
    tuning_system_destroy(system);
    return built;
}

static void test_sorted_with_order(void) {
    /* Source order: later, earlier high, earlier low. */
    Built notes[3] = {make_note("n0", 1, 4, 1, 4, 60, false), make_note("n1", 0, 1, 1, 4, 67, true),
                      make_note("n2", 0, 1, 1, 4, 62, false)};
    const NoteEvent *events[3] = {notes[0].event, notes[1].event, notes[2].event};
    PitchNumericValue *pitches[3] = {notes[0].pitch, notes[1].pitch, notes[2].pitch};
    size_t tracks[3] = {5, 6, 7};
    AnalysisNoteSource source = {.events = events, .pitch_values = pitches, .tracks = tracks, .count = 3,
                                 .use_spelling = true, .map = NULL};
    AnalysisNoteSet set;
    size_t *order = NULL;
    assert(analysis_note_set_build(&source, &set, &order));
    assert(set.count == 3 && order != NULL && set.map == NULL);
    /* Start, then midi: n2 (0, 62), n1 (0, 67), n0 (1/4, 60). */
    assert(order[0] == 2 && order[1] == 1 && order[2] == 0);
    assert(strcmp(set.notes[0].note_id, "n2") == 0 && set.notes[0].midi == 62 && set.notes[0].track == 7);
    assert(set.notes[1].midi == 67 && set.notes[1].track == 6 && set.notes[1].voice_id != NULL &&
           set.notes[1].voice_confidence == 1.0);
    assert(set.notes[2].midi == 60 && set.notes[2].track == 5 && set.notes[2].voice_id == NULL &&
           set.notes[2].voice_confidence == 0.0);
    for (size_t i = 0; i < 3; i++) {
        assert(set.notes[i].spelling != NULL && set.notes[i].event == events[order[i]]);
        assert(set.notes[i].voice == ANALYSIS_INDEX_NONE);
        assert(analysis_note_valid(&set.notes[i]));
    }
    analysis_note_set_built_destroy(&set, order);
    assert(set.notes == NULL && set.count == 0);

    /* Spelling off and no tracks. */
    source.use_spelling = false;
    source.tracks = NULL;
    assert(analysis_note_set_build(&source, &set, NULL));
    for (size_t i = 0; i < 3; i++) {
        assert(set.notes[i].spelling == NULL && set.notes[i].track == ANALYSIS_INDEX_NONE);
    }
    analysis_note_set_built_destroy(&set, NULL);

    for (size_t i = 0; i < 3; i++) {
        note_event_destroy(notes[i].event);
        pitch_numeric_value_destroy(notes[i].pitch);
    }
}

static void test_map_is_the_sources(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    assert(meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    Built note = make_note("n0", 0, 1, 1, 4, 60, false);
    const NoteEvent *events[1] = {note.event};
    PitchNumericValue *pitches[1] = {note.pitch};
    AnalysisNoteSource source = {.events = events, .pitch_values = pitches, .count = 1, .map = map};
    AnalysisNoteSet set;
    assert(analysis_note_set_build(&source, &set, NULL));
    assert(set.map == map && analysis_note_set_valid(&set));
    analysis_note_set_built_destroy(&set, NULL);
    note_event_destroy(note.event);
    pitch_numeric_value_destroy(note.pitch);
    meter_map_destroy(map);
}

static void test_empty_and_refused(void) {
    AnalysisNoteSource empty = {0};
    AnalysisNoteSet set = {0};
    size_t *order = (size_t *)1;
    assert(analysis_note_set_build(&empty, &set, &order));
    assert(set.notes == NULL && set.count == 0 && order == NULL);
    analysis_note_set_built_destroy(&set, order);

    assert(!analysis_note_set_build(NULL, &set, NULL));
    AnalysisNoteSource missing = {.count = 2};
    assert(!analysis_note_set_build(&missing, &set, NULL));

    /* A pitch outside 0 to 127 makes no valid note: nothing is built and the outputs are untouched. */
    Built high = make_note("n0", 0, 1, 1, 4, 200, false);
    const NoteEvent *events[1] = {high.event};
    PitchNumericValue *pitches[1] = {high.pitch};
    AnalysisNoteSource bad = {.events = events, .pitch_values = pitches, .count = 1};
    AnalysisNoteSet untouched = {.count = 7};
    size_t *no_order = (size_t *)2;
    assert(!analysis_note_set_build(&bad, &untouched, &no_order));
    assert(untouched.count == 7 && no_order == (size_t *)2);
    note_event_destroy(high.event);
    pitch_numeric_value_destroy(high.pitch);
}

int main(void) {
    test_sorted_with_order();
    test_map_is_the_sources();
    test_empty_and_refused();
    return 0;
}
