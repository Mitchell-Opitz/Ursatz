#include "harmony/harmony_internal.h"

#include <stdio.h>
#include <string.h>

#include <analysis_cadence.h>
#include <analysis_input.h>
#include <time/duration.h>
#include <time/metrical_position.h>
#include <time/musical_time.h>
#include <time/time_span.h>

#define MAX_HEARD_CHORDS 32
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
 * The chord as the recognizer wants it, a set of intervals above the root: the
 * quality of the diatonic triad on the degree, and the dominant seventh on V.
 * Other sevenths are not written by this operator.
 */
static bool chord_intervals(PlanKeyMode mode, const HarmonyChord *chord, int intervals[4], size_t *count) {
    static const int MAJOR_TRIAD[3] = {0, 4, 7};
    static const int MINOR_TRIAD[3] = {0, 3, 7};
    static const int DIMINISHED_TRIAD[3] = {0, 3, 6};
    static const int DOMINANT_SEVENTH[4] = {0, 4, 7, 10};
    /* Quality by degree: 0 major, 1 minor, 2 diminished. */
    static const int MAJOR_QUALITY[8] = {0, 0, 1, 1, 0, 0, 1, 2};
    static const int MINOR_QUALITY[8] = {0, 1, 2, 0, 1, 0, 0, 2};
    memset(intervals, 0, 4 * sizeof(int));
    if (chord->seventh) {
        if (chord->degree != 5) {
            return false;
        }
        memcpy(intervals, DOMINANT_SEVENTH, sizeof(DOMINANT_SEVENTH));
        *count = 4;
        return true;
    }
    int quality = mode == PLAN_MODE_MINOR ? MINOR_QUALITY[chord->degree] : MAJOR_QUALITY[chord->degree];
    /* In a minor key the chord on the raised seventh degree is diminished; the subtonic is major. */
    if (mode == PLAN_MODE_MINOR && chord->degree == 7) {
        quality = chord->leading_tone ? 2 : 0;
    }
    const int *triad = quality == 0 ? MAJOR_TRIAD : (quality == 1 ? MINOR_TRIAD : DIMINISHED_TRIAD);
    memcpy(intervals, triad, 3 * sizeof(int));
    *count = 3;
    return true;
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

static void times_destroy(SegmentTimes *times, size_t count) {
    for (size_t i = 0; i < count; i++) {
        time_span_destroy(times[i].span);
        metrical_position_destroy(times[i].position);
        duration_destroy(times[i].offset);
        musical_time_destroy(times[i].end);
        musical_time_destroy(times[i].start);
    }
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

bool harmony_hear_phrase(const HeardPhrase *phrase, CadenceType *heard, bool *at_end) {
    if (!phrase || !heard || !at_end || phrase->count == 0 || phrase->count > MAX_HEARD_CHORDS ||
        phrase->cells_per_bar == 0) {
        return false;
    }
    HarmonySegmentInput inputs[MAX_HEARD_CHORDS];
    char ids[MAX_HEARD_CHORDS][12];
    SegmentTimes times[MAX_HEARD_CHORDS];
    memset(inputs, 0, sizeof(inputs));
    memset(times, 0, sizeof(times));
    HarmonySegment *segments = NULL;
    HarmonyCadences found = {NULL, 0};
    bool ok = false;
    size_t created = 0;

    size_t first_cell = 0;
    for (size_t i = 0; i < phrase->count; i++, created++) {
        const PhraseChord *chord = &phrase->chords[i];
        HarmonySegmentInput *input = &inputs[i];
        snprintf(ids[i], sizeof(ids[i]), "c%zu", i);
        if (!times_create(phrase, first_cell, chord->cells, &times[i]) ||
            !chord_intervals(phrase->mode, &chord->chord, input->intervals, &input->interval_count)) {
            created = i + 1;
            goto done;
        }
        input->segment_id = ids[i];
        input->span = times[i].span;
        input->position = times[i].position;
        input->beat_strength = harmony_rhythm_cell_strength(first_cell % phrase->cells_per_bar);
        input->degree = chord->chord.degree;
        input->inversion = 0;
        input->applied_to = 0;
        input->bass_degree = chord->chord.degree;
        input->soprano_degree = implied_soprano(phrase, i);
        input->key_ref = HEARD_KEY_REF;
        input->mode = harmony_mode_of(phrase->mode);
        input->uncertainty = 0.0;
        input->beat_length = NULL;
        first_cell += chord->cells;
    }
    if (harmony_analysis_prepare(inputs, phrase->count, &segments) != HARMONY_ANALYSIS_OK ||
        harmony_analysis_find_cadences(segments, phrase->count, NULL, &found) != HARMONY_CADENCE_ANALYSIS_OK ||
        found.count == 0) {
        goto done;
    }
    *heard = found.items[found.count - 1].type;
    *at_end = found.items[found.count - 1].arrival_last == phrase->count - 1;
    ok = true;

done:
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    times_destroy(times, created);
    return ok;
}

/* The chords of the harmony lane over span, from the spans written by this operator. */
bool harmony_hear_cadence(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PlanCadenceType requested,
                          CadenceType *heard, bool *at_end) {
    if (!plan || !heard || !at_end) {
        return false;
    }
    LaneSpanId meters[2];
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, meters, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, meters[0]), span)) {
        return false;
    }
    PlanMeter meter = plan_lane_span_payload(plan, meters[0])->meter;
    HeardPhrase phrase;
    PlanTime bar_length;
    if (!harmony_rhythm_cells_per_bar(&meter, &phrase.cells_per_bar) ||
        plan_time_make(meter.beats_per_bar, (unsigned long)meter.beat_unit, &bar_length) != PLAN_OK ||
        plan_time_make(bar_length.num, bar_length.den * phrase.cells_per_bar, &phrase.cell_length) != PLAN_OK) {
        return false;
    }

    LaneSpanId ids[MAX_HEARD_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_HEARD_CHORDS);
    if (count == 0 || count > MAX_HEARD_CHORDS) {
        return false;
    }
    PhraseChord chords[MAX_HEARD_CHORDS];
    PlanTime position = span.start;
    for (size_t i = 0; i < count; i++) {
        PlanSpan chord_span = plan_lane_span_span(plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        PlanTime end;
        PhraseChord *chord = &chords[i];
        memset(chord, 0, sizeof(*chord));
        /* Chords must start where the last ended, in whole cells. */
        if (plan_time_compare(chord_span.start, position) != 0 || plan_span_end(chord_span, &end) != PLAN_OK ||
            !harmony_roman_parse(mode, harmony->roman_numeral, &chord->chord)) {
            return false;
        }
        for (PlanTime length = {0, 1}; plan_time_compare(length, chord_span.length) < 0; chord->cells++) {
            if (plan_time_add(length, phrase.cell_length, &length) != PLAN_OK) {
                return false;
            }
            if (plan_time_compare(length, chord_span.length) > 0) {
                return false;
            }
        }
        position = end;
    }
    PlanTime span_end;
    if (plan_span_end(span, &span_end) != PLAN_OK || plan_time_compare(position, span_end) != 0) {
        return false;
    }
    phrase.chords = chords;
    phrase.count = count;
    phrase.mode = mode;
    phrase.requested = requested;
    return harmony_hear_phrase(&phrase, heard, at_end);
}
