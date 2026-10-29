#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "core/provenance_reference.h"
#include "io/midi_importer.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "segmentation_beat_grid.h"
#include "time/duration.h"
#include "core/rational.h"
#include "time/time_rational.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/*
 * Voice separation against ground truth. URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR are passed in by CMake.
 *
 *  - Chorale (asserted, 90%): tests/fixtures/annotations/chorale_satb.txt is a four-part chorale (S, A, T, B),
 *    each part its own line of notes. The parts are merged into ONE stream and separated; a note is correct
 *    when its voice index equals its part (S = 0 ... B = 3). The fixture is a DRAFT hand-written chorale in
 *    Bach's idiom, not an edition (see its header).
 *  - Bach 114 (asserted, 80%): the MIDI file is one track, so there are no tracks to compare. The ground truth is
 *    the draft hand annotation tests/fixtures/annotations/bach114_outer_voices.txt (measures 1-8): the right
 *    hand's melody and the left hand's bass and chords. The file is separated as one stream with a 3/4 MeterMap
 *    (the file declares 4/4, wrongly). A note is correct when it is in voice 0 exactly if it is the melody (the
 *    left hand's chord notes may fall in any lower voice).
 *  - Clementi 36, Field 1 and Mozart 545 (printed, not asserted): no ground truth for their inner voices. Where
 *    a file has per-hand tracks (Clementi 36, Field 1) each track is also a stream, and the merged stream's
 *    voices are tabled against the tracks.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

typedef struct {
    NoteEvent **notes;
    PitchNumericValue **values;
    size_t count;
    size_t capacity;
} Built;

static void add_note(Built *set, const TuningSystem *tuning, const char *id, long on_n, unsigned long on_d, long dur_n,
                     unsigned long dur_d, int midi) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    assert(set->count < set->capacity);
    MusicalTime *position = NULL;
    assert(musical_time_create(on_n, on_d, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(dur_n, dur_d, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    assert(pitch_identity_create("pitch", 6, &pitch) == PITCH_IDENTITY_OK);
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(LETTER[midi % 12], ACCIDENTAL[midi % 12], midi / 12 - 1, &spelling) ==
           PITCH_SPELLING_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(make_id(id), anchor, duration, pitch, spelling, NULL, provenance,
                             &set->notes[set->count]) == NOTE_EVENT_OK);
    assert(pitch_numeric_value_create(tuning, midi, 1, &set->values[set->count]) == PITCH_NUMERIC_VALUE_OK);
    set->count++;
}

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length + 1);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);
    data[length] = '\0';
    *out_size = (size_t)length;
    return data;
}

/* --- the chorale --- */

typedef struct {
    size_t truth[512];
    size_t count;
} Truth;

static void test_chorale(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/chorale_satb.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = read_file(path, &size);

    TuningSystem *tuning = NULL;
    assert(tuning_system_create(make_id("midi"), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    Built set = {(NoteEvent **)calloc(512, sizeof(NoteEvent *)), (PitchNumericValue **)calloc(512, sizeof(void *)), 0,
                 512};
    Truth truth = {{0}, 0};
    long part_length[4] = {-1, -1, -1, -1};
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        const char *parts = "SATB";
        const char *found = (line[0] != '#' && line[0] != '\0') ? strchr(parts, line[0]) : NULL;
        if (found == NULL || line[1] != ' ') {
            continue;
        }
        size_t part = (size_t)(found - parts);
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
            char id[32];
            snprintf(id, sizeof(id), "%c%03ld", line[0], at);
            add_note(&set, tuning, id, at, 8, eighths, 8, (int)midi);
            truth.truth[truth.count++] = part;
            at += eighths;
        }
        part_length[part] = at;
    }
    free(text);
    for (int p = 1; p < 4; p++) {
        assert(part_length[p] == part_length[0] && part_length[0] >= 8 * 8);
    }

    MeterMap *map = NULL;
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(4, quarter, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(make_id("chorale-meter"), &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);

    VoiceAssignment a = {0};
    assert(voice_separate(set.notes, set.values, set.count, NULL, map, standard_meter_segmentation_beat_grid, NULL,
                          NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.stream_count == 1 && a.streams[0].voice_count == 4);

    size_t per_voice_total[4] = {0, 0, 0, 0};
    size_t per_voice_right[4] = {0, 0, 0, 0};
    size_t right = 0;
    for (size_t i = 0; i < set.count; i++) {
        per_voice_total[truth.truth[i]]++;
        if (a.entries[i].voice == truth.truth[i]) {
            per_voice_right[truth.truth[i]]++;
            right++;
        }
    }
    printf("Chorale (draft, hand-written, merged to one stream), %zu notes:\n", set.count);
    static const char *const NAMES[4] = {"soprano", "alto", "tenor", "bass"};
    for (int v = 0; v < 4; v++) {
        printf("  %s: %zu/%zu (%.1f%%)\n", NAMES[v], per_voice_right[v], per_voice_total[v],
               100.0 * (double)per_voice_right[v] / (double)per_voice_total[v]);
    }
    printf("  all voices: %zu/%zu (%.1f%%)\n", right, set.count, 100.0 * (double)right / (double)set.count);
    assert((double)right / (double)set.count >= 0.90);

    voice_assignment_destroy(&a);
    for (size_t i = 0; i < set.count; i++) {
        note_event_destroy(set.notes[i]);
        pitch_numeric_value_destroy(set.values[i]);
    }
    free(set.notes);
    free(set.values);
    tuning_system_destroy(tuning);
    meter_map_destroy(map);
}

/* --- corpus files --- */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitches;
    size_t count;
    MidiMeterImport meter;
    long ticks_per_quarter;
} Imported;

static Imported import_file(const char *file, long ticks_per_quarter) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    Imported imported;
    memset(&imported, 0, sizeof(imported));
    assert(midi_importer_import_with_meter(data, size, &imported.events, &imported.pitches, &imported.count,
                                           &imported.meter) == MIDI_IMPORT_OK);
    free(data);
    imported.ticks_per_quarter = ticks_per_quarter;
    return imported;
}

static void destroy_imported(Imported *imported) {
    for (size_t i = 0; i < imported->count; i++) {
        note_event_destroy(imported->events[i]);
        pitch_numeric_value_destroy(imported->pitches[i]);
    }
    free(imported->events);
    free(imported->pitches);
    midi_meter_import_destroy(&imported->meter);
}

static long onset_ticks(const Imported *imported, size_t i) {
    const MusicalTime *at = temporal_anchor_position(note_event_temporal_anchor(imported->events[i]));
    return musical_time_numerator(at) * 4 * imported->ticks_per_quarter / (long)musical_time_denominator(at);
}

static MeterMap *three_four_map(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *three_four = NULL;
    assert(time_signature_create(3, quarter, &three_four) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {three_four, zero};
    MeterMap *map = NULL;
    assert(meter_map_create(make_id("bach-114-score-meter"), &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    return map;
}

typedef struct {
    long start;
    long pitch;
    int kind; /* 0 melody, 1 accompaniment */
} Written;

static size_t load_bach_truth(Written *out, size_t capacity) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/bach114_outer_voices.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = read_file(path, &size);
    size_t count = 0;
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char which[16];
        long measure = 0;
        long offset = 0;
        long duration = 0;
        long pitch = 0;
        assert(sscanf(line, "%15s %ld %ld %ld %ld", which, &measure, &offset, &duration, &pitch) == 5);
        assert(count < capacity);
        out[count].start = (measure - 1) * 1152 + offset;
        out[count].pitch = pitch;
        out[count].kind = strcmp(which, "soprano") == 0 ? 0 : 1;
        count++;
    }
    free(text);
    return count;
}

static void test_bach_114(void) {
    Imported imported = import_file("Bach 114.mid", 384);
    MeterMap *map = three_four_map();
    VoiceAssignment a = {0};
    assert(voice_separate(imported.events, imported.pitches, imported.count, NULL, map,
                          standard_meter_segmentation_beat_grid, NULL, NULL, &a) == VOICE_SEPARATION_OK);

    Written truth[512];
    size_t truth_count = load_bach_truth(truth, 512);
    size_t compared = 0;
    size_t right = 0;
    size_t melody_total = 0;
    size_t melody_right = 0;
    size_t accompaniment_total = 0;
    size_t accompaniment_right = 0;
    printf("Bach 114, measures 1-8, one stream (%zu voices), against the written parts:\n",
           a.streams[0].voice_count);
    for (size_t i = 0; i < imported.count; i++) {
        long start = onset_ticks(&imported, i);
        if (start >= 8 * 1152) {
            continue;
        }
        long pitch = pitch_numeric_value_position_numerator(imported.pitches[i]);
        int kind = -1;
        for (size_t t = 0; t < truth_count; t++) {
            if (truth[t].start == start && truth[t].pitch == pitch) {
                kind = truth[t].kind;
                break;
            }
        }
        if (kind < 0) {
            continue; /* a note the draft annotation does not list */
        }
        compared++;
        bool top = a.entries[i].voice == 0;
        bool ok = (kind == 0) == top;
        right += ok ? 1 : 0;
        if (kind == 0) {
            melody_total++;
            melody_right += ok ? 1 : 0;
        } else {
            accompaniment_total++;
            accompaniment_right += ok ? 1 : 0;
        }
        if (!ok) {
            printf("  mismatch at measure %ld tick %ld pitch %ld: written %s, got voice %zu\n", start / 1152 + 1,
                   start % 1152, pitch, kind == 0 ? "melody" : "accompaniment", a.entries[i].voice);
        }
    }
    assert(compared > 0);
    printf("  %zu notes compared: %zu correct (%.1f%%); melody %zu/%zu, accompaniment %zu/%zu\n", compared, right,
           100.0 * (double)right / (double)compared, melody_right, melody_total, accompaniment_right,
           accompaniment_total);
    assert((double)right / (double)compared >= 0.80);

    voice_assignment_destroy(&a);
    meter_map_destroy(map);
    destroy_imported(&imported);
}

/* A minimal SMF reader: the track of each note-on (velocity > 0), as (tick, pitch, track). */
typedef struct {
    long tick;
    int pitch;
    size_t track;
    bool used;
} MidiOn;

static size_t read_note_ons(const unsigned char *d, size_t size, MidiOn *out, size_t capacity) {
    assert(size > 14 && memcmp(d, "MThd", 4) == 0);
    size_t tracks = (size_t)(d[10] << 8 | d[11]);
    size_t pos = 14;
    size_t count = 0;
    for (size_t t = 0; t < tracks; t++) {
        assert(pos + 8 <= size && memcmp(d + pos, "MTrk", 4) == 0);
        size_t length = ((size_t)d[pos + 4] << 24) | ((size_t)d[pos + 5] << 16) | ((size_t)d[pos + 6] << 8) | d[pos + 7];
        size_t i = pos + 8;
        size_t end = i + length;
        long tick = 0;
        unsigned char running = 0;
        while (i < end) {
            long delta = 0;
            while (d[i] & 0x80) {
                delta = (delta << 7) | (d[i++] & 0x7f);
            }
            delta = (delta << 7) | d[i++];
            tick += delta;
            unsigned char status = d[i];
            if (status >= 0x80) {
                i++;
                if (status < 0xf0) {
                    running = status;
                }
            } else {
                status = running;
            }
            if (status == 0xff) {
                i++;
                long meta = 0;
                while (d[i] & 0x80) {
                    meta = (meta << 7) | (d[i++] & 0x7f);
                }
                meta = (meta << 7) | d[i++];
                i += (size_t)meta;
            } else if (status == 0xf0 || status == 0xf7) {
                long sys = 0;
                while (d[i] & 0x80) {
                    sys = (sys << 7) | (d[i++] & 0x7f);
                }
                sys = (sys << 7) | d[i++];
                i += (size_t)sys;
            } else {
                unsigned char kind = status & 0xf0;
                if (kind == 0xc0 || kind == 0xd0) {
                    i += 1;
                } else {
                    if (kind == 0x90 && d[i + 1] > 0) {
                        assert(count < capacity);
                        out[count].tick = tick;
                        out[count].pitch = d[i];
                        out[count].track = t;
                        out[count].used = false;
                        count++;
                    }
                    i += 2;
                }
            }
        }
        pos = end;
    }
    return count;
}

static void report_file(const char *file, long ticks_per_quarter, bool has_tracks) {
    Imported imported = import_file(file, ticks_per_quarter);
    const MeterMap *map = imported.meter.meter_map;

    VoiceAssignment merged = {0};
    assert(voice_separate(imported.events, imported.pitches, imported.count, NULL, map,
                          standard_meter_segmentation_beat_grid, NULL, NULL, &merged) == VOICE_SEPARATION_OK);
    size_t flagged = 0;
    double confidence = 0.0;
    size_t by_voice[8] = {0};
    for (size_t i = 0; i < merged.count; i++) {
        flagged += merged.entries[i].chord_member ? 1 : 0;
        confidence += merged.entries[i].confidence;
        by_voice[merged.entries[i].voice]++;
    }
    printf("%s, %zu notes, one stream: %zu voices, %zu chord-member notes (%.1f%%), mean confidence %.2f\n", file,
           imported.count, merged.streams[0].voice_count, flagged,
           100.0 * (double)flagged / (double)imported.count, confidence / (double)imported.count);
    printf("  notes per voice:");
    for (size_t v = 0; v < merged.streams[0].voice_count; v++) {
        printf(" %zu", by_voice[v]);
    }
    printf("\n");

    if (has_tracks) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
        size_t size = 0;
        unsigned char *data = read_file(path, &size);
        MidiOn *ons = (MidiOn *)calloc(imported.count + 16, sizeof(MidiOn));
        size_t on_count = read_note_ons(data, size, ons, imported.count + 16);
        free(data);
        assert(on_count == imported.count);
        size_t *stream = (size_t *)calloc(imported.count, sizeof(size_t));
        for (size_t i = 0; i < imported.count; i++) {
            long tick = onset_ticks(&imported, i);
            long pitch = pitch_numeric_value_position_numerator(imported.pitches[i]);
            bool matched = false;
            for (size_t k = 0; k < on_count && !matched; k++) {
                if (!ons[k].used && ons[k].tick == tick && ons[k].pitch == pitch) {
                    ons[k].used = true;
                    stream[i] = ons[k].track;
                    matched = true;
                }
            }
            assert(matched);
        }
        VoiceAssignment tracks = {0};
        assert(voice_separate(imported.events, imported.pitches, imported.count, stream, map,
                              standard_meter_segmentation_beat_grid, NULL, NULL, &tracks) == VOICE_SEPARATION_OK);
        size_t tracks_flagged = 0;
        for (size_t i = 0; i < tracks.count; i++) {
            tracks_flagged += tracks.entries[i].chord_member ? 1 : 0;
        }
        printf("  per-track streams: %zu streams, %zu chord-member notes (%.1f%%); voices per stream:",
               tracks.stream_count, tracks_flagged, 100.0 * (double)tracks_flagged / (double)imported.count);
        for (size_t s = 0; s < tracks.stream_count; s++) {
            printf(" track %zu: %zu", tracks.streams[s].stream, tracks.streams[s].voice_count);
        }
        printf("\n  one-stream voice of each track's notes (rows: track; columns: voice 0..):\n");
        for (size_t s = 0; s < tracks.stream_count; s++) {
            size_t row[8] = {0};
            for (size_t i = 0; i < imported.count; i++) {
                if (stream[i] == tracks.streams[s].stream) {
                    row[merged.entries[i].voice]++;
                }
            }
            printf("    track %zu:", tracks.streams[s].stream);
            for (size_t v = 0; v < merged.streams[0].voice_count; v++) {
                printf(" %zu", row[v]);
            }
            printf("\n");
        }
        voice_assignment_destroy(&tracks);
        free(stream);
        free(ons);
    }
    printf("\n");

    voice_assignment_destroy(&merged);
    destroy_imported(&imported);
}

int main(void) {
    test_chorale();
    test_bach_114();
    report_file("Mozart 545.mid", 240, false);
    report_file("Field 1.mid", 240, true);
    report_file("Clementi 36.mid", 240, true);
    printf("test_voice_separation_corpus: ok\n");
    return 0;
}
