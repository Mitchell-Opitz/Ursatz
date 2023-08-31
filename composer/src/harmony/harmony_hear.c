#include "harmony/harmony_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis_cadence.h>
#include <analysis_input.h>
#include <time/duration.h>
#include <time/metrical_position.h>
#include <time/musical_time.h>
#include <time/time_span.h>

#define HEARD_KEY_REF "key"

CadenceType harmony_cadence_type_of(PlanCadenceType type) {
    switch (type) {
    case PLAN_CADENCE_PAC:
        return CADENCE_TYPE_PERFECT_AUTHENTIC;
    case PLAN_CADENCE_IAC:
        return CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    case PLAN_CADENCE_HC:
        return CADENCE_TYPE_HALF;
    case PLAN_CADENCE_DC:
        return CADENCE_TYPE_DECEPTIVE;
    case PLAN_CADENCE_PC:
        return CADENCE_TYPE_PLAGAL;
    }
    return CADENCE_TYPE_UNKNOWN;
}

/*
 * The chord as the recognizer wants it, a set of intervals above the root. The recognizer reads
 * the quality from the intervals, so each chord of a key gets the quality it has there: the
 * triad on each degree, and the seventh chord built on it (the dominant seventh on V, a fully
 * diminished seventh on the leading tone).
 */
bool harmony_diatonic_intervals(PlanKeyMode mode, const HarmonyChord *chord, int intervals[4], size_t *count) {
    static const int MAJOR_TRIAD[3] = {0, 4, 7};
    static const int MINOR_TRIAD[3] = {0, 3, 7};
    static const int DIMINISHED_TRIAD[3] = {0, 3, 6};
    static const int MAJOR_SEVENTH[4] = {0, 4, 7, 11};
    static const int MINOR_SEVENTH[4] = {0, 3, 7, 10};
    static const int DOMINANT_SEVENTH[4] = {0, 4, 7, 10};
    static const int HALF_DIMINISHED_SEVENTH[4] = {0, 3, 6, 10};
    static const int DIMINISHED_SEVENTH[4] = {0, 3, 6, 9};
    /* Quality by degree: 0 major, 1 minor, 2 diminished, 3 dominant (a major triad with a minor seventh). */
    static const int MAJOR_QUALITY[8] = {0, 0, 1, 1, 0, 3, 1, 2};
    static const int MINOR_QUALITY[8] = {0, 1, 2, 0, 1, 3, 0, 3};
    int quality = mode == PLAN_MODE_MINOR ? MINOR_QUALITY[chord->degree] : MAJOR_QUALITY[chord->degree];
    /* In a minor key the chord on the raised seventh degree is diminished; the subtonic is a dominant seventh. */
    if (mode == PLAN_MODE_MINOR && chord->degree == 7 && chord->leading_tone) {
        quality = 2;
    }
    memset(intervals, 0, 4 * sizeof(int));
    if (!chord->seventh) {
        const int *triad = quality == 1 ? MINOR_TRIAD : (quality == 2 ? DIMINISHED_TRIAD : MAJOR_TRIAD);
        memcpy(intervals, triad, 3 * sizeof(int));
        *count = 3;
        return true;
    }
    const int *seventh = NULL;
    if (quality == 3) {
        seventh = DOMINANT_SEVENTH;
    } else if (quality == 2) {
        /* The leading-tone seventh is fully diminished; on ii of a minor key it is half diminished. */
        seventh = chord->degree == 7 ? DIMINISHED_SEVENTH : HALF_DIMINISHED_SEVENTH;
    } else if (quality == 1) {
        seventh = MINOR_SEVENTH;
    } else {
        seventh = MAJOR_SEVENTH;
    }
    memcpy(intervals, seventh, 4 * sizeof(int));
    *count = 4;
    return true;
}

bool harmony_applied_intervals(const HarmonyAppliedChord *applied, int intervals[4], size_t *count) {
    memset(intervals, 0, 4 * sizeof(int));
    switch (applied->quality) {
    case HARMONY_APPLIED_MAJOR_TRIAD:
        intervals[1] = 4;
        intervals[2] = 7;
        *count = 3;
        return true;
    case HARMONY_APPLIED_DOMINANT_SEVENTH:
        intervals[1] = 4;
        intervals[2] = 7;
        intervals[3] = 10;
        *count = 4;
        return true;
    case HARMONY_APPLIED_DIMINISHED_TRIAD:
        intervals[1] = 3;
        intervals[2] = 6;
        *count = 3;
        return true;
    case HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH:
        intervals[1] = 3;
        intervals[2] = 6;
        intervals[3] = 9;
        *count = 4;
        return true;
    case HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH:
        intervals[1] = 3;
        intervals[2] = 6;
        intervals[3] = 10;
        *count = 4;
        return true;
    default:
        return false;
    }
}

/* The scale degree of the bass: the chord's root, or its third, fifth or seventh by inversion. */
static int bass_degree_of(PlanKeyMode mode, const PhraseChord *chord) {
    if (chord->applied) {
        return (harmony_applied_root_degree(harmony_mode_of(mode), &chord->applied_chord) - 1 +
                2 * chord->applied_inversion) % 7 + 1;
    }
    return (chord->chord.degree - 1 + 2 * chord->chord.inversion) % 7 + 1;
}

/* The soprano the requested cadence implies over chord i, or 0 for none. */
static int implied_soprano(const HeardPhrase *phrase, size_t i) {
    size_t last = phrase->count - 1;
    switch (phrase->requested) {
    case PLAN_CADENCE_PAC:
        return i == last ? 1 : 0;
    case PLAN_CADENCE_IAC:
        return i == last ? 3 : 0;
    case PLAN_CADENCE_PC:
        return i == last || (last >= 2 && i == last - 2) ? 1 : 0;
    default:
        return 0;
    }
}

typedef struct {
    MusicalTime *start;
    MusicalTime *end;
    TimeSpan *span;
    Duration *offset;
    MetricalPosition *position;
} SegmentTimes;

struct SegmentSet {
    size_t count;
    HarmonySegmentInput inputs[HARMONY_MAX_CHORDS];
    char ids[HARMONY_MAX_CHORDS][12];
    SegmentTimes times[HARMONY_MAX_CHORDS];
    HarmonySegment *segments;
};

void harmony_segments_destroy(SegmentSet *set) {
    if (!set) {
        return;
    }
    for (size_t i = 0; i < HARMONY_MAX_CHORDS; i++) {
        time_span_destroy(set->times[i].span);
        metrical_position_destroy(set->times[i].position);
        duration_destroy(set->times[i].offset);
        musical_time_destroy(set->times[i].end);
        musical_time_destroy(set->times[i].start);
    }
    harmony_analysis_destroy(set->segments);
    free(set);
}

static bool times_create(const HeardPhrase *phrase, size_t first_cell, size_t cells, SegmentTimes *times) {
    PlanTime start;
    PlanTime end;
    PlanTime offset;
    if (plan_time_make(phrase->cell_length.num * (long)first_cell, phrase->cell_length.den, &start) != PLAN_OK ||
        plan_time_make(phrase->cell_length.num * (long)(first_cell + cells), phrase->cell_length.den, &end) !=
            PLAN_OK ||
        plan_time_make(phrase->cell_length.num * (long)(first_cell % phrase->cells_per_bar),
                       phrase->cell_length.den, &offset) != PLAN_OK) {
        return false;
    }
    return musical_time_create(start.num, start.den, &times->start) == MUSICAL_TIME_OK &&
           musical_time_create(end.num, end.den, &times->end) == MUSICAL_TIME_OK &&
           time_span_create(times->start, times->end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &times->span) ==
               TIME_SPAN_OK &&
           duration_create(offset.num, offset.den, &times->offset) == DURATION_OK &&
           metrical_position_create((int)(first_cell / phrase->cells_per_bar) + 1, times->offset,
                                    &times->position) == METRICAL_POSITION_OK;
}

SegmentSet *harmony_segments_create(const HeardPhrase *phrase) {
    if (!phrase || phrase->count == 0 || phrase->count > HARMONY_MAX_CHORDS || phrase->cells_per_bar == 0) {
        return NULL;
    }
    SegmentSet *set = calloc(1, sizeof(*set));
    if (!set) {
        return NULL;
    }
    set->count = phrase->count;
    size_t first_cell = 0;
    for (size_t i = 0; i < phrase->count; i++) {
        const PhraseChord *chord = &phrase->chords[i];
        HarmonySegmentInput *input = &set->inputs[i];
        snprintf(set->ids[i], sizeof(set->ids[i]), "c%zu", i);
        bool intervals_ok = chord->applied ? harmony_applied_intervals(&chord->applied_chord, input->intervals,
                                                               &input->interval_count)
                                           : harmony_diatonic_intervals(phrase->mode, &chord->chord, input->intervals,
                                                                &input->interval_count);
        if (!intervals_ok || !times_create(phrase, first_cell, chord->cells, &set->times[i])) {
            harmony_segments_destroy(set);
            return NULL;
        }
        input->segment_id = set->ids[i];
        input->span = set->times[i].span;
        input->position = set->times[i].position;
        input->beat_strength = harmony_rhythm_cell_strength(first_cell % phrase->cells_per_bar);
        if (chord->applied) {
            bool leading = chord->applied_chord.kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE;
            input->degree = leading ? 7 : 5;
            input->applied_to = chord->applied_chord.target;
            input->inversion = chord->applied_inversion;
        } else {
            input->degree = chord->chord.degree;
            input->applied_to = 0;
            input->inversion = chord->chord.inversion;
        }
        input->bass_degree = bass_degree_of(phrase->mode, chord);
        input->soprano_degree = implied_soprano(phrase, i);
        input->key_ref = HEARD_KEY_REF;
        input->mode = harmony_mode_of(phrase->mode);
        input->uncertainty = 0.0;
        input->beat_length = NULL;
        first_cell += chord->cells;
    }
    if (harmony_analysis_prepare(set->inputs, set->count, &set->segments) != HARMONY_ANALYSIS_OK) {
        set->segments = NULL;
        harmony_segments_destroy(set);
        return NULL;
    }
    return set;
}

const HarmonySegment *harmony_segments_get(const SegmentSet *set, size_t *count) {
    *count = set->count;
    return set->segments;
}

bool harmony_hear_phrase(const HeardPhrase *phrase, CadenceType *heard, bool *at_end) {
    if (!heard || !at_end) {
        return false;
    }
    SegmentSet *set = harmony_segments_create(phrase);
    if (!set) {
        return false;
    }
    size_t count;
    const HarmonySegment *segments = harmony_segments_get(set, &count);
    HarmonyCadences found = {NULL, 0};
    bool ok = harmony_analysis_find_cadences(segments, count, NULL, &found) == HARMONY_CADENCE_ANALYSIS_OK &&
              found.count > 0;
    if (ok) {
        *heard = found.items[found.count - 1].type;
        *at_end = found.items[found.count - 1].arrival_last == count - 1;
    }
    harmony_cadences_destroy(&found);
    harmony_segments_destroy(set);
    return ok;
}

bool harmony_read_lane(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PhraseChord *chords, size_t *count,
                       PhraseMetre *metre) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    if (!plan || !chords || !count || !metre || !harmony_read_metre(plan, span, metre)) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, HARMONY_MAX_CHORDS);
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    PlanTime position = span.start;
    for (size_t i = 0; i < found; i++) {
        PlanSpan chord_span = plan_lane_span_span(plan, ids[i]);
        PlanTime end;
        /* Chords must start where the last ended, in whole cells. */
        if (plan_time_compare(chord_span.start, position) != 0 || plan_span_end(chord_span, &end) != PLAN_OK ||
            !harmony_read_chord(plan, ids[i], mode, metre->cell_length, &chords[i])) {
            return false;
        }
        position = end;
    }
    PlanTime span_end;
    if (plan_span_end(span, &span_end) != PLAN_OK || plan_time_compare(position, span_end) != 0) {
        return false;
    }
    *count = found;
    return true;
}

bool harmony_hear_cadence(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PlanCadenceType requested,
                          CadenceType *heard, bool *at_end) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t count;
    if (!heard || !at_end || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    HeardPhrase phrase = {chords, count, mode, requested, metre.cells_per_bar, metre.cell_length, false};
    return harmony_hear_phrase(&phrase, heard, at_end);
}
