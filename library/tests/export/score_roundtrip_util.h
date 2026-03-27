#ifndef URSATZ_TESTS_EXPORT_SCORE_ROUNDTRIP_UTIL_H
#define URSATZ_TESTS_EXPORT_SCORE_ROUNDTRIP_UTIL_H

#include "score_test_util.h"

#include "io/musicxml_importer.h"
#include "pitch/pitch_numeric_value.h"

/*
 * Export an import through musicxml_export_write_score, import the document again, and compare everything the export
 * is meant to carry: the notes (position, duration, sounding pitch, spelling, part, staff), the parts, the MeterMap
 * regions and the measure count, the TempoMap regions, the key signatures, the tempo markings and the dynamics. The
 * `defaulted` flag of a map is not compared: a defaulted region is written as a declared <time> or <sound tempo>-less.
 */

typedef struct {
    MusicXmlExportPart *parts;
    MusicXmlExportKeySignature *keys;
} RoundTripHolder;

static inline void rt_fill_score(const MusicXmlImport *import, MusicXmlExportScore *score, RoundTripHolder *holder) {
    memset(score, 0, sizeof(*score));
    holder->parts = (MusicXmlExportPart *)calloc(import->part_count > 0 ? import->part_count : 1, sizeof(MusicXmlExportPart));
    holder->keys = (MusicXmlExportKeySignature *)calloc(import->key_signature_count > 0 ? import->key_signature_count : 1,
                                                        sizeof(MusicXmlExportKeySignature));
    assert(holder->parts != NULL && holder->keys != NULL);
    for (size_t i = 0; i < import->part_count; i++) {
        holder->parts[i].id = import->parts[i].id;
        holder->parts[i].name = import->parts[i].name;
        holder->parts[i].staff_count = import->parts[i].staff_count;
    }
    /* A <direction> may name a staff its part never declared (<staves> absent); the export refuses that, so the
       part is widened to hold it, and the re-import then declares the wider part. */
    for (size_t i = 0; i < import->dynamics_count; i++) {
        MusicXmlExportPart *part = &holder->parts[import->dynamics_part[i]];
        if (import->dynamics_staff[i] > part->staff_count) {
            part->staff_count = import->dynamics_staff[i];
        }
    }
    for (size_t i = 0; i < import->key_signature_count; i++) {
        const MusicXmlKeySignatureEvent *key = &import->key_signatures[i];
        holder->keys[i].sharps_flats = key->sharps_flats;
        holder->keys[i].mode = key->mode_hint == MUSICXML_MODE_HINT_MAJOR   ? MUSICXML_EXPORT_MODE_MAJOR
                               : key->mode_hint == MUSICXML_MODE_HINT_MINOR ? MUSICXML_EXPORT_MODE_MINOR
                                                                            : MUSICXML_EXPORT_MODE_UNKNOWN;
        holder->keys[i].position = key->position;
        holder->keys[i].part_index = key->part_index;
    }
    score->events = import->events;
    score->pitch_values = import->pitch_values;
    score->count = import->count;
    score->part_index = import->part_index;
    score->staff = import->staff;
    score->parts = import->part_count > 0 ? holder->parts : NULL;
    score->part_count = import->part_count;
    score->meter_map = import->meter_map;
    score->tempo_map = import->tempo_map;
    score->tempo_markings = import->tempo_markings;
    score->tempo_marking_part = import->tempo_marking_part;
    score->tempo_marking_count = import->tempo_marking_count;
    score->dynamics = import->dynamics;
    score->dynamics_part = import->dynamics_part;
    score->dynamics_staff = import->dynamics_staff;
    score->dynamics_count = import->dynamics_count;
    score->key_signatures = import->key_signature_count > 0 ? holder->keys : NULL;
    score->key_signature_count = import->key_signature_count;
}

typedef struct {
    long pos_n;
    unsigned long pos_d;
    long dur_n;
    unsigned long dur_d;
    size_t part;
    int staff;
    long midi;
    int letter;
    int accidental;
    int octave;
} RtNote;

static inline int rt_compare_notes(const void *left, const void *right) {
    const RtNote *a = (const RtNote *)left;
    const RtNote *b = (const RtNote *)right;
    long long position_a = (long long)a->pos_n * (long long)b->pos_d;
    long long position_b = (long long)b->pos_n * (long long)a->pos_d;
    if (position_a != position_b) {
        return position_a < position_b ? -1 : 1;
    }
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->staff != b->staff) {
        return a->staff < b->staff ? -1 : 1;
    }
    if (a->midi != b->midi) {
        return a->midi < b->midi ? -1 : 1;
    }
    long long duration_a = (long long)a->dur_n * (long long)b->dur_d;
    long long duration_b = (long long)b->dur_n * (long long)a->dur_d;
    if (duration_a != duration_b) {
        return duration_a < duration_b ? -1 : 1;
    }
    if (a->letter != b->letter) {
        return a->letter < b->letter ? -1 : 1;
    }
    if (a->accidental != b->accidental) {
        return a->accidental < b->accidental ? -1 : 1;
    }
    return a->octave < b->octave ? -1 : (a->octave > b->octave ? 1 : 0);
}

static inline RtNote *rt_notes(const MusicXmlImport *import) {
    RtNote *notes = (RtNote *)calloc(import->count > 0 ? import->count : 1, sizeof(RtNote));
    assert(notes != NULL);
    for (size_t i = 0; i < import->count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(import->events[i]));
        const Duration *duration = note_event_duration(import->events[i]);
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(import->events[i]);
        notes[i].pos_n = musical_time_numerator(position);
        notes[i].pos_d = musical_time_denominator(position);
        notes[i].dur_n = duration_numerator(duration);
        notes[i].dur_d = duration_denominator(duration);
        notes[i].part = import->part_index[i];
        notes[i].staff = import->staff[i];
        notes[i].midi = pitch_numeric_value_position_numerator(import->pitch_values[i]);
        notes[i].letter = (int)pitch_spelling_letter(spelling);
        notes[i].accidental = pitch_spelling_accidental(spelling);
        notes[i].octave = pitch_spelling_octave(spelling);
    }
    qsort(notes, import->count, sizeof(RtNote), rt_compare_notes);
    return notes;
}

/* The number of measures the MeterMap lays under the notes' last end, and the last end it used. */
static inline size_t rt_measure_count(const MusicXmlImport *import) {
    MusicalTime *last = tu_time(0, 1);
    for (size_t i = 0; i < import->count; i++) {
        MusicalTime *end = NULL;
        assert(musical_time_add(temporal_anchor_position(note_event_temporal_anchor(import->events[i])),
                                note_event_duration(import->events[i]), &end) == MUSICAL_TIME_OK);
        long long a = (long long)musical_time_numerator(end) * (long long)musical_time_denominator(last);
        long long b = (long long)musical_time_numerator(last) * (long long)musical_time_denominator(end);
        if (a > b) {
            musical_time_destroy(last);
            last = end;
        } else {
            musical_time_destroy(end);
        }
    }
    size_t count = 0;
    assert(meter_map_measure_count(import->meter_map, last, &count) == METER_MAP_OK);
    musical_time_destroy(last);
    return count;
}

static inline void rt_assert_same_meter(const MeterMap *a, const MeterMap *b) {
    assert(meter_map_region_count(a) == meter_map_region_count(b));
    assert(meter_map_has_pickup(a) == meter_map_has_pickup(b));
    assert(meter_map_first_measure_number(a) == meter_map_first_measure_number(b));
    for (size_t i = 0; i < meter_map_region_count(a); i++) {
        const MeterMapRegion *x = meter_map_region(a, i);
        const MeterMapRegion *y = meter_map_region(b, i);
        assert(time_signature_equals(meter_map_region_time_signature(x), meter_map_region_time_signature(y)));
        assert(musical_time_equals(meter_map_region_start(x), meter_map_region_start(y)));
        assert(meter_map_region_first_measure_number(x) == meter_map_region_first_measure_number(y));
        assert(meter_map_region_is_pickup(x) == meter_map_region_is_pickup(y));
    }
}

static inline void rt_assert_same_tempo(const TempoMap *a, const TempoMap *b) {
    assert(tempo_map_region_count(a) == tempo_map_region_count(b));
    for (size_t i = 0; i < tempo_map_region_count(a); i++) {
        const TempoMapRegion *x = tempo_map_region(a, i);
        const TempoMapRegion *y = tempo_map_region(b, i);
        assert(tempo_map_region_microseconds_per_quarter(x) == tempo_map_region_microseconds_per_quarter(y));
        assert(musical_time_equals(tempo_map_region_start(x), tempo_map_region_start(y)));
    }
}

static inline void rt_assert_same(const MusicXmlImport *a, const MusicXmlImport *b) {
    assert(a->count == b->count);
    RtNote *notes_a = rt_notes(a);
    RtNote *notes_b = rt_notes(b);
    for (size_t i = 0; i < a->count; i++) {
        assert(rt_compare_notes(&notes_a[i], &notes_b[i]) == 0);
    }
    free(notes_a);
    free(notes_b);

    assert(a->part_count == b->part_count);
    for (size_t i = 0; i < a->part_count; i++) {
        assert(strcmp(a->parts[i].id, b->parts[i].id) == 0 && strcmp(a->parts[i].name, b->parts[i].name) == 0);
        assert(b->parts[i].staff_count >= a->parts[i].staff_count); /* equal unless widened for a direction */
    }

    rt_assert_same_meter(a->meter_map, b->meter_map);
    assert(rt_measure_count(a) == rt_measure_count(b));
    rt_assert_same_tempo(a->tempo_map, b->tempo_map);

    assert(a->key_signature_count == b->key_signature_count);
    for (size_t i = 0; i < a->key_signature_count; i++) {
        assert(musical_time_equals(a->key_signatures[i].position, b->key_signatures[i].position));
        assert(a->key_signatures[i].sharps_flats == b->key_signatures[i].sharps_flats);
        assert(a->key_signatures[i].mode_hint == b->key_signatures[i].mode_hint);
        assert(a->key_signatures[i].part_index == b->key_signatures[i].part_index);
    }

    assert(a->tempo_marking_count == b->tempo_marking_count);
    for (size_t i = 0; i < a->tempo_marking_count; i++) {
        const TempoMarking *x = a->tempo_markings[i];
        const TempoMarking *y = b->tempo_markings[i];
        assert(musical_time_equals(temporal_anchor_position(tempo_marking_temporal_anchor(x)),
                                   temporal_anchor_position(tempo_marking_temporal_anchor(y))));
        assert(tempo_marking_has_text(x) == tempo_marking_has_text(y));
        if (tempo_marking_has_text(x)) {
            assert(strcmp(tempo_marking_text(x), tempo_marking_text(y)) == 0);
        }
        assert(tempo_marking_has_metronome_marking(x) == tempo_marking_has_metronome_marking(y));
        if (tempo_marking_has_metronome_marking(x)) {
            assert(metronome_marking_equals(tempo_marking_metronome_marking(x), tempo_marking_metronome_marking(y)));
        }
        assert(a->tempo_marking_part[i] == b->tempo_marking_part[i]);
    }

    assert(a->dynamics_count == b->dynamics_count);
    for (size_t i = 0; i < a->dynamics_count; i++) {
        assert(strcmp(dynamics_marking(a->dynamics[i]), dynamics_marking(b->dynamics[i])) == 0);
        assert(time_span_equals(dynamics_target_time_span(a->dynamics[i]), dynamics_target_time_span(b->dynamics[i])));
        assert(a->dynamics_part[i] == b->dynamics_part[i] && a->dynamics_staff[i] == b->dynamics_staff[i]);
    }
}

/* Writes the import as a score, reads it back, compares, and hands back the document (caller free()s it). */
static inline char *rt_round_trip(const MusicXmlImport *first) {
    MusicXmlExportScore score;
    RoundTripHolder holder;
    rt_fill_score(first, &score, &holder);
    char *xml = NULL;
    MusicXmlExportResult exported = musicxml_export_write_score(&score, &xml);
    if (exported != MUSICXML_EXPORT_OK) {
        printf("score export failed with status %d\n", (int)exported);
    }
    assert(exported == MUSICXML_EXPORT_OK);
    MusicXmlImport second;
    MusicXmlImportStatus status = musicxml_import_buffer(xml, strlen(xml), &second);
    if (status != MUSICXML_IMPORT_OK) {
        printf("re-import failed: %s\n", second.error_message);
    }
    assert(status == MUSICXML_IMPORT_OK);
    rt_assert_same(first, &second);
    musicxml_import_destroy(&second);
    free(holder.parts);
    free(holder.keys);
    return xml;
}

#endif
