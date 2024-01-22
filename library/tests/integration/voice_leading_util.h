#ifndef URSATZ_TESTS_INTEGRATION_VOICE_LEADING_UTIL_H
#define URSATZ_TESTS_INTEGRATION_VOICE_LEADING_UTIL_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "core/provenance_reference.h"
#include "pitch/pitch_spelling.h"
#include "time/time_span.h"
#include "analysis_grid.h"
#include "analysis_input.h"
#include "analysis_voices.h"
#include "pitch/pitch_identity.h"
#include "pitch/tuning_system.h"
#include "segmentation_beat_grid.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/*
 * Test glue for the voice leading adapters: a four-part piece in the chorale text format (S, A, T and B on separate lines,
 * <midi>:<eighths>), its chords, and the two ways to put its notes into voices: the TRUE parts (the line each note is on) or
 * the voices L2.7's voice separation assigns when the parts are merged into one stream. From either assignment this builds
 * what both adapters take: the voice-pitch grid of standard-counterpoint and the one of standard-harmony (identical
 * semantics, defined in each framework), and the neutral per-segment inputs of standard-harmony with the hand-labelled chords.
 * The frameworks do not include each other; this glue, which includes both, lives in the tests.
 *
 * Segments are cut at every onset of any voice (one chord segment per distinct stretch of the four parts), and a segment's
 * chord is the label of the quarter note it falls in.
 */

#define VL_MAX_NOTES 256
#define VL_MAX_SEGMENTS 128
#define VL_VOICES 4

typedef struct {
    int part; /* 0 S, 1 A, 2 T, 3 B */
    long on;  /* eighth notes */
    long off;
    int midi;
    char id[16];
} VlNote;

typedef struct {
    char rule[32];
    int from;
    int to;
    int upper;
    int lower;
} VlExpected;

typedef struct {
    VlNote notes[VL_MAX_NOTES];
    size_t note_count;
    long total; /* eighth notes */
    char chords[64][8];
    size_t chord_count;
    VlExpected expected[16];
    size_t expected_count;
} VlPiece;

static inline unsigned char *vl_read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)length + 1);
    assert(data != NULL);
    size_t got = fread(data, 1, (size_t)length, file);
    assert(got == (size_t)length);
    fclose(file);
    data[length] = '\0';
    *out_size = (size_t)length;
    return data;
}

/* Reads the part lines (and an H line and error lines when present) of a fixture into `piece`. */
static inline void vl_load(const char *path, VlPiece *piece) {
    size_t size = 0;
    unsigned char *text = vl_read_file(path, &size);
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        const char *parts = "SATB";
        const char *found = line[1] == ' ' ? strchr(parts, line[0]) : NULL;
        if (found != NULL && line[0] != '\0') {
            int part = (int)(found - parts);
            long at = 0;
            char *cursor = line + 2;
            while (*cursor != '\0') {
                char *end = NULL;
                long midi = strtol(cursor, &end, 10);
                assert(end != cursor && *end == ':');
                cursor = end + 1;
                long eighths = strtol(cursor, &end, 10);
                assert(end != cursor && eighths > 0);
                cursor = end;
                while (*cursor == ' ') {
                    cursor++;
                }
                VlNote *note = &piece->notes[piece->note_count++];
                assert(piece->note_count <= VL_MAX_NOTES);
                note->part = part;
                note->on = at;
                note->off = at + eighths;
                note->midi = (int)midi;
                snprintf(note->id, sizeof(note->id), "%c%03ld", line[0], at);
                at += eighths;
            }
            piece->total = at;
        } else if (strncmp(line, "H ", 2) == 0) {
            char *save = NULL;
            char *token = strtok_r(line + 2, " ", &save);
            while (token != NULL) {
                snprintf(piece->chords[piece->chord_count++], sizeof(piece->chords[0]), "%s", token);
                assert(piece->chord_count <= 64);
                token = strtok_r(NULL, " ", &save);
            }
        } else if (strncmp(line, "error ", 6) == 0) {
            VlExpected *e = &piece->expected[piece->expected_count++];
            assert(sscanf(line, "error %31s %d %d %d %d", e->rule, &e->from, &e->to, &e->upper, &e->lower) == 5);
        }
    }
    free(text);
}

/* --- a voice assignment: each note's voice and confidence --- */

typedef struct {
    size_t voice[VL_MAX_NOTES];
    double confidence[VL_MAX_NOTES];
} VlAssignment;

static inline void vl_true_parts(const VlPiece *piece, VlAssignment *a) {
    for (size_t i = 0; i < piece->note_count; i++) {
        a->voice[i] = (size_t)piece->notes[i].part;
        a->confidence[i] = 1.0;
    }
}

static inline EntityID *vl_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

/* The voices L2.7 assigns when every part is merged into one stream (a 4/4 MeterMap, eighth-note units). */
static inline void vl_separated(const VlPiece *piece, VlAssignment *a) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    TuningSystem *tuning = NULL;
    assert(tuning_system_create(vl_id("vl-midi"), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    NoteEvent **events = (NoteEvent **)calloc(piece->note_count, sizeof(NoteEvent *));
    PitchNumericValue **values = (PitchNumericValue **)calloc(piece->note_count, sizeof(PitchNumericValue *));
    assert(events != NULL && values != NULL);
    for (size_t i = 0; i < piece->note_count; i++) {
        const VlNote *n = &piece->notes[i];
        MusicalTime *position = NULL;
        assert(musical_time_create(n->on, 8, &position) == MUSICAL_TIME_OK);
        TemporalAnchor *anchor = NULL;
        assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
        Duration *duration = NULL;
        assert(duration_create(n->off - n->on, 8, &duration) == DURATION_OK);
        PitchIdentity *pitch = NULL;
        assert(pitch_identity_create("pitch", 6, &pitch) == PITCH_IDENTITY_OK);
        PitchSpelling *spelling = NULL;
        assert(pitch_spelling_create(LETTER[n->midi % 12], ACCIDENTAL[n->midi % 12], n->midi / 12 - 1, &spelling) ==
               PITCH_SPELLING_OK);
        ProvenanceReference *provenance = NULL;
        assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
        assert(note_event_create(vl_id(n->id), anchor, duration, pitch, spelling, NULL, provenance, &events[i]) ==
               NOTE_EVENT_OK);
        assert(pitch_numeric_value_create(tuning, n->midi, 1, &values[i]) == PITCH_NUMERIC_VALUE_OK);
    }
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(4, quarter, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {signature, zero};
    MeterMap *map = NULL;
    assert(meter_map_create(vl_id("vl-meter"), &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);

    VoiceAssignment assignment = {0};
    assert(voice_separate(events, values, piece->note_count, NULL, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                          &assignment) == VOICE_SEPARATION_OK);
    for (size_t i = 0; i < piece->note_count; i++) {
        a->voice[i] = assignment.entries[i].voice;
        a->confidence[i] = assignment.entries[i].confidence;
    }
    voice_assignment_destroy(&assignment);
    for (size_t i = 0; i < piece->note_count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    free(events);
    free(values);
    tuning_system_destroy(tuning);
    meter_map_destroy(map);
}

/* --- segments, grids and the neutral inputs --- */

typedef struct {
    size_t segment_count;
    long start[VL_MAX_SEGMENTS];
    long end[VL_MAX_SEGMENTS];
    char segment_names[VL_MAX_SEGMENTS][16];
    const char *segment_ids[VL_MAX_SEGMENTS];

    StandardCounterpointVoiceCell cp_cells[VL_MAX_SEGMENTS * VL_VOICES];
    HarmonyVoiceCell harmony_cells[VL_MAX_SEGMENTS * VL_VOICES];
    StandardCounterpointVoiceGrid cp_grid;
    HarmonyVoiceGrid harmony_grid;

    HarmonySegmentInput inputs[VL_MAX_SEGMENTS];
    TimeSpan *spans[VL_MAX_SEGMENTS];
    MetricalPosition *positions[VL_MAX_SEGMENTS];

    PitchNumericValue *values[VL_MAX_NOTES];
    TuningSystem *tuning;
} VlSegments;

static inline int vl_roman_degree(const char *letters, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    assert(length < sizeof(lower));
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    return -1;
}

static inline void vl_fill_chord(HarmonySegmentInput *input, const char *numeral) {
    static const int MAJOR[3] = {0, 4, 7};
    static const int MINOR[3] = {0, 3, 7};
    static const int DIM[3] = {0, 3, 6};
    static const int DOMINANT7[4] = {0, 4, 7, 10};
    static const int QUALITY_OF_DEGREE[7] = {0, 1, 1, 0, 0, 1, 2};
    size_t n = 0;
    while (numeral[n] == 'I' || numeral[n] == 'V' || numeral[n] == 'i' || numeral[n] == 'v') {
        n++;
    }
    int degree = vl_roman_degree(numeral, n);
    assert(degree > 0);
    const char *figure = numeral + n;
    bool seventh = strcmp(figure, "7") == 0;
    input->degree = degree;
    input->inversion = strcmp(figure, "6") == 0 ? 1 : 0;
    memset(input->intervals, 0, sizeof(input->intervals));
    if (seventh) {
        memcpy(input->intervals, DOMINANT7, sizeof(DOMINANT7));
        input->interval_count = 4;
    } else {
        int q = QUALITY_OF_DEGREE[degree - 1];
        memcpy(input->intervals, q == 0 ? MAJOR : (q == 1 ? MINOR : DIM), sizeof(MAJOR));
        input->interval_count = 3;
    }
}

static inline void vl_build(const VlPiece *piece, const VlAssignment *assignment, VlSegments *seg) {
    memset(seg, 0, sizeof(*seg));
    assert(tuning_system_create(vl_id("vl-grid-midi"), "MIDI note number", &seg->tuning) == TUNING_SYSTEM_OK);
    /* cut at every onset */
    bool onset[512] = {false};
    for (size_t i = 0; i < piece->note_count; i++) {
        onset[piece->notes[i].on] = true;
    }
    long begin = 0;
    for (long t = 1; t <= piece->total; t++) {
        if (t == piece->total || onset[t]) {
            assert(seg->segment_count < VL_MAX_SEGMENTS);
            seg->start[seg->segment_count] = begin;
            seg->end[seg->segment_count] = t;
            seg->segment_count++;
            begin = t;
        }
    }
    for (size_t i = 0; i < piece->note_count; i++) {
        assert(pitch_numeric_value_create(seg->tuning, piece->notes[i].midi, 1, &seg->values[i]) ==
               PITCH_NUMERIC_VALUE_OK);
    }
    for (size_t s = 0; s < seg->segment_count; s++) {
        snprintf(seg->segment_names[s], sizeof(seg->segment_names[s]), "seg-%zu", s);
        seg->segment_ids[s] = seg->segment_names[s];
        for (size_t i = 0; i < piece->note_count; i++) {
            const VlNote *n = &piece->notes[i];
            if (!(n->on <= seg->start[s] && seg->start[s] < n->off)) {
                continue;
            }
            size_t voice = assignment->voice[i];
            if (voice >= VL_VOICES) {
                continue;
            }
            size_t cell = s * VL_VOICES + voice;
            if (seg->cp_cells[cell].present) {
                continue; /* an extra note in the same voice: the first stands */
            }
            seg->cp_cells[cell].present = true;
            seg->cp_cells[cell].pitch = seg->values[i];
            seg->cp_cells[cell].note_id = n->id;
            seg->cp_cells[cell].attacked = n->on == seg->start[s];
            seg->cp_cells[cell].confidence = assignment->confidence[i];
            seg->harmony_cells[cell].present = true;
            seg->harmony_cells[cell].pitch = seg->values[i];
            seg->harmony_cells[cell].note_id = n->id;
            seg->harmony_cells[cell].attacked = n->on == seg->start[s];
            seg->harmony_cells[cell].confidence = assignment->confidence[i];
        }
    }
    seg->cp_grid.segment_count = seg->segment_count;
    seg->cp_grid.voice_count = VL_VOICES;
    seg->cp_grid.segment_ids = seg->segment_ids;
    seg->cp_grid.cells = seg->cp_cells;
    seg->harmony_grid.segment_count = seg->segment_count;
    seg->harmony_grid.voice_count = VL_VOICES;
    seg->harmony_grid.cells = seg->harmony_cells;

    /* the neutral inputs of standard-harmony: the quarter's hand-labelled chord */
    for (size_t s = 0; s < seg->segment_count; s++) {
        MusicalTime *from = NULL;
        MusicalTime *to = NULL;
        assert(musical_time_create(seg->start[s], 8, &from) == MUSICAL_TIME_OK);
        assert(musical_time_create(seg->end[s], 8, &to) == MUSICAL_TIME_OK);
        assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &seg->spans[s]) == TIME_SPAN_OK);
        musical_time_destroy(from);
        musical_time_destroy(to);
        Duration *offset = NULL;
        assert(duration_create(seg->start[s] % 8, 8, &offset) == DURATION_OK);
        assert(metrical_position_create((int)(seg->start[s] / 8) + 1, offset, &seg->positions[s]) ==
               METRICAL_POSITION_OK);
        duration_destroy(offset);

        HarmonySegmentInput *input = &seg->inputs[s];
        input->segment_id = seg->segment_ids[s];
        input->span = seg->spans[s];
        input->position = seg->positions[s];
        input->beat_strength = seg->start[s] % 8 == 0 ? BEAT_STRENGTH_STRONG
                               : seg->start[s] % 4 == 0 ? BEAT_STRENGTH_MEDIUM
                                                        : BEAT_STRENGTH_WEAK;
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->uncertainty = 0.0;
        size_t quarter = (size_t)(seg->start[s] / 2);
        if (quarter < piece->chord_count) {
            vl_fill_chord(input, piece->chords[quarter]);
        } else {
            input->degree = 0; /* no chord labelled: not a chord this framework reads */
            input->interval_count = 3;
        }
    }
}

static inline void vl_destroy(VlSegments *seg, size_t note_count) {
    for (size_t s = 0; s < seg->segment_count; s++) {
        time_span_destroy(seg->spans[s]);
        metrical_position_destroy(seg->positions[s]);
    }
    for (size_t i = 0; i < note_count; i++) {
        pitch_numeric_value_destroy(seg->values[i]);
    }
    tuning_system_destroy(seg->tuning);
}

#endif
