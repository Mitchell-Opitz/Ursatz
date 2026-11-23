/*
 * A framework rule check on the plan's own planned material (boundary rule 7, R-257): the cadence of the planned chords, for a cadence a request asks for. Its framework
 * inputs are built from the plan, not from a render, so it stays a framework call. What a listener finds in the
 * written music is heard through the Analyzer once per run (check/hearing.h).
 */
#include "harmony/harmony_internal.h"
#include "util/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/harmony/analysis_cadence.h>
#include <analysis/harmony/analysis_input.h>
#include <model/time/duration.h>
#include <model/time/metrical_position.h>
#include <model/time/musical_time.h>
#include <model/time/time_span.h>
#include <theory/tonality/seventh_in_key_rules.h>
#include <theory/tonality/triad_in_key_rules.h>

#define HEARD_KEY_REF "key"

/*
 * The chord as the recognizer wants it, a set of intervals above the root. The recognizer reads
 * the quality from the intervals, so each chord of a key gets the quality it has there: the
 * triad on each degree, and the seventh chord built on it (the dominant seventh on V, a fully
 * diminished seventh on the leading tone).
 */
bool harmony_diatonic_intervals(const ComposerChords *chords, HarmonyMode mode, const HarmonyChord *chord, int intervals[4], size_t *count) {
    const CommonPracticeMaterials *materials = chords != NULL ? &chords->materials : NULL;
    bool is_minor = mode == HARMONY_MODE_MINOR;
    /* A minor key's chords are the natural scale's, but the dominant is major and the leading-tone chord diminished. */
    bool raised = is_minor && (chord->degree == 5 || (chord->degree == 7 && chord->leading_tone));
    memset(intervals, 0, 4 * sizeof(int));
    if (!chord->seventh) {
        StandardTonalityTriadQuality triad;
        StandardTonalityTriadKey key = !is_minor ? STANDARD_TONALITY_TRIAD_KEY_MAJOR
                                       : raised ? STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC
                                                : STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL;
        if (!standard_tonality_triad_quality_in_key(key, chord->degree, &triad) ||
            !common_practice_triad_semitones(materials, triad, intervals)) {
            return false;
        }
        *count = 3;
        return true;
    }
    StandardTonalitySeventhQuality seventh;
    StandardTonalitySeventhKey key = !is_minor ? STANDARD_TONALITY_SEVENTH_KEY_MAJOR
                                     : raised ? STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE
                                              : STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL;
    if (!is_minor && chord->degree == 7) {
        /* Kept on purpose (R-582): the library owner reads the seventh on the leading tone of a major key as half
           diminished, the Composer has always played it fully diminished, and changing it changes what is played. */
        seventh = STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED;
    } else if (!standard_tonality_seventh_quality_in_key(key, chord->degree, &seventh)) {
        return false;
    }
    if (!common_practice_seventh_semitones(materials, seventh, intervals)) {
        return false;
    }
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

/* The soprano the requested cadence implies over chord i, or 0 for none. */
static int implied_soprano(const HeardPhrase *phrase, size_t i) {
    size_t last = phrase->count - 1;
    switch (phrase->requested) {
    case CADENCE_TYPE_PERFECT_AUTHENTIC:
        return i == last ? 1 : 0;
    case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
        return i == last ? 3 : 0;
    case CADENCE_TYPE_PLAGAL:
        return i == last || (last >= 2 && i == last - 2) ? 1 : 0;
    default:
        return 0;
    }
}

typedef struct {
    TimeSpan span;
    MetricalPosition *position;
    bool has_beat_length;
    Duration beat_length; /* a part cell's beat, when the recognizer should not read it as shorter than one */
} SegmentTimes;

struct SegmentSet {
    size_t count;
    HarmonySegmentInput inputs[HARMONY_MAX_CHORDS];
    char ids[HARMONY_MAX_CHORDS][12];
    char key_refs[HARMONY_MAX_CHORDS][16];
    SegmentTimes times[HARMONY_MAX_CHORDS];
    HarmonySegment *segments;
};

void harmony_segments_destroy(SegmentSet *set) {
    if (!set) {
        return;
    }
    for (size_t i = 0; i < HARMONY_MAX_CHORDS; i++) {
        metrical_position_destroy(set->times[i].position);
    }
    harmony_analysis_destroy(set->segments);
    free(set);
}

/* The times of the cells [first_cell, first_cell + cells) on the metre, and the bar and place in it where they start. */
static bool metre_times(const PhraseMetre *metre, size_t first_cell, size_t cells, Rational *start, Rational *end,
                        size_t *bar, Rational *offset) {
    return harmony_metre_cell_offset(metre, first_cell, start) &&
           harmony_metre_cell_offset(metre, first_cell + cells, end) &&
           harmony_metre_cell_position(metre, first_cell, bar, offset);
}

static bool times_create(const HeardPhrase *phrase, size_t first_cell, size_t cells, SegmentTimes *times) {
    Rational start;
    Rational end;
    Rational offset;
    size_t bar = first_cell / phrase->cells_per_bar;
    if (phrase->metre) {
        if (!metre_times(phrase->metre, first_cell, cells, &start, &end, &bar, &offset)) {
            return false;
        }
    } else if (rational_mul_int(phrase->cell_length, (long long)first_cell, &start) != RATIONAL_OK ||
        rational_mul_int(phrase->cell_length, (long long)(first_cell + cells), &end) !=
            RATIONAL_OK ||
        rational_mul_int(phrase->cell_length, (long long)(first_cell % phrase->cells_per_bar), &offset) != RATIONAL_OK) {
        return false;
    }
    Duration offset_duration;
    return time_span_make(musical_time_of(start), musical_time_of(end), &times->span) == TIME_SPAN_OK &&
           duration_of(offset, &offset_duration) == DURATION_OK &&
           metrical_position_create((int)bar + 1, offset_duration, &times->position) == METRICAL_POSITION_OK;
}

/* soprano[i] is the soprano degree over chord i, or NULL for the one the requested cadence implies. */
SegmentSet *harmony_segments_create_heard(const HeardPhrase *phrase, const int *soprano) {
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
        HarmonyMode mode = phrase->chord_modes ? phrase->chord_modes[i] : phrase->mode;
        snprintf(set->ids[i], sizeof(set->ids[i]), "c%zu", i);
        snprintf(set->key_refs[i], sizeof(set->key_refs[i]), phrase->chord_keys ? "key%u" : HEARD_KEY_REF,
                 phrase->chord_keys ? phrase->chord_keys[i] : 0u);
        int root_semitones;
        bool intervals_ok = chord->applied ? harmony_applied_intervals(&chord->applied_chord, input->intervals,
                                                               &input->interval_count)
                                           : composer_chord_intervals(phrase->composer_chords, mode, phrase->modal, &chord->chord,
                                                                      &root_semitones, input->intervals, &input->interval_count);
        if (!intervals_ok || !times_create(phrase, first_cell, chord->cells, &set->times[i])) {
            harmony_segments_destroy(set);
            return NULL;
        }
        input->segment.id = set->ids[i];
        input->segment.span = set->times[i].span;
        input->segment.position = set->times[i].position;
        input->segment.strength = phrase->metre ? harmony_metre_cell_strength(phrase->metre, first_cell)
                                             : harmony_rhythm_cell_strength(first_cell % phrase->cells_per_bar);
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
        input->bass_degree = harmony_member_degree(mode, chord);
        input->soprano_degree = soprano ? soprano[i] : implied_soprano(phrase, i);
        input->key_ref = set->key_refs[i];
        input->mode = mode;
        input->uncertainty = 0.0;
        input->has_beat_length = false;
        input->materials = phrase->composer_chords != NULL ? &phrase->composer_chords->materials : NULL;
        /* A part cell of a beat or more is long enough to arrive on, whatever the meter's beat is in whole notes. */
        if (phrase->metre && harmony_metre_cell_is_part(phrase->metre, first_cell) &&
            duration_make(1, (long long)phrase->metre->meter.beat_unit, &set->times[i].beat_length) == DURATION_OK) {
            input->has_beat_length = true;
            input->beat_length = set->times[i].beat_length;
        }
        first_cell += chord->cells;
    }
    if (harmony_analysis_prepare(set->inputs, set->count, &set->segments) != HARMONY_ANALYSIS_OK) {
        set->segments = NULL;
        harmony_segments_destroy(set);
        return NULL;
    }
    return set;
}

SegmentSet *harmony_segments_create(const HeardPhrase *phrase) {
    return harmony_segments_create_heard(phrase, NULL);
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

/* The key of the key lane over a chord, when one span holds it. */
static bool key_under(const Plan *plan, PlanSpan chord_span, LaneSpanId *id, SpelledKey *key) {
    LaneSpanId keys[2];
    if (plan_lane_find(plan, PLAN_LANE_REGION, 0, chord_span, keys, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, keys[0]), chord_span)) {
        return false;
    }
    *id = keys[0];
    *key = plan_lane_span_payload(plan, keys[0])->region;
    return true;
}

static bool read_lane(const Plan *plan, PlanSpan span, const HarmonyKind *kind, PhraseChord *chords, size_t *count, PhraseMetre *metre,
                      HarmonyMode *modes, unsigned *keys) {
    HarmonyMode mode = kind->mode;
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    if (!plan || !chords || !count || !metre || !harmony_read_metre(plan, span, metre)) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, HARMONY_MAX_CHORDS);
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    Rational position = span.start;
    for (size_t i = 0; i < found; i++) {
        PlanSpan chord_span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        HarmonyMode chord_mode = mode;
        HarmonyKind chord_kind = *kind;
        LaneSpanId key_id = PLAN_LANE_SPAN_NONE;
        SpelledKey key;
        if (modes && key_under(plan, chord_span, &key_id, &key)) {
            chord_mode = harmony_key_mode(&key);
            chord_kind = (HarmonyKind){chord_mode, NULL, NULL};
        }
        /* Chords must start where the last ended, in whole cells. */
        if (rational_cmp(chord_span.start, position) != 0 || plan_span_end(chord_span, &end) != PLAN_OK ||
            !harmony_read_chord_in_for(plan, ids[i], &chord_kind, metre, &chords[i])) {
            return false;
        }
        if (modes) {
            modes[i] = chord_mode;
            keys[i] = (unsigned)key_id;
        }
        position = end;
    }
    Rational span_end;
    if (plan_span_end(span, &span_end) != PLAN_OK || rational_cmp(position, span_end) != 0) {
        return false;
    }
    *count = found;
    return true;
}

bool harmony_read_lane_keys(const Plan *plan, PlanSpan span, HarmonyMode mode, PhraseChord *chords, size_t *count,
                            PhraseMetre *metre, HarmonyMode *modes, unsigned *keys) {
    HarmonyKind kind = {mode, NULL, NULL};
    return read_lane(plan, span, &kind, chords, count, metre, modes, keys);
}

bool harmony_read_lane_for(const Plan *plan, PlanSpan span, const HarmonyKind *kind, PhraseChord *chords, size_t *count,
                           PhraseMetre *metre) {
    return read_lane(plan, span, kind, chords, count, metre, NULL, NULL);
}

bool harmony_read_lane(const Plan *plan, PlanSpan span, HarmonyMode mode, PhraseChord *chords, size_t *count,
                       PhraseMetre *metre) {
    HarmonyKind kind = {mode, NULL, NULL};
    return read_lane(plan, span, &kind, chords, count, metre, NULL, NULL);
}

bool harmony_hear_cadence(const Plan *plan, PlanSpan span, HarmonyMode mode, CadenceType requested,
                          CadenceType *heard, bool *at_end) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t count;
    if (!heard || !at_end || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    HeardPhrase phrase = {chords, count, mode, requested, metre.cells_per_bar, metre.cell_length, false, NULL, false, false, NULL, NULL, false, &metre, NULL, NULL};
    return harmony_hear_phrase(&phrase, heard, at_end);
}

