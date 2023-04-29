#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "io/midi_importer.h"
#include "meter_test_util.h"
#include "metric_position_rules.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/temporal_anchor.h"

/*
 * Integration: MusicalTime -> MeterMap -> MetricalPosition (kernel) -> beat, subdivision and
 * BeatStrength (standard-meter). Two parts: hand-built maps with a meter change, a short measure and
 * pickups, and the framework-v1-corpus files imported through the MIDI importer. URSATZ_CORPUS_DIR is
 * passed in by CMake.
 */

typedef struct {
    int measure;
    StandardMeterBeatPosition beat;
    StandardMeterLevel level;
    BeatStrength strength;
} Reading;

/* Reads the position of the time tn/td in the map, with the signature the map says that measure is
   read in. */
static Reading read_time(const MeterMap *map, long tn, unsigned long td) {
    MusicalTime *time = NULL;
    assert(musical_time_create(tn, td, &time) == MUSICAL_TIME_OK);
    MetricalPosition *position = NULL;
    assert(metrical_position_from_time(map, time, &position) == METRICAL_POSITION_OK);

    Reading reading;
    reading.measure = metrical_position_measure(position);
    const TimeSignature *signature = NULL;
    assert(metrical_position_reading_signature(map, reading.measure, &signature) == METRICAL_POSITION_OK);
    assert(standard_meter_beat_position(signature, position, &reading.beat));
    assert(standard_meter_metric_level(signature, position, &reading.level));
    assert(standard_meter_beat_strength(signature, position, &reading.strength));

    metrical_position_destroy(position);
    musical_time_destroy(time);
    return reading;
}

static void assert_reading(Reading r, int measure, unsigned beat, long sub_n, unsigned long sub_d,
                           StandardMeterLevel level, BeatStrength strength) {
    assert(r.measure == measure);
    assert(r.beat.beat == beat);
    assert(r.beat.subdivision_numerator == sub_n && r.beat.subdivision_denominator == sub_d);
    assert(r.level == level);
    assert(r.strength == strength);
}

typedef struct {
    unsigned top;
    int bottom_exponent;
    long start_n;
    unsigned long start_d;
} Change;

static MeterMap *build(const Change *changes, size_t count) {
    TimeSignature *signatures[4];
    MusicalTime *starts[4];
    MeterMapChangePoint points[4];
    for (size_t i = 0; i < count; i++) {
        signatures[i] = make_signature(changes[i].top, changes[i].bottom_exponent);
        assert(musical_time_create(changes[i].start_n, changes[i].start_d, &starts[i]) == MUSICAL_TIME_OK);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK
#define DOWN STANDARD_METER_LEVEL_DOWNBEAT
#define SECOND STANDARD_METER_LEVEL_SECONDARY_BEAT
#define BEAT STANDARD_METER_LEVEL_BEAT
#define OFF STANDARD_METER_LEVEL_OFFBEAT
#define FINE STANDARD_METER_LEVEL_FINER

static void test_meter_change(void) {
    /* 4/4 for four measures, then 3/4: the same half-measure offset reads differently in each. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 4, 1}};
    MeterMap *map = build(changes, 2);
    assert_reading(read_time(map, 7, 2), 4, 3, 0, 1, SECOND, M); /* beat 3 of 4/4, measure 4 */
    assert_reading(read_time(map, 4, 1), 5, 1, 0, 1, DOWN, S);   /* the change point is a downbeat */
    assert_reading(read_time(map, 9, 2), 5, 3, 0, 1, BEAT, W);   /* beat 3 of 3/4 is not secondary */
    assert_reading(read_time(map, 19, 4), 6, 1, 0, 1, DOWN, S);
    meter_map_destroy(map);
}

static void test_short_measure(void) {
    /* 4/4 until 5/2 whole notes, then 3/4: measure 3 is cut short. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 5, 2}};
    MeterMap *map = build(changes, 2);
    assert_reading(read_time(map, 2, 1), 3, 1, 0, 1, DOWN, S);
    assert_reading(read_time(map, 9, 4), 3, 2, 0, 1, BEAT, W);
    assert_reading(read_time(map, 5, 2), 4, 1, 0, 1, DOWN, S); /* the 3/4 measure starts at the cut */
    assert_reading(read_time(map, 11, 4), 4, 2, 0, 1, BEAT, W);
    meter_map_destroy(map);
}

static void test_pickup_in_four_four(void) {
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build(changes, 2);
    assert_reading(read_time(map, 0, 1), 0, 4, 0, 1, BEAT, W);  /* a quarter pickup is beat 4 */
    assert_reading(read_time(map, 1, 4), 1, 1, 0, 1, DOWN, S);  /* the first full measure, now in 3/4 */
    assert_reading(read_time(map, 1, 2), 1, 2, 0, 1, BEAT, W);
    meter_map_destroy(map);
}

static void test_pickup_before_twelve_eight(void) {
    /* A 1/8 region before 12/8: the pickup eighth is read in 12/8, at 11/8, the last eighth. */
    Change changes[] = {{1, 3, 0, 1}, {12, 3, 1, 8}};
    MeterMap *map = build(changes, 2);
    assert_reading(read_time(map, 0, 1), 0, 4, 2, 3, OFF, W);
    assert_reading(read_time(map, 1, 8), 1, 1, 0, 1, DOWN, S);
    assert_reading(read_time(map, 7, 8), 1, 3, 0, 1, SECOND, M);
    meter_map_destroy(map);
}

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    MidiMeterImport meter;
} Imported;

static Imported import_corpus_file(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, name);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)length);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);

    Imported imported = {NULL, NULL, 0, {NULL, NULL, NULL, 0}};
    assert(midi_importer_import_with_meter(data, (size_t)length, &imported.events, &imported.pitch_values,
                                           &imported.count, &imported.meter) == MIDI_IMPORT_OK);
    free(data);
    return imported;
}

static void release(Imported *imported) {
    for (size_t i = 0; i < imported->count; i++) {
        note_event_destroy(imported->events[i]);
        pitch_numeric_value_destroy(imported->pitch_values[i]);
    }
    free(imported->events);
    free(imported->pitch_values);
    midi_meter_import_destroy(&imported->meter);
}

/* The note starting at `tick` with MIDI pitch `midi_note`, read in the file's own meter. */
static Reading read_note(const Imported *imported, long tick, unsigned long ticks_per_whole, long midi_note) {
    MusicalTime *when = NULL;
    assert(musical_time_create(tick, ticks_per_whole, &when) == MUSICAL_TIME_OK);
    for (size_t i = 0; i < imported->count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(imported->events[i]));
        if (musical_time_equals(position, when) &&
            pitch_numeric_value_position_numerator(imported->pitch_values[i]) == midi_note) {
            musical_time_destroy(when);
            return read_time(imported->meter.meter_map, musical_time_numerator(position),
                             musical_time_denominator(position));
        }
    }
    assert(!"no note at that tick with that pitch");
    musical_time_destroy(when);
    Reading none = {0};
    return none;
}

/*
 * Mozart, Sonata K. 545, first movement, 4/4 (240 ticks per quarter, so a measure is 960 ticks).
 * Score, measure 1: no upbeat; the right hand is C5 (a half note) on beat 1, E5 on beat 3 and G5 on
 * beat 4, over a left-hand Alberti figure in eighth notes (C4 G4 E4 G4). The tick positions and
 * pitches are read from the file; the bar-1 rhythm is the printed opening of the score.
 * Measure 2 begins at tick 960 (left hand D4, right hand B4).
 */
static void test_mozart_545(void) {
    Imported imported = import_corpus_file("Mozart 545.mid");
    assert(!meter_map_is_defaulted(imported.meter.meter_map));

    assert_reading(read_note(&imported, 0, 960, 72), 1, 1, 0, 1, DOWN, S);   /* C5, m. 1 beat 1 */
    assert_reading(read_note(&imported, 0, 960, 60), 1, 1, 0, 1, DOWN, S);   /* C4 in the left hand */
    assert_reading(read_note(&imported, 120, 960, 67), 1, 1, 1, 2, OFF, W);  /* G4, the "and" of 1 */
    assert_reading(read_note(&imported, 480, 960, 76), 1, 3, 0, 1, SECOND, M); /* E5, m. 1 beat 3 */
    assert_reading(read_note(&imported, 720, 960, 79), 1, 4, 0, 1, BEAT, W);  /* G5, m. 1 beat 4 */
    assert_reading(read_note(&imported, 960, 960, 71), 2, 1, 0, 1, DOWN, S);  /* B4, m. 2 beat 1 */
    assert_reading(read_note(&imported, 1320, 960, 72), 2, 2, 1, 2, OFF, W);  /* C5, the "and" of 2 */
    assert_reading(read_note(&imported, 1380, 960, 74), 2, 2, 3, 4, FINE, W); /* D5, a sixteenth later */

    release(&imported);
}

/*
 * Field, Nocturne No. 1 in E-flat, 12/8 (240 ticks per quarter: a measure is 1440 ticks, a beat, a
 * dotted quarter, 360). The positions and pitches below are read from the file; they are NOT yet
 * checked against the printed score.
 * The first note (Bb4) starts at tick 1080, measure 1 beat 4: the file declares no pickup, so it is
 * measure 1 here, though if the score counts the opening as an upbeat it is measure 0 there (see
 * test_midi_corpus_meter.c). Measure 2 begins at tick 1440 with E-flat 3 in the left hand.
 */
static void test_field_1(void) {
    Imported imported = import_corpus_file("Field 1.mid");

    assert_reading(read_note(&imported, 1080, 960, 70), 1, 4, 0, 1, BEAT, W);   /* m. 1 beat 4 */
    assert_reading(read_note(&imported, 1440, 960, 51), 2, 1, 0, 1, DOWN, S);   /* Eb3, m. 2 beat 1 */
    assert_reading(read_note(&imported, 1560, 960, 67), 2, 1, 1, 3, OFF, W);    /* "la" */
    assert_reading(read_note(&imported, 1680, 960, 63), 2, 1, 2, 3, OFF, W);    /* "li" */
    assert_reading(read_note(&imported, 1800, 960, 58), 2, 2, 0, 1, BEAT, W);   /* m. 2 beat 2 */
    assert_reading(read_note(&imported, 2160, 960, 55), 2, 3, 0, 1, SECOND, M); /* m. 2 beat 3 */
    assert_reading(read_note(&imported, 2880, 960, 46), 3, 1, 0, 1, DOWN, S);   /* m. 3 beat 1 */
    assert_reading(read_note(&imported, 1450, 960, 75), 2, 1, 1, 36, FINE, W);  /* a melody note 10 ticks late */

    release(&imported);
}

int main(void) {
    test_meter_change();
    test_short_measure();
    test_pickup_in_four_four();
    test_pickup_before_twelve_eight();
    test_mozart_545();
    test_field_1();
    printf("test_metric_position_integration: ok\n");
    return 0;
}
