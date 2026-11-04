#include "score_roundtrip_util.h"

#include "io/midi_importer.h"
#include "../io/midi_import_split.h"
#include "io/midi_key_signature.h"

/*
 * The five framework-v1 reference pieces: MIDI import (notes, MeterMap, TempoMap, key signatures) ->
 * musicxml_export_write_score -> MusicXML import, compared by rt_assert_same. A two-channel file's notes carry no
 * voice, so the two hands overlap in one implicit voice; this test, not the library, gives each hand its own staff by
 * colouring the notes of a voice greedily (Clementi 36 and Field 1). URSATZ_CORPUS_DIR is passed in by CMake.
 */

static unsigned char *read_corpus_file(const char *name, size_t *size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, name);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length);
    assert(data != NULL && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return data;
}

typedef struct {
    const char *voice;
    int staff;
    long end_n;
    unsigned long end_d;
} Lane;

/* staff[i]: the lowest staff of the note's voice that is free at its start; returns the number of staves used. */
static int colour_staves(NoteEvent *const *events, size_t count, int *staff) {
    Lane *lanes = (Lane *)calloc(count > 0 ? count : 1, sizeof(Lane));
    assert(lanes != NULL);
    size_t lane_count = 0;
    int staves = 1;
    for (size_t i = 0; i < count; i++) {
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        const char *key = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        MusicalTime *end = NULL;
        assert(musical_time_add(position, duration, &end) == MUSICAL_TIME_OK);
        int chosen = 0;
        for (int s = 1; chosen == 0; s++) {
            Lane *lane = NULL;
            for (size_t l = 0; l < lane_count; l++) {
                bool same = (lanes[l].voice == NULL && key == NULL) ||
                            (lanes[l].voice != NULL && key != NULL && strcmp(lanes[l].voice, key) == 0);
                if (same && lanes[l].staff == s) {
                    lane = &lanes[l];
                    break;
                }
            }
            bool free_lane = lane == NULL || (long long)lane->end_n * (long long)musical_time_denominator(position) <=
                                                 (long long)musical_time_numerator(position) * (long long)lane->end_d;
            if (free_lane) {
                if (lane == NULL) {
                    lane = &lanes[lane_count++];
                    lane->voice = key;
                    lane->staff = s;
                }
                lane->end_n = musical_time_numerator(end);
                lane->end_d = musical_time_denominator(end);
                chosen = s;
            }
        }
        musical_time_destroy(end);
        staff[i] = chosen;
        if (chosen > staves) {
            staves = chosen;
        }
    }
    free(lanes);
    return staves;
}

static void round_trip_piece(const char *name, const char *expected_beats) {
    size_t size = 0;
    unsigned char *data = read_corpus_file(name, &size);

    MusicXmlImport first;
    memset(&first, 0, sizeof(first));

    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    assert(test_midi_import_parts(data, size, &first.events, &first.pitch_values, &first.count, &meter, NULL) ==
           MIDI_IMPORT_OK);
    assert(first.count > 0);
    first.meter_map = meter.meter_map;

    NoteEvent **tempo_events = NULL;
    PitchNumericValue **tempo_pitches = NULL;
    size_t tempo_count = 0;
    MidiTempoImport tempo = {NULL, NULL, 0};
    assert(test_midi_import_parts(data, size, &tempo_events, &tempo_pitches, &tempo_count, NULL, &tempo) ==
           MIDI_IMPORT_OK);
    for (size_t i = 0; i < tempo_count; i++) {
        note_event_destroy(tempo_events[i]);
        pitch_numeric_value_destroy(tempo_pitches[i]);
    }
    free(tempo_events);
    free(tempo_pitches);
    first.tempo_map = tempo.tempo_map;

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(midi_key_signature_scan_all(data, size, &keys) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    first.key_signatures = (MusicXmlKeySignatureEvent *)calloc(keys.count > 0 ? keys.count : 1,
                                                               sizeof(MusicXmlKeySignatureEvent));
    assert(first.key_signatures != NULL);
    first.key_signature_count = keys.count;
    for (size_t i = 0; i < keys.count; i++) {
        first.key_signatures[i].sharps_flats = keys.events[i].sharps_flats;
        first.key_signatures[i].mode_hint = keys.events[i].mode_byte == 0   ? MUSICXML_MODE_HINT_MAJOR
                                            : keys.events[i].mode_byte == 1 ? MUSICXML_MODE_HINT_MINOR
                                                                            : MUSICXML_MODE_HINT_UNKNOWN;
        first.key_signatures[i].position = keys.events[i].position;
        first.key_signatures[i].part_index = 0;
    }

    first.part_index = (size_t *)calloc(first.count, sizeof(size_t));
    first.staff = (int *)calloc(first.count, sizeof(int));
    assert(first.part_index != NULL && first.staff != NULL);
    int staves = colour_staves(first.events, first.count, first.staff);
    MusicXmlPart part = {"P1", "Ursatz Export", staves};
    first.parts = &part;
    first.part_count = 1;

    char *xml = rt_round_trip(&first);
    assert(TU_HAS(xml, expected_beats));
    printf("%s: %zu notes, %d staff(s), %zu measure(s), %zu key signature(s): round-trips exactly\n", name, first.count,
           staves, rt_measure_count(&first), first.key_signature_count);
    free(xml);

    free(first.part_index);
    free(first.staff);
    free(first.key_signatures);
    midi_key_signature_events_destroy(&keys);
    midi_tempo_import_destroy(&tempo);
    midi_meter_import_destroy(&meter);
    for (size_t i = 0; i < first.count; i++) {
        note_event_destroy(first.events[i]);
        pitch_numeric_value_destroy(first.pitch_values[i]);
    }
    free(first.events);
    free(first.pitch_values);
    free(data);
}

int main(void) {
    round_trip_piece("Bach 114.mid", "<beats>");
    round_trip_piece("Clementi 36.mid", "<beats>");
    round_trip_piece("Field 1.mid", "<beats>12</beats>"); /* 12/8 */
    round_trip_piece("Field 5.mid", "<beats>");
    round_trip_piece("Mozart 545.mid", "<beats>");
    printf("test_musicxml_score_reference: all tests passed\n");
    return 0;
}
