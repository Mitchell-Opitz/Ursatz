#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/notation_reading.h>

#include "time/meter_map.h"
#include "time/musical_time.h"

/*
 * Notation readings: the parsers, scaling that keeps every note's identity, the meter map a reading sets (with and
 * without a pickup), a failed reading leaving the notes unchanged, and Bach 114 read in 3/4 at scale 2.
 */

static char path_buffer[512];

static const char *musicxml(const char *name) {
    snprintf(path_buffer, sizeof path_buffer, "%s/%s", URSATZ_MUSICXML_DIR, name);
    return path_buffer;
}

static void load(const char *name, ImportedNotes *notes) {
    assert(input_pipeline_import_file(musicxml(name), INPUT_FORMAT_MUSICXML, notes) == INPUT_PIPELINE_OK);
}

static bool time_is(const MusicalTime *time, long numerator, unsigned long denominator) {
    return musical_time_numerator(time) == numerator && musical_time_denominator(time) == denominator;
}

static NotationReading reading_of(const char *meter, const char *scale, const char *pickup) {
    NotationReading reading = notation_reading_none();
    assert(!meter || notation_reading_parse_meter(meter, &reading));
    assert(!scale || notation_reading_parse_scale(scale, &reading));
    assert(!pickup || notation_reading_parse_pickup(pickup, &reading));
    return reading;
}

static void test_parsers(void) {
    NotationReading r = notation_reading_none();
    assert(!r.has_meter && !r.has_pickup && r.scale_numerator == 1 && r.scale_denominator == 1);

    assert(notation_reading_parse_meter("3/4", &r) && r.has_meter && r.beats == 3 && r.beat_type == 4);
    assert(notation_reading_parse_meter("12/8", &r) && r.beats == 12 && r.beat_type == 8);
    assert(notation_reading_parse_meter("64/16", &r) && r.beats == 64 && r.beat_type == 16);

    NotationReading kept = r;
    const char *bad_meters[] = {"", "3", "3/", "/4", "3/3", "0/4", "65/4", "3/32", "3/0", "-3/4", "3/4x", " 3/4", "3 /4", NULL};
    for (int i = 0; bad_meters[i]; i++) {
        assert(!notation_reading_parse_meter(bad_meters[i], &r));
    }
    assert(!notation_reading_parse_meter(NULL, &r));
    assert(memcmp(&r, &kept, sizeof r) == 0); /* a failed parse leaves the reading untouched */

    struct {
        const char *text;
        long numerator;
        unsigned long denominator;
    } scales[] = {{"1/4", 1, 4}, {"1/2", 1, 2}, {"1", 1, 1}, {"2", 2, 1}, {"4", 4, 1}, {"2/1", 2, 1}};
    for (size_t i = 0; i < sizeof scales / sizeof scales[0]; i++) {
        assert(notation_reading_parse_scale(scales[i].text, &r));
        assert(r.scale_numerator == scales[i].numerator && r.scale_denominator == scales[i].denominator);
    }
    const char *bad_scales[] = {"", "0", "3", "8", "1/3", "1/8", "2/3", "3/2", "-2", "1/0", "x", "2x", NULL};
    for (int i = 0; bad_scales[i]; i++) {
        kept = r;
        assert(!notation_reading_parse_scale(bad_scales[i], &r));
        assert(memcmp(&r, &kept, sizeof r) == 0);
    }

    assert(notation_reading_parse_pickup("1/4", &r) && r.has_pickup && r.pickup_numerator == 1 && r.pickup_denominator == 4);
    assert(notation_reading_parse_pickup("3/8", &r) && r.pickup_numerator == 3 && r.pickup_denominator == 8);
    const char *bad_pickups[] = {"", "0", "0/4", "1/0", "-1/4", "1/", "/4", "1/4/2", "a", NULL};
    for (int i = 0; bad_pickups[i]; i++) {
        assert(!notation_reading_parse_pickup(bad_pickups[i], &r));
    }
    assert(r.pickup_numerator == 3 && r.pickup_denominator == 8);
}

/* Scale 2 doubles every start and duration and keeps everything else about each note. */
static void test_scale_keeps_identity(void) {
    ImportedNotes before;
    ImportedNotes notes;
    load("bach-114.musicxml", &before);
    load("bach-114.musicxml", &notes);
    assert(notes.count > 100);

    NotationReading reading = reading_of(NULL, "2", NULL);
    assert(notation_reading_apply(&notes, &reading) == NOTATION_READING_OK);
    assert(notes.count == before.count);
    for (size_t i = 0; i < notes.count; i++) {
        const NoteEvent *old = before.events[i];
        const NoteEvent *now = notes.events[i];
        assert(note_event_equals_by_id(old, now));
        assert(strcmp(entity_id_value(note_event_id(old)), entity_id_value(note_event_id(now))) == 0);
        const MusicalTime *old_start = temporal_anchor_position(note_event_temporal_anchor(old));
        const MusicalTime *now_start = temporal_anchor_position(note_event_temporal_anchor(now));
        assert(time_is(now_start, 2 * musical_time_numerator(old_start), musical_time_denominator(old_start)) ||
               (musical_time_denominator(old_start) % 2 == 0 &&
                time_is(now_start, musical_time_numerator(old_start), musical_time_denominator(old_start) / 2)));
        const Duration *old_length = note_event_duration(old);
        const Duration *now_length = note_event_duration(now);
        assert(duration_numerator(now_length) * (long)duration_denominator(old_length) ==
               2 * duration_numerator(old_length) * (long)duration_denominator(now_length));
        assert(pitch_identity_equals(note_event_pitch(old), note_event_pitch(now)));
        assert(pitch_spelling_equals(note_event_optional_pitch_spelling(old), note_event_optional_pitch_spelling(now)));
        assert((note_event_optional_voice(old) == NULL) == (note_event_optional_voice(now) == NULL));
        assert(provenance_reference_state(note_event_provenance(old)) ==
               provenance_reference_state(note_event_provenance(now)));
        assert(pitch_numeric_value_equals(before.pitch_values[i], notes.pitch_values[i]));
    }

    /* The end time doubles, and the existing 4/4 map keeps its meter, so there are half as many measures. */
    size_t old_measures = 0;
    size_t new_measures = 0;
    assert(meter_map_measure_count(before.meter.meter_map, before.meter.end_time, &old_measures) == METER_MAP_OK);
    assert(meter_map_measure_count(notes.meter.meter_map, notes.meter.end_time, &new_measures) == METER_MAP_OK);
    assert(new_measures >= 2 * old_measures - 1 && new_measures <= 2 * old_measures);
    assert(notes.source.reading_applied && notes.source.reading.scale_numerator == 2);
    assert(!before.source.reading_applied);

    /* Scale 1 changes nothing but is recorded. */
    NotationReading none = notation_reading_none();
    ImportedNotes same;
    load("bach-114.musicxml", &same);
    assert(notation_reading_apply(&same, &none) == NOTATION_READING_OK && same.source.reading_applied);
    for (size_t i = 0; i < same.count; i++) {
        assert(note_event_equals_by_value(same.events[i], before.events[i]));
    }

    imported_notes_destroy(&same);
    imported_notes_destroy(&notes);
    imported_notes_destroy(&before);
}

/* Tempo markings, dynamics and key signature events are scaled with the notes. */
static void test_scales_markings(void) {
    ImportedNotes before;
    ImportedNotes notes;
    load("markings.musicxml", &before);
    load("markings.musicxml", &notes);
    assert(before.source.tempo_marking_count >= 1 && before.source.dynamics_count >= 2);

    NotationReading reading = reading_of(NULL, "1/2", NULL);
    assert(notation_reading_apply(&notes, &reading) == NOTATION_READING_OK);
    bool later = false;
    for (size_t i = 0; i < notes.source.tempo_marking_count; i++) {
        const ImportedTempoMarking *old = &before.source.tempo_markings[i];
        const ImportedTempoMarking *now = &notes.source.tempo_markings[i];
        assert(now->position_numerator * (long)old->position_denominator * 2 ==
               old->position_numerator * (long)now->position_denominator);
        assert(strcmp(now->text ? now->text : "", old->text ? old->text : "") == 0);
        later = later || old->position_numerator > 0;
    }
    for (size_t i = 0; i < notes.source.dynamics_count; i++) {
        const ImportedDynamic *old = &before.source.dynamics[i];
        const ImportedDynamic *now = &notes.source.dynamics[i];
        assert(now->start_numerator * (long)old->start_denominator * 2 == old->start_numerator * (long)now->start_denominator);
        assert(now->end_numerator * (long)old->end_denominator * 2 == old->end_numerator * (long)now->end_denominator);
        later = later || old->start_numerator > 0;
    }
    assert(later);
    for (size_t i = 0; i < notes.key_signature_events.count; i++) {
        const MusicalTime *old = before.key_signature_events.events[i].position;
        const MusicalTime *now = notes.key_signature_events.events[i].position;
        assert(musical_time_numerator(now) * (long)musical_time_denominator(old) * 2 ==
               musical_time_numerator(old) * (long)musical_time_denominator(now));
    }

    imported_notes_destroy(&notes);
    imported_notes_destroy(&before);
}

/* 3/4 with a pickup: a pickup region (measure 0) of a quarter, then 3/4 from there. */
static void test_meter_with_pickup(void) {
    ImportedNotes notes;
    load("bach-114.musicxml", &notes);
    assert(!meter_map_has_pickup(notes.meter.meter_map));

    NotationReading reading = reading_of("3/4", NULL, "1/4");
    assert(notation_reading_apply(&notes, &reading) == NOTATION_READING_OK);
    const MeterMap *map = notes.meter.meter_map;
    assert(meter_map_region_count(map) == 2);
    assert(meter_map_has_pickup(map) && meter_map_first_measure_number(map) == 0);
    assert(meter_map_region_is_pickup(meter_map_region(map, 0)));
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(map, 1));
    assert(time_signature_top_number(signature) == 3 && time_signature_bottom_number(signature) == 4);
    assert(time_is(meter_map_region_start(meter_map_region(map, 1)), 1, 4));
    assert(!meter_map_is_defaulted(map));
    assert(notes.source.reading_applied && notes.source.reading.has_meter && notes.source.reading.has_pickup);

    /* No pickup: one region of the reading's meter. */
    ImportedNotes plain;
    load("bach-114.musicxml", &plain);
    NotationReading meter_only = reading_of("6/8", NULL, NULL);
    assert(notation_reading_apply(&plain, &meter_only) == NOTATION_READING_OK);
    assert(meter_map_region_count(plain.meter.meter_map) == 1 && !meter_map_has_pickup(plain.meter.meter_map));
    const TimeSignature *six_eight = meter_map_region_time_signature(meter_map_region(plain.meter.meter_map, 0));
    assert(time_signature_top_number(six_eight) == 6 && time_signature_bottom_number(six_eight) == 8);

    /* A pickup alone keeps the map's meter. */
    ImportedNotes alone;
    load("bach-114.musicxml", &alone);
    NotationReading pickup_only = reading_of(NULL, NULL, "1/8");
    assert(notation_reading_apply(&alone, &pickup_only) == NOTATION_READING_OK);
    assert(meter_map_has_pickup(alone.meter.meter_map) && meter_map_region_count(alone.meter.meter_map) == 2);
    const TimeSignature *kept = meter_map_region_time_signature(meter_map_region(alone.meter.meter_map, 1));
    assert(time_signature_top_number(kept) == 4 && time_signature_bottom_number(kept) == 4);

    imported_notes_destroy(&alone);
    imported_notes_destroy(&plain);
    imported_notes_destroy(&notes);
}

typedef struct {
    size_t count;
    long first_start_numerator;
    unsigned long first_start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    size_t regions;
    bool pickup;
    bool applied;
} Snapshot;

static Snapshot snapshot(const ImportedNotes *notes) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes->events[0]));
    Snapshot snapshot;
    memset(&snapshot, 0, sizeof snapshot); /* so the padding compares equal */
    snapshot.count = notes->count;
    snapshot.first_start_numerator = musical_time_numerator(start);
    snapshot.first_start_denominator = musical_time_denominator(start);
    snapshot.end_numerator = musical_time_numerator(notes->meter.end_time);
    snapshot.end_denominator = musical_time_denominator(notes->meter.end_time);
    snapshot.regions = meter_map_region_count(notes->meter.meter_map);
    snapshot.pickup = meter_map_has_pickup(notes->meter.meter_map);
    snapshot.applied = notes->source.reading_applied;
    return snapshot;
}

static void test_failure_leaves_notes_unchanged(void) {
    ImportedNotes notes;
    load("bach-114.musicxml", &notes);
    ImportedNotes reference;
    load("bach-114.musicxml", &reference);
    Snapshot before = snapshot(&notes);

    NotationReading too_long = reading_of("3/4", "2", "3/4"); /* a pickup of a whole measure */
    assert(notation_reading_apply(&notes, &too_long) == NOTATION_READING_ERR_INVALID);
    NotationReading longer = reading_of("3/4", NULL, "1/1");
    assert(notation_reading_apply(&notes, &longer) == NOTATION_READING_ERR_INVALID);
    NotationReading bad_scale = notation_reading_none();
    bad_scale.scale_numerator = 3;
    assert(notation_reading_apply(&notes, &bad_scale) == NOTATION_READING_ERR_INVALID);
    NotationReading bad_meter = notation_reading_none();
    bad_meter.has_meter = true;
    bad_meter.beats = 3;
    bad_meter.beat_type = 3;
    assert(notation_reading_apply(&notes, &bad_meter) == NOTATION_READING_ERR_INVALID);
    assert(notation_reading_apply(NULL, &too_long) == NOTATION_READING_ERR_NULL_ARGUMENT);
    assert(notation_reading_apply(&notes, NULL) == NOTATION_READING_ERR_NULL_ARGUMENT);

    Snapshot after = snapshot(&notes);
    assert(memcmp(&before, &after, sizeof before) == 0);
    for (size_t i = 0; i < notes.count; i++) {
        assert(note_event_equals_by_value(notes.events[i], reference.events[i]));
    }
    assert(!notes.source.reading_applied);

    imported_notes_destroy(&reference);
    imported_notes_destroy(&notes);
}

/*
 * Bach 114 is notated in 4/4 with 24 measures and read as a 3/4 minuet at twice the length: 64 measures of 3/4. Its
 * perfect authentic cadences arrive at 117/8 and 165/8 whole notes (measure 15 beat 3.5 and measure 21 beat 3.5 of
 * the 4/4 reading); scaled they arrive on beat 1 of measures 40 and 56. This checks positions only.
 */
static void test_bach_114_three_four(void) {
    ImportedNotes notes;
    load("bach-114.musicxml", &notes);
    NoteEvent *first_arrival = NULL;
    NoteEvent *second_arrival = NULL;
    for (size_t i = 0; i < notes.count; i++) {
        const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes.events[i]));
        if (time_is(start, 117, 8) && !first_arrival) {
            first_arrival = notes.events[i];
        }
        if (time_is(start, 165, 8) && !second_arrival) {
            second_arrival = notes.events[i];
        }
    }
    assert(first_arrival && second_arrival);
    const char *first_id = entity_id_value(note_event_id(first_arrival));
    const char *second_id = entity_id_value(note_event_id(second_arrival));
    char first_copy[128];
    char second_copy[128];
    snprintf(first_copy, sizeof first_copy, "%s", first_id);
    snprintf(second_copy, sizeof second_copy, "%s", second_id);

    NotationReading reading = reading_of("3/4", "2", NULL);
    assert(notation_reading_apply(&notes, &reading) == NOTATION_READING_OK);

    size_t measures = 0;
    assert(meter_map_measure_count(notes.meter.meter_map, notes.meter.end_time, &measures) == METER_MAP_OK);
    assert(measures == 64);

    const char *ids[2] = {first_copy, second_copy};
    int expected_measure[2] = {40, 56};
    long expected_numerator[2] = {117, 165};
    for (int k = 0; k < 2; k++) {
        const NoteEvent *arrival = NULL;
        for (size_t i = 0; i < notes.count; i++) {
            if (strcmp(entity_id_value(note_event_id(notes.events[i])), ids[k]) == 0) {
                arrival = notes.events[i];
            }
        }
        assert(arrival);
        const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(arrival));
        assert(time_is(start, expected_numerator[k], 4));
        int measure = 0;
        assert(meter_map_measure_index_at(notes.meter.meter_map, start, &measure) == METER_MAP_OK);
        assert(measure == expected_measure[k]);
        TimeSpan *span = NULL;
        assert(meter_map_measure_span(notes.meter.meter_map, measure, &span) == METER_MAP_OK);
        assert(musical_time_equals(time_span_start(span), start)); /* beat 1 of the measure */
        time_span_destroy(span);
    }

    imported_notes_destroy(&notes);
}

int main(void) {
    test_parsers();
    test_scale_keeps_identity();
    test_scales_markings();
    test_meter_with_pickup();
    test_failure_leaves_notes_unchanged();
    test_bach_114_three_four();
    printf("test_notation_reading: ok\n");
    return 0;
}
