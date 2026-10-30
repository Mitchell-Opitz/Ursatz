#ifndef URSATZ_TESTS_INTEGRATION_SCHEMA_UTIL_H
#define URSATZ_TESTS_INTEGRATION_SCHEMA_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "galant-schemas/analysis/galant_analysis_input.h"
#include "standard-tonality/analysis/chord_input.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"
#include "weighted_pipeline.h"

/*
 * Helpers for the schema and sequence test: the neutral per-segment input (galant_analysis_input.h) from a hand-written fixture, and the
 * fixture's MIDI file. The input from a piece's outer voices, keys and Roman numerals is chain_build_galant_segments (ursatz_chain).
 *
 * DEGREES. A pitch class is a degree of a key by the offset above its tonic, with the alteration against the major scale (galant_degree.h):
 *     0 1 2 3 4 5 6 7 8 9 10 11  ->  1  2- 2  3- 3  4  4+ 5  6- 6  7- 7
 * the same in major and minor keys (a minor key's third is 3 lowered). The key's own tonic is a degree of the home key by the same table.
 */

typedef struct {
    GalantSegmentList list;
    TimeSpan **spans; /* owned */
    size_t span_count;
    size_t span_capacity;
} SchemaBuild;

static inline void schema_build_init(SchemaBuild *build) {
    memset(build, 0, sizeof(*build));
    galant_segments_init(&build->list);
}

static inline void schema_build_destroy(SchemaBuild *build) {
    galant_segments_destroy(&build->list);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    free(build->spans);
    memset(build, 0, sizeof(*build));
}

static inline const TimeSpan *schema_build_span(SchemaBuild *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        build->span_capacity = build->span_capacity == 0 ? 256 : build->span_capacity * 2;
        build->spans = (TimeSpan **)realloc(build->spans, build->span_capacity * sizeof(TimeSpan *));
        assert(build->spans != NULL);
    }
    TimeSpan *span = NULL;
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    build->spans[build->span_count++] = span;
    return span;
}

static inline GalantPitchDegree schema_degree_of_offset(int semitones) {
    static const GalantPitchDegree TABLE[12] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0}, {4, 1}, {5, 0}, {6, -1}, {6, 0}, {7, -1}, {7, 0}};
    return TABLE[((semitones % 12) + 12) % 12];
}

/* --- the fixture --- */

typedef struct {
    int measure;
    double beat;
    double length;
    int key_pc;
    int bass;
    int soprano;
    int root_pc;
    char quality[4];
    int inversion;
    int applied;
} FixtureSegment;

typedef struct {
    FixtureSegment *segments;
    size_t count;
    int home;
} Fixture;

static inline Fixture schema_load_fixture(const char *path) {
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    fixture.segments = (FixtureSegment *)calloc(256, sizeof(FixtureSegment));
    assert(fixture.segments != NULL);
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        int beats = 0;
        int unit = 0;
        int home = 0;
        FixtureSegment *s = &fixture.segments[fixture.count];
        if (sscanf(line, "meter %d %d", &beats, &unit) == 2) {
            assert(beats == 4 && unit == 4);
        } else if (sscanf(line, "home %d", &home) == 1) {
            fixture.home = home;
        } else if (sscanf(line, "seg %d %lf %lf %d %d %d %d %3s %d %d", &s->measure, &s->beat, &s->length, &s->key_pc, &s->bass, &s->soprano,
                          &s->root_pc, s->quality, &s->inversion, &s->applied) == 10) {
            fixture.count++;
            assert(fixture.count < 256);
        }
    }
    free(text);
    assert(fixture.count > 0);
    return fixture;
}

static inline void schema_fixture_destroy(Fixture *fixture) {
    free(fixture->segments);
    memset(fixture, 0, sizeof(*fixture));
}

/* The time, in whole notes, of a measure and beat of a 4/4 fixture, in eighths. */
static inline long schema_eighths(double measure, double beat) {
    return (long)((measure - 1.0) * 8.0 + (beat - 1.0) * 2.0 + 0.5);
}

/* The segments of the fixture as the adapters' input: the keys, numerals and degrees as written. */
static inline void schema_build_from_fixture(SchemaBuild *build, const Fixture *fixture) {
    for (size_t i = 0; i < fixture->count; i++) {
        const FixtureSegment *f = &fixture->segments[i];
        long start = schema_eighths(f->measure, f->beat);
        long end = start + (long)(f->length * 2.0 + 0.5);
        MusicalTime *from = NULL;
        MusicalTime *to = NULL;
        assert(musical_time_create(start, 8, &from) == MUSICAL_TIME_OK && musical_time_create(end, 8, &to) == MUSICAL_TIME_OK);
        const TimeSpan *span = schema_build_span(build, from, to);
        musical_time_destroy(from);
        musical_time_destroy(to);

        GalantSegment segment;
        memset(&segment, 0, sizeof(segment));
        snprintf(segment.id, sizeof(segment.id), "fixture-%zu", i);
        snprintf(segment.key_id, sizeof(segment.key_id), "key-%d", f->key_pc);
        segment.span = span;
        segment.strength = f->beat == 1.0 ? BEAT_STRENGTH_STRONG : (f->beat == 3.0 ? BEAT_STRENGTH_MEDIUM : BEAT_STRENGTH_WEAK);
        segment.beat_numerator = 1;
        segment.beat_denominator = 4;
        segment.key = schema_degree_of_offset(f->key_pc - fixture->home);
        segment.has_bass = true;
        segment.bass = schema_degree_of_offset(f->bass - f->key_pc);
        segment.has_soprano = true;
        segment.soprano = schema_degree_of_offset(f->soprano - f->key_pc);
        segment.has_numeral = true;
        segment.root = schema_degree_of_offset(f->root_pc - f->key_pc);
        segment.quality = f->quality[0] == 'M' ? 1 : (f->quality[0] == 'm' ? 2 : (f->quality[0] == 'd' ? 3 : 1));
        segment.seventh = f->quality[0] == 'D';
        segment.inversion = f->inversion;
        segment.applied_to = f->applied;
        segment.uncertainty = 0.0;
        assert(galant_segments_add(&build->list, &segment) == GALANT_ANALYSIS_OK);
    }
}

/* --- a MIDI file from the fixture --- */

typedef struct {
    unsigned long tick;
    int order; /* note-offs before note-ons at one tick */
    unsigned char status;
    unsigned char key;
} MidiEvent;

static inline int schema_compare_midi(const void *a, const void *b) {
    const MidiEvent *x = (const MidiEvent *)a;
    const MidiEvent *y = (const MidiEvent *)b;
    if (x->tick != y->tick) {
        return x->tick < y->tick ? -1 : 1;
    }
    return (x->order > y->order) - (x->order < y->order);
}

static inline size_t schema_put_vlq(unsigned char *out, unsigned long value) {
    unsigned char bytes[5];
    size_t n = 0;
    bytes[n++] = (unsigned char)(value & 0x7f);
    while ((value >>= 7) != 0) {
        bytes[n++] = (unsigned char)((value & 0x7f) | 0x80);
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = bytes[n - 1 - i];
    }
    return n;
}

/* The chord tones (pitch classes of the root and its quality) not on the bass or the soprano, placed between them. */
static inline size_t schema_inner_pitches(const FixtureSegment *f, int *out) {
    static const int TRIAD[4][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 3, 6, -1}, {0, 4, 7, 10}};
    int kind = f->quality[0] == 'M' ? 0 : (f->quality[0] == 'm' ? 1 : (f->quality[0] == 'd' ? 2 : 3));
    size_t n = 0;
    for (int t = 0; t < 4; t++) {
        if (TRIAD[kind][t] < 0) {
            continue;
        }
        int pc = (f->root_pc + TRIAD[kind][t]) % 12;
        if (pc == f->bass % 12 || pc == f->soprano % 12) {
            continue;
        }
        int middle = (f->bass + f->soprano) / 2;
        int best = -1;
        for (int pitch = f->bass + 1; pitch < f->soprano; pitch++) {
            if (pitch % 12 == pc && (best < 0 || abs(pitch - middle) < abs(best - middle))) {
                best = pitch;
            }
        }
        if (best >= 0) {
            out[n++] = best;
        }
    }
    return n;
}

/* A format 0 standard MIDI file of the fixture, 480 ticks to the quarter, 4/4; the caller frees *out. */
static inline size_t schema_midi_bytes(const Fixture *fixture, unsigned char **out) {
    const unsigned long division = 480;
    MidiEvent *events = (MidiEvent *)calloc(fixture->count * 14, sizeof(MidiEvent));
    size_t n = 0;
    assert(events != NULL);
    for (size_t i = 0; i < fixture->count; i++) {
        const FixtureSegment *f = &fixture->segments[i];
        unsigned long start = (unsigned long)((((f->measure - 1) * 4.0) + (f->beat - 1.0)) * (double)division + 0.5);
        unsigned long end = start + (unsigned long)(f->length * (double)division + 0.5);
        int pitches[8];
        size_t count = 0;
        pitches[count++] = f->bass;
        pitches[count++] = f->soprano;
        count += schema_inner_pitches(f, pitches + count);
        for (size_t k = 0; k < count; k++) {
            events[n++] = (MidiEvent){start, 1, 0x90, (unsigned char)pitches[k]};
            events[n++] = (MidiEvent){end, 0, 0x80, (unsigned char)pitches[k]};
        }
    }
    qsort(events, n, sizeof(MidiEvent), schema_compare_midi);
    unsigned char *track = (unsigned char *)calloc(n * 8 + 64, 1);
    size_t t = 0;
    assert(track != NULL);
    const unsigned char header[] = {0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, 0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20};
    memcpy(track, header, sizeof(header));
    t = sizeof(header);
    unsigned long previous = 0;
    for (size_t i = 0; i < n; i++) {
        t += schema_put_vlq(track + t, events[i].tick - previous);
        previous = events[i].tick;
        track[t++] = events[i].status;
        track[t++] = events[i].key;
        track[t++] = events[i].status == 0x90 ? 80 : 0;
    }
    track[t++] = 0x00;
    track[t++] = 0xff;
    track[t++] = 0x2f;
    track[t++] = 0x00;
    unsigned char *bytes = (unsigned char *)malloc(t + 22);
    assert(bytes != NULL);
    memcpy(bytes, "MThd", 4);
    const unsigned char head[] = {0, 0, 0, 6, 0, 0, 0, 1, (unsigned char)(division >> 8), (unsigned char)(division & 0xff)};
    memcpy(bytes + 4, head, sizeof(head));
    memcpy(bytes + 14, "MTrk", 4);
    bytes[18] = (unsigned char)(t >> 24);
    bytes[19] = (unsigned char)(t >> 16);
    bytes[20] = (unsigned char)(t >> 8);
    bytes[21] = (unsigned char)t;
    memcpy(bytes + 22, track, t);
    free(track);
    free(events);
    *out = bytes;
    return t + 22;
}

/* --- reports --- */

static inline void schema_beat_length(const MeterMap *map, const MusicalTime *time, long *numerator, unsigned long *denominator) {
    const MeterMapRegion *region = NULL;
    *numerator = 1;
    *denominator = 4;
    if (meter_map_region_at(map, time, &region) != METER_MAP_OK) {
        return;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (signature == NULL || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return;
    }
    long n = duration_numerator(unit);
    unsigned long d = duration_denominator(unit);
    duration_destroy(unit);
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        n *= 3;
    }
    *numerator = n;
    *denominator = d;
}

/* The measure and beat (1-based, fractional) of a time, for reports. */
static inline void schema_where(const MeterMap *map, const MusicalTime *time, int *measure, double *beat) {
    MetricalPosition *position = NULL;
    *measure = 0;
    *beat = 0.0;
    if (metrical_position_from_time(map, time, &position) != METRICAL_POSITION_OK) {
        return;
    }
    long n = 1;
    unsigned long d = 4;
    schema_beat_length(map, time, &n, &d);
    const Duration *offset = metrical_position_offset(position);
    *measure = metrical_position_measure(position);
    *beat = 1.0 + ((double)duration_numerator(offset) / (double)duration_denominator(offset)) / ((double)n / (double)d);
    metrical_position_destroy(position);
}

#endif
