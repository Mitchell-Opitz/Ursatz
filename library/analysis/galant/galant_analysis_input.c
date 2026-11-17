#include "analysis/galant/galant_analysis_input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "model/pitch/midi_pitch.h"
#include "theory/tonality/tonal_key.h"

static int mod_int(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

static int semitones_of(GalantPitchDegree degree) {
    return standard_tonality_natural_degree_semitone(degree.degree) + degree.alteration;
}

/* The degree on letter distance `letter` (0 to 6) at `semitones` above the tonic (octaves dropped). */
static GalantPitchDegree make_degree(int letter, int semitones) {
    int alteration = mod_int(semitones - standard_tonality_natural_degree_semitone(letter + 1), 12);
    if (alteration >= 6) {
        alteration -= 12;
    }
    GalantPitchDegree result = {letter + 1, alteration};
    return result;
}

GalantPitchDegree galant_degree_plus(GalantPitchDegree a, GalantPitchDegree b) {
    return make_degree(mod_int((a.degree - 1) + (b.degree - 1), 7), semitones_of(a) + semitones_of(b));
}

GalantPitchDegree galant_degree_minus(GalantPitchDegree a, GalantPitchDegree b) {
    return make_degree(mod_int((a.degree - 1) - (b.degree - 1), 7), semitones_of(a) - semitones_of(b));
}

bool galant_degree_equal(GalantPitchDegree a, GalantPitchDegree b) {
    return a.degree == b.degree && a.alteration == b.alteration;
}

GalantPitchDegree galant_degree_in_home(GalantPitchDegree local, GalantPitchDegree segment_key) {
    return galant_degree_plus(local, segment_key);
}

GalantPitchDegree galant_degree_in_frame(GalantPitchDegree local, GalantPitchDegree segment_key, GalantPitchDegree frame) {
    return galant_degree_minus(galant_degree_plus(local, segment_key), frame);
}

void galant_segments_init(GalantSegmentList *list) {
    if (list != NULL) {
        memset(list, 0, sizeof(*list));
    }
}

static bool degree_ok(GalantPitchDegree degree) {
    return degree.degree >= 1 && degree.degree <= 7 && degree.alteration >= -2 && degree.alteration <= 2;
}

static bool starts_before(TimeSpan a, TimeSpan b) {
    return musical_time_cmp(a.start, b.start) < 0;
}

GalantAnalysisStatus galant_segments_add(GalantSegmentList *list, const GalantSegment *segment) {
    if (list == NULL || segment == NULL) {
        return GALANT_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (segment->segment.id == NULL || segment->segment.id[0] == '\0' || strlen(segment->segment.id) >= GALANT_ANALYSIS_ID_MAX ||
        memchr(segment->key_id, '\0', GALANT_ANALYSIS_ID_MAX) == NULL || !time_span_holds(segment->segment.span) ||
        !beat_strength_valid(segment->segment.strength) || segment->beat_numerator <= 0 || segment->beat_denominator == 0 ||
        !degree_ok(segment->key) || (segment->has_bass && !degree_ok(segment->bass)) ||
        (segment->has_soprano && !degree_ok(segment->soprano)) || (segment->has_numeral && !degree_ok(segment->root)) ||
        segment->inversion < 0 || segment->inversion > 3 || !(segment->uncertainty >= 0.0 && segment->uncertainty <= 1.0)) {
        return GALANT_ANALYSIS_ERR_INVALID;
    }
    if (list->count > 0 && starts_before(segment->segment.span, list->items[list->count - 1].segment.span)) {
        return GALANT_ANALYSIS_ERR_INVALID;
    }
    if (list->count == list->capacity) {
        size_t next = list->capacity == 0 ? 128 : list->capacity * 2;
        GalantSegment *grown = (GalantSegment *)realloc(list->items, next * sizeof(GalantSegment));
        if (grown == NULL) {
            return GALANT_ANALYSIS_ERR_ALLOC;
        }
        list->items = grown;
        list->capacity = next;
    }
    list->items[list->count++] = *segment;
    return GALANT_ANALYSIS_OK;
}

void galant_segments_destroy(GalantSegmentList *list) {
    if (list == NULL) {
        return;
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

