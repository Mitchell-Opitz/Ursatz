#include <stdio.h>
#include <string.h>

#include <stdint.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/variant_registry.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "util/plan_util.h"
#include "util/position.h"
#include "motive/motive_internal.h"

/*
 * The weights of the choices of a continuation by GrowDevelopment (ch. VIII, XI); the balanced rows are the default. A 0
 * removes the choice; when every choice a request allows is 0, the balanced row is used for that draw.
 */
/* Fragment 1's segment: a segment lettered other than `a` first, then one lettered `a`. */
static const unsigned FRAGMENT_SEGMENT_WEIGHTS[GROW_DEVELOP_COUNT][2] = {
    [GROW_DEVELOP_MELODY] = {1, 2}, [GROW_DEVELOP_BALANCED] = {2, 1}, [GROW_DEVELOP_THEME] = {3, 1}};
/* Fragment 2: fragment 1's segment in sequence one step down, then the next segment (exact). */
static const unsigned FRAGMENT_2_WEIGHTS[GROW_DEVELOP_COUNT][2] = {
    [GROW_DEVELOP_MELODY] = {1, 3}, [GROW_DEVELOP_BALANCED] = {3, 1}, [GROW_DEVELOP_THEME] = {1, 0}};
/* The steps a liquidation takes: from `least` (a motive with fewer than 2 segments cannot take 2) to `most`, 0 for as many as it has segments. */
static const struct {
    size_t least;
    size_t most;
} LIQUIDATION_STEPS[GROW_DEVELOP_COUNT] = {
    [GROW_DEVELOP_MELODY] = {1, 1}, [GROW_DEVELOP_BALANCED] = {1, 0}, [GROW_DEVELOP_THEME] = {2, 0}};

/* The segment of m0 as a fragment's motive: a proper part of m0 (a fragment shorter than it) of two notes or more. */
static bool segment_fragment(const GrowThemeSpec *spec, size_t index, Motive *out) {
    return grow_segment_motive(&spec->source, &spec->segments[index], out) && out->note_count < spec->source.note_count &&
           rational_cmp(out->length, spec->source.length) < 0;
}

/* Whether a fragment idea, made as the spec now says, fits the room. */
static bool fragment_fits(const GrowThemeSpec *spec, GrowPlace idea, Rational room) {
    Motive result;
    return grow_idea_develop(spec, idea, &result) && rational_cmp(result.length, room) <= 0;
}

static void clear_segment_fragments(GrowThemeSpec *spec) {
    spec->fragment_from_segment[0] = false;
    spec->fragment_from_segment[1] = false;
    spec->fragment_sequence = false;
    spec->fragment_cut[0] = 0;
    spec->fragment_cut[1] = 0;
    spec->fragment_diminished[0] = false;
    spec->fragment_diminished[1] = false;
}

/*
 * The fragments of a continuation from the segments of m0 (ch. VIII): fragment 1 is a segment that fits its room, one
 * lettered other than `a` preferred, and fragment 2 is that segment in sequence one step down or the next segment. False,
 * with nothing set, when m0 has no segment that fits: the leading fragments of m0 are made as before.
 */
static bool draw_segment_fragments(FillRng *rng, GrowDevelopment development, GrowThemeSpec *spec) {
    const unsigned *segment_weights = FRAGMENT_SEGMENT_WEIGHTS[development];
    const unsigned *sequence_weights = FRAGMENT_2_WEIGHTS[development];
    Rational room1 = spec->layout.length[GROW_PLACE_FRAGMENT_1];
    Rational room2 = spec->layout.length[GROW_PLACE_FRAGMENT_2];
    size_t fits[MOTIVE_MAX_FEATURES];
    size_t count = 0;
    unsigned total = 0;
    if (!spec->segment_room || grow_idea_own(spec, GROW_PLACE_FRAGMENT_1) || grow_idea_own(spec, GROW_PLACE_FRAGMENT_2)) {
        return false;
    }
    for (size_t s = 0; s < spec->segment_count; s++) {
        Motive motive;
        if (segment_fragment(spec, s, &motive) && rational_cmp(motive.length, room1) <= 0) {
            fits[count++] = s;
            total += segment_weights[spec->segments[s].label[0] == 'a' ? 1 : 0];
        }
    }
    if (count == 0) {
        return false;
    }
    uint64_t pick = fill_rng_below(rng, total);
    size_t chosen = fits[count - 1];
    for (size_t i = 0; i < count; i++) {
        unsigned weight = segment_weights[spec->segments[fits[i]].label[0] == 'a' ? 1 : 0];
        if (pick < weight) {
            chosen = fits[i];
            break;
        }
        pick -= weight;
    }
    bool sequence = fill_rng_below(rng, sequence_weights[0] + sequence_weights[1]) < sequence_weights[0];
    segment_fragment(spec, chosen, &spec->fragment_motive[0]);
    spec->fragment_from_segment[0] = true;
    spec->fragment_segment[0] = chosen;
    spec->fragment_from_segment[1] = true;
    for (int attempt = 0; attempt < 2; attempt++, sequence = !sequence) {
        spec->fragment_sequence = sequence;
        if (sequence) {
            spec->fragment_segment[1] = chosen;
            spec->fragment_motive[1] = spec->fragment_motive[0];
        } else if (chosen + 1 < spec->segment_count && segment_fragment(spec, chosen + 1, &spec->fragment_motive[1])) {
            spec->fragment_segment[1] = chosen + 1;
        } else {
            continue;
        }
        if (fragment_fits(spec, GROW_PLACE_FRAGMENT_2, room2)) {
            return true;
        }
    }
    clear_segment_fragments(spec);
    return false;
}

/* Whether any continuation setting is on: the continuation is then drawn by draw_accelerating_continuation. */
static bool continuation_on(const GrowContinuation *settings) {
    return settings->every_segment || settings->diminish || settings->liquidate || settings->fragment_lengths[GROW_FRAGMENT_HALF_BAR] > 0 ||
           settings->fragment_lengths[GROW_FRAGMENT_BEAT] > 0;
}

/* The count of leading notes of a segment whose onsets are before `cut`, when it is 2 or more and fewer than the segment's, else 0. */
static size_t cut_notes(const Motive *segment, Rational cut) {
    size_t count = 0;
    while (count < segment->note_count && rational_cmp(segment->notes[count].onset, cut) < 0) {
        count++;
    }
    return count >= 2 && count < segment->note_count ? count : 0;
}

/* A beat of the meter: 1/beat_unit, or the dotted beat, 3/beat_unit, in an eighth-note meter whose bar divides by 3. */
static Rational fragment_beat(const PlanMeter *meter) {
    Rational beat;
    long long count = meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0 ? 3 : 1;
    return rational_make(count, meter->beat_unit, &beat) == RATIONAL_OK ? beat : position_far();
}

/*
 * How long fragment k is (R-462): its whole segment, half a bar of it, or a beat of it, by the weights of
 * `settings->fragment_lengths` (all 0 reads as the whole segment alone; no draw when only that has weight, else one
 * fill_rng_below of their sum). A beat with under 2 notes falls back to half a bar, then to the whole segment; a cut the
 * library refuses, or that does not fit the idea's room, is the whole segment. A whole segment is its diminution 1:2 with
 * `settings->diminish`; the library refuses that of a one-bar segment in 3/4, which stays whole. fragment_motive[k] is the segment.
 */
static void draw_fragment_length(FillRng *rng, const GrowContinuation *settings, GrowThemeSpec *spec, size_t k) {
    GrowPlace idea = k == 0 ? GROW_PLACE_FRAGMENT_1 : GROW_PLACE_FRAGMENT_2;
    unsigned weights[GROW_FRAGMENT_LENGTH_COUNT];
    unsigned total = 0;
    for (int length = 0; length < GROW_FRAGMENT_LENGTH_COUNT; length++) {
        weights[length] = settings->fragment_lengths[length];
        total += weights[length];
    }
    if (total == 0) {
        weights[GROW_FRAGMENT_SEGMENT] = 1;
        total = 1;
    }
    GrowFragmentLength length = GROW_FRAGMENT_SEGMENT;
    if (total > weights[GROW_FRAGMENT_SEGMENT]) {
        uint64_t pick = fill_rng_below(rng, total);
        for (int i = 0; i < GROW_FRAGMENT_LENGTH_COUNT; i++) {
            if (pick < weights[i]) {
                length = (GrowFragmentLength)i;
                break;
            }
            pick -= weights[i];
        }
    }
    size_t cut = 0;
    if (length == GROW_FRAGMENT_BEAT) {
        cut = cut_notes(&spec->fragment_motive[k], fragment_beat(&spec->meter));
        length = cut > 0 ? GROW_FRAGMENT_SEGMENT : GROW_FRAGMENT_HALF_BAR;
    }
    if (length == GROW_FRAGMENT_HALF_BAR) {
        cut = cut_notes(&spec->fragment_motive[k], position_div(grow_bar_length(&spec->meter), 2));
    }
    spec->fragment_cut[k] = cut;
    spec->fragment_diminished[k] = cut == 0 && settings->diminish;
    if ((cut > 0 || spec->fragment_diminished[k]) && !fragment_fits(spec, idea, spec->layout.length[idea])) {
        spec->fragment_cut[k] = 0;
        spec->fragment_diminished[k] = false;
    }
}

/*
 * The fragments of an accelerating continuation (R-462): fragment 1 is a segment of m0 that fits a bar, the segment `avoid`
 * (SIZE_MAX for none) left out when another fits, weight 1 each with every_segment; fragment 2 is the first segment after
 * fragment 1's, round to the first and fragment 1's own last, that fits, exact. With `liquidate` it stops at fragment 1:
 * fragment 2 is the residue. False, with nothing set, when none fits.
 */
static bool draw_accelerating_fragments(FillRng *rng, const GrowThemeRequest *request, GrowThemeSpec *spec,
                                        size_t avoid) {
    const GrowContinuation *settings = &request->continuation;
    const unsigned *segment_weights = FRAGMENT_SEGMENT_WEIGHTS[request->development];
    Rational room1 = spec->layout.length[GROW_PLACE_FRAGMENT_1];
    Rational room2 = spec->layout.length[GROW_PLACE_FRAGMENT_2];
    size_t fits[MOTIVE_MAX_FEATURES];
    unsigned weights[MOTIVE_MAX_FEATURES];
    size_t count = 0;
    unsigned total = 0;
    if (!spec->segment_room || grow_idea_own(spec, GROW_PLACE_FRAGMENT_1) || grow_idea_own(spec, GROW_PLACE_FRAGMENT_2)) {
        return false;
    }
    for (int skip = avoid != SIZE_MAX; skip >= 0 && count == 0; skip--) {
        total = 0;
        for (size_t s = 0; s < spec->segment_count; s++) {
            Motive motive;
            if ((skip && s == avoid) || !segment_fragment(spec, s, &motive) || rational_cmp(motive.length, room1) > 0) {
                continue;
            }
            weights[count] = settings->every_segment ? 1 : segment_weights[spec->segments[s].label[0] == 'a' ? 1 : 0];
            fits[count++] = s;
            total += weights[count - 1];
        }
    }
    if (count == 0) {
        return false;
    }
    uint64_t pick = fill_rng_below(rng, total);
    size_t chosen = fits[count - 1];
    for (size_t i = 0; i < count; i++) {
        if (pick < weights[i]) {
            chosen = fits[i];
            break;
        }
        pick -= weights[i];
    }
    segment_fragment(spec, chosen, &spec->fragment_motive[0]);
    spec->fragment_from_segment[0] = true;
    spec->fragment_segment[0] = chosen;
    spec->fragment_from_segment[1] = true;
    spec->fragment_sequence = false;
    draw_fragment_length(rng, settings, spec, 0);
    if (settings->liquidate) {
        /* Fragment 2 is the liquidated residue (draw_accelerating_continuation), not a segment. */
        spec->fragment_from_segment[1] = false;
        return true;
    }
    if (settings->every_segment || spec->fragment_cut[0] > 0 || spec->fragment_diminished[0]) {
        /* Fragment 2 is the first segment after fragment 1's that fits, at its own length, exact. */
        for (size_t i = 1; i <= spec->segment_count; i++) {
            size_t next = (chosen + i) % spec->segment_count;
            if (segment_fragment(spec, next, &spec->fragment_motive[1])) {
                spec->fragment_segment[1] = next;
                draw_fragment_length(rng, settings, spec, 1);
                if (fragment_fits(spec, GROW_PLACE_FRAGMENT_2, room2)) {
                    return true;
                }
            }
        }
    } else {
        /* Fragment 1 is whole: fragment 2 is fragment 1's segment in sequence one step down, or the next segment. */
        const unsigned *sequence_weights = FRAGMENT_2_WEIGHTS[request->development];
        bool sequence = fill_rng_below(rng, sequence_weights[0] + sequence_weights[1]) < sequence_weights[0];
        for (int attempt = 0; attempt < 2; attempt++, sequence = !sequence) {
            spec->fragment_sequence = sequence;
            if (sequence) {
                spec->fragment_segment[1] = chosen;
                spec->fragment_motive[1] = spec->fragment_motive[0];
            } else if (chosen + 1 < spec->segment_count && segment_fragment(spec, chosen + 1, &spec->fragment_motive[1])) {
                spec->fragment_segment[1] = chosen + 1;
            } else {
                continue;
            }
            if (fragment_fits(spec, GROW_PLACE_FRAGMENT_2, room2)) {
                return true;
            }
        }
    }
    clear_segment_fragments(spec);
    return false;
}

/* The pitches an idea made by the motive-forms may use (grow_form.c), so the melody has room to place it. */
#define LIQUID_LOW 58
#define LIQUID_HIGH 84
/* The widest leap such an idea may have, a fifth, unless m0 has a wider one. */
#define LIQUID_LEAP 7

/*
 * Whether every pitch of the motive is in range and, when `in_scale`, in the key's scale (a minor key also has the raised
 * seventh). A residue keeps to the scale when m0 does (liquid_diatonic); a chromatic m0's residue keeps to the range only.
 */
static bool liquid_in_range(const Motive *motive, bool in_scale) {
    static const bool MAJOR[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    static const bool MINOR[12] = {true, false, true, true, false, true, false, true, true, false, true, true};
    for (size_t i = 0; i < motive->note_count; i++) {
        int pitch = motive_pitch(motive, i);
        if (pitch < LIQUID_LOW || pitch > LIQUID_HIGH) {
            return false;
        }
        if (in_scale && motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED &&
            !(motive->minor ? MINOR : MAJOR)[((pitch - motive->tonic_pc) % 12 + 12) % 12]) {
            return false;
        }
    }
    return true;
}

/* Whether every pitch of the motive is in its key's scale. */
static bool liquid_diatonic(const Motive *motive) {
    return grow_motive_in_range(motive, 0, 127, true);
}

static int widest_leap(const Motive *motive) {
    int widest = 0;
    for (size_t i = 1; i < motive->note_count; i++) {
        int leap = motive_pitch(motive, i) - motive_pitch(motive, i - 1);
        widest = (leap < 0 ? -leap : leap) > widest ? (leap < 0 ? -leap : leap) : widest;
    }
    return widest;
}

/*
 * The cadence rule of P33: an idea that ends the theme (or the first part) ends on the cadence's tone, the tonic for a PAC,
 * the third for an IAC, with the rhythm kept. The last note moves to the nearest such pitch (other = false) or the next
 * nearest on the other side (other = true); other cadences leave it. False when the motive has no key to read the tone in.
 */
static bool liquid_take_cadence(Motive *motive, bool has_cadence, CadenceType cadence, bool other) {
    if (!has_cadence || (cadence != CADENCE_TYPE_PERFECT_AUTHENTIC && cadence != CADENCE_TYPE_IMPERFECT_AUTHENTIC)) {
        return true;
    }
    if (motive->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        return false;
    }
    int want = (motive->tonic_pc + (cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? 0 : (motive->minor ? 3 : 4))) % 12;
    MotiveSketchNote *last = &motive->notes[motive->note_count - 1];
    int pitch = motive_pitch(motive, motive->note_count - 1);
    int shift = (((want - pitch) % 12) + 12) % 12;
    int nearest = shift > 6 ? shift - 12 : shift;
    last->pitch = pitch + (other ? (nearest > 0 ? nearest - 12 : nearest + 12) : nearest);
    return true;
}

/* The first interval of a motive in scale steps (signed), or in semitones when it has no key. */
static bool head_interval(const Motive *motive, int *steps, int *semitones) {
    MotiveAnalysis analysis;
    if (motive->note_count < 2 || !motive_analyze(motive, &analysis)) {
        return false;
    }
    *steps = analysis.intervals[0].has_steps ? analysis.intervals[0].steps : analysis.intervals[0].semitones;
    *semitones = analysis.intervals[0].semitones;
    return true;
}

/* Whether the first `count` notes of a have the durations of b's first, one to one or each half of b's (`shorten`). */
static bool same_head_rhythm(const Motive *a, const Motive *b, size_t count) {
    bool equal = true;
    bool halved = true;
    for (size_t i = 0; i < count; i++) {
        equal = equal && rational_cmp(a->notes[i].duration, b->notes[i].duration) == 0;
        halved = halved && rational_cmp(grow_scaled(a->notes[i].duration, 2), b->notes[i].duration) == 0;
    }
    return equal || halved;
}

/*
 * The segment of m0 a residue keeps (R-345): the first whose first two notes have the residue's first two durations, one
 * to one or all halved, and whose first interval is the residue's in scale steps, same direction and generic size.
 * SIZE_MAX when it keeps none.
 */
static size_t residue_segment(const Motive *residue, const GrowThemeSpec *spec) {
    int steps;
    int semitones;
    if (residue->note_count < 2 || !head_interval(residue, &steps, &semitones)) {
        return SIZE_MAX;
    }
    for (size_t s = 0; s < spec->segment_count; s++) {
        Motive segment;
        int segment_steps;
        int segment_semitones;
        if (!grow_segment_motive(&spec->source, &spec->segments[s], &segment) || segment.note_count < 2 ||
            !head_interval(&segment, &segment_steps, &segment_semitones)) {
            continue;
        }
        bool same_direction = (semitones > 0) == (segment_semitones > 0) && (semitones < 0) == (segment_semitones < 0);
        if (same_direction && steps == segment_steps && same_head_rhythm(residue, &segment, 2)) {
            return s;
        }
    }
    return SIZE_MAX;
}

/* Whether a residue keeps a segment's rhythm and head interval (R-345). */
static bool residue_keeps(const Motive *residue, const GrowThemeSpec *spec) {
    return residue_segment(residue, spec) != SIZE_MAX;
}

/*
 * The first liquidation of base, from `from` steps to `most` (0 for as many as base has features), that is a form the
 * motive-forms accept and keeps a segment: fewer notes than base, in range, no wider a leap than `allowed`, within the
 * room, and, when the idea ends in a cadence, its last note moved to the cadence's tone; and whose residue keeps a
 * segment's rhythm and head interval (residue_keeps), `segment` that segment. Each step is a liquidate-features
 * operation with steps 1 and shorten 1, as grow_form.c offers. False when none is.
 */
static bool liquidate_base(const GrowThemeSpec *spec, const Motive *base, size_t from, size_t most, int allowed, Rational room,
                           bool has_cadence, CadenceType cadence, size_t *steps, size_t *segment, Motive *out) {
    for (size_t count = from > 0 ? from : 1;
         count <= base->feature_count && count <= VARIANT_CHAIN_MAX && (most == 0 || count <= most); count++) {
        VariantChain chain;
        Motive result;
        VariantRecord record;
        memset(&chain, 0, sizeof(chain));
        for (size_t t = 0; t < count; t++) {
            if (!variant_step_init(&chain.steps[t], "liquidate-features") ||
                !variant_step_set(&chain.steps[t], "steps", 1, 0) || !variant_step_set(&chain.steps[t], "shorten", 1, 0)) {
                return false;
            }
        }
        chain.step_count = count;
        if (variant_apply_chain(base, &chain, 1, &result, &record) != VARIANT_OK || result.note_count < 1 ||
            result.note_count >= base->note_count) {
            continue;
        }
        /* A form below the range is taken up an octave, as the motive-forms do. */
        int lowest = 127;
        int highest = 0;
        for (size_t i = 0; i < result.note_count; i++) {
            lowest = motive_pitch(&result, i) < lowest ? motive_pitch(&result, i) : lowest;
            highest = motive_pitch(&result, i) > highest ? motive_pitch(&result, i) : highest;
        }
        if (lowest < LIQUID_LOW && highest + 12 <= LIQUID_HIGH) {
            for (size_t i = 0; i < result.note_count; i++) {
                if (result.notes[i].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
                    result.notes[i].pitch += 12;
                }
            }
        }
        for (int other = 0; other < 2; other++) {
            Motive moved = result;
            if (!liquid_take_cadence(&moved, has_cadence, cadence, other != 0)) {
                break;
            }
            if (moved.form == MOTIVE_SKETCH_FORM_FULL && liquid_in_range(&moved, liquid_diatonic(base)) &&
                widest_leap(&moved) <= allowed && rational_cmp(moved.length, room) <= 0 && residue_keeps(&moved, spec)) {
                *steps = count;
                *segment = residue_segment(&moved, spec);
                *out = moved;
                return true;
            }
        }
    }
    return false;
}

/*
 * base with notes first..last (a segment's head) left whole: its features are the runs of the other notes, split where a
 * segment of base begins or ends, so liquidate-features liquidates everything but that head. False when nothing is left
 * to liquidate.
 */
static bool around_head(const Motive *base, size_t first, size_t last, Motive *out) {
    *out = *base;
    out->feature_count = 0;
    size_t previous = base->feature_count + 1; /* the segment the run so far is in; feature_count for none */
    for (size_t i = 0; i < base->note_count; i++) {
        if (i >= first && i <= last) {
            previous = base->feature_count + 1;
            continue;
        }
        size_t holder = base->feature_count;
        for (size_t f = 0; f < base->feature_count; f++) {
            holder = base->features[f].first <= i && i <= base->features[f].last ? f : holder;
        }
        if (holder == previous && out->feature_count > 0) {
            out->features[out->feature_count - 1].last = i;
            continue;
        }
        if (out->feature_count == MOTIVE_MAX_FEATURES) {
            return false;
        }
        MotiveSketchFeature *feature = &out->features[out->feature_count++];
        if (holder < base->feature_count) {
            snprintf(feature->label, sizeof(feature->label), "%s", base->features[holder].label);
        } else {
            snprintf(feature->label, sizeof(feature->label), "x%zu", out->feature_count);
        }
        feature->first = i;
        feature->last = i;
        previous = holder;
    }
    return out->feature_count > 0;
}

/* Whether two motives have the same notes: count, pitches and durations. */
static bool same_notes(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (motive_pitch(a, i) != motive_pitch(b, i) || rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/*
 * The head residue (R-350), for a motive liquidate-features cannot keep a head in: three notes, a segment's first two
 * (whole, then each duration halved) and a close on base's last pitch, moved to the cadence's tone (nearest, then the
 * other side), else in the octave nearest the head's second note. The close lasts base's last duration, else the head's second, the first that keeps the residue within the
 * room. Accepted when in range, no wider a leap than `allowed`, it keeps a segment (residue_keeps) and is not base
 * itself; `steps` is base's feature count (at least 1). No draw: the first accepted wins.
 */
static bool head_residue(const GrowThemeSpec *spec, const Motive *base, int allowed, Rational room, bool has_cadence,
                         CadenceType cadence, size_t *steps, size_t *segment, Motive *out) {
    for (size_t f = 0; f < base->feature_count; f++) {
        const MotiveSketchFeature *feature = &base->features[f];
        if (feature->last <= feature->first || feature->first + 1 >= base->note_count) {
            continue;
        }
        for (int halved = 0; halved < 2; halved++) {
            Motive residue = *base;
            residue.note_count = 3;
            for (size_t i = 0; i < 2; i++) {
                residue.notes[i] = base->notes[feature->first + i];
                if (halved) {
                    residue.notes[i].duration = position_div(residue.notes[i].duration, 2);
                }
            }
            residue.notes[0].onset = POSITION_ZERO;
            residue.notes[1].onset = residue.notes[0].duration;
            residue.notes[2].pitch = base->notes[base->note_count - 1].pitch;
            residue.notes[2].onset = position_add(residue.notes[1].onset, residue.notes[1].duration);
            const Rational closes[2] = {base->notes[base->note_count - 1].duration, residue.notes[1].duration};
            bool fits = false;
            for (size_t c = 0; c < 2 && !fits; c++) {
                residue.notes[2].duration = closes[c];
                residue.length = position_add(residue.notes[2].onset, closes[c]);
                fits = rational_cmp(residue.length, room) <= 0;
            }
            if (!fits) {
                continue;
            }
            residue.feature_count = 1;
            residue.features[0] = *feature;
            residue.features[0].first = 0;
            residue.features[0].last = 1;
            residue.chord_count = 0;
            for (size_t k = 0; k < base->chord_count; k++) {
                if (rational_cmp(base->chords[k].onset, residue.notes[2].onset) < 0) {
                    residue.chords[residue.chord_count++] = base->chords[k];
                }
            }
            /* The close on base's last pitch (to the cadence's nearest tone), on the other side, then base's last pitch class
               in the octave nearest the head's second note (to the cadence's nearest tone). */
            for (int other = 0; other < 3; other++) {
                Motive moved = residue;
                if (other == 2) {
                    int from = moved.notes[1].pitch;
                    int shift = ((moved.notes[2].pitch - from) % 12 + 12) % 12;
                    moved.notes[2].pitch = from + (shift > 6 ? shift - 12 : shift);
                }
                if (!liquid_take_cadence(&moved, has_cadence, cadence, other == 1)) {
                    break;
                }
                if (liquid_in_range(&moved, liquid_diatonic(base)) && widest_leap(&moved) <= allowed &&
                    residue_keeps(&moved, spec) && !same_notes(&moved, base)) {
                    *steps = base->feature_count > 0 ? base->feature_count : 1;
                    *segment = residue_segment(&moved, spec);
                    *out = moved;
                    return true;
                }
            }
        }
    }
    return false;
}

/*
 * Liquidates the segments of m0 (ch. VIII) one step at a time, from `from` steps (at least the development's least) to as
 * many as m0 has segments (liquidate_base). When no count keeps a segment, the draw is widened (R-345): each segment of m0
 * in turn keeps its head (the whole segment, then one note fewer, down to two) and the rest of m0 is liquidated around
 * it, every step count from `from`. Last, the head residue (head_residue, `head` set). False when none keeps one.
 */
static bool liquidate_segments(const GrowThemeSpec *spec, GrowDevelopment development, size_t from, Rational room,
                               bool has_cadence, CadenceType cadence, size_t *steps, size_t *segment, bool *head,
                               Motive *out) {
    size_t least = LIQUIDATION_STEPS[development].least;
    size_t most = LIQUIDATION_STEPS[development].most;
    const Motive *base = &spec->liquid_base;
    int allowed = widest_leap(base) > LIQUID_LEAP ? widest_leap(base) : LIQUID_LEAP;
    *head = false;
    if (!spec->has_liquid_base) {
        return false;
    }
    if (spec->segment_count >= 2 && liquidate_base(spec, base, from < least ? least : from, most, allowed, room, has_cadence,
                                                   cadence, steps, segment, out)) {
        return true;
    }
    for (size_t kept = 0; kept < base->feature_count; kept++) {
        const MotiveSketchFeature *keep = &base->features[kept];
        for (size_t last = keep->last; last > keep->first; last--) {
            Motive around;
            if (around_head(base, keep->first, last, &around) &&
                liquidate_base(spec, &around, from, 0, allowed, room, has_cadence, cadence, steps, segment, out)) {
                return true;
            }
        }
    }
    /* A motive too short to keep a head in by liquidate-features gets a head residue (R-350). */
    *head = head_residue(spec, base, allowed, room, has_cadence, cadence, steps, segment, out);
    return *head;
}

/* Keeps the liquidation as the idea's motive: the idea restates it exactly (grow_idea_dev). */
static void keep_liquidation(GrowThemeSpec *spec, GrowPlace idea, size_t steps, size_t segment, bool head,
                             const Motive *result) {
    spec->liquidated[idea] = *result;
    spec->liquidation_head[idea] = head;
    spec->has_liquidated[idea] = true;
    spec->liquidation_steps[idea] = steps;
    spec->liquidation_segment[idea] = segment;
}

/* The room the idea at a place has in the material: its own length, or the bars of a derived motive if shorter. */
static Rational idea_room(const GrowThemeSpec *spec, GrowPlace place) {
    Rational idea = spec->layout.length[place];
    Rational bar = grow_bar_length(&spec->meter);
    Rational node = grow_scaled(bar, grow_motive_layout_bars(&spec->source, bar));
    return rational_cmp(idea, node) < 0 ? idea : node;
}

/* The longest leading fragment of at most `wanted` notes that fits an idea of `room`; 0 when none does. */
static size_t fitting_fragment(GrowThemeSpec *spec, size_t slot, size_t wanted, Rational room) {
    GrowPlace idea = slot == 0 ? GROW_PLACE_FRAGMENT_1 : GROW_PLACE_FRAGMENT_2;
    size_t most = grow_idea_source(spec, idea)->note_count - 1;
    for (size_t length = wanted < most ? wanted : most; length >= 1; length--) {
        Motive result;
        spec->fragment_notes[slot] = length;
        if (grow_idea_develop(spec, idea, &result) && rational_cmp(result.length, room) <= 0) {
            return length;
        }
    }
    return 0;
}

/* Develops an idea of the ending from m0 and keeps it if it fits its place in the layout. */
static bool keep_idea(GrowThemeSpec *spec, GrowPlace idea) {
    if (!grow_idea_develop(spec, idea, &spec->developed[idea]) ||
        rational_cmp(spec->developed[idea].length, spec->layout.length[idea]) > 0) {
        return false;
    }
    spec->has_developed[idea] = true;
    return true;
}

/* The continuation's harmonic rhythm: a phrase model if it has room for one, else the cadential progression alone. */
static void set_continuation_harmony(const GrowThemeRequest *request, GrowThemeSpec *spec) {
    size_t cells = spec->layout.ending_bars * request->cells_per_bar;
    spec->ending_shape = cells >= harmony_phrase_room_cells(request->cadence, request->cells_per_bar)
                             ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                             : PLAN_HARMONY_SHAPE_CADENTIAL;
    /* Its chords change at least twice as fast as the presentation's longest could, and still deal the cells out. */
    size_t cap = spec->basic_cells / 2 > 1 ? spec->basic_cells / 2 : 1;
    size_t room = cells > request->cells_per_bar ? cells - request->cells_per_bar : 1;
    size_t chords = grow_chords_before_last(request, spec->ending_shape, request->cadence);
    size_t least = (room + chords - 1) / chords;
    spec->ending_cap = (int)(cap > least ? cap : least);
    spec->ending_split = grow_split_possible(request, spec->layout.ending_bars, spec->ending_shape, request->cadence);
}

/*
 * The rest of a continuation once its fragments are kept: the cadential idea liquidates the segments of m0 from `from`
 * steps until a residue fits, keeps a segment and meets the cadence (a draw with no such residue fails, R-345); each idea
 * after the presentation is made from m0 and has to fit in the room it has; then the harmonic rhythm.
 */
static bool finish_continuation(const GrowThemeRequest *request, GrowThemeSpec *spec, size_t from) {
    if (!grow_idea_own(spec, GROW_PLACE_CADENTIAL)) {
        size_t steps;
        size_t segment;
        bool head;
        Motive result;
        if (!liquidate_segments(spec, request->development, from, idea_room(spec, GROW_PLACE_CADENTIAL),
                                true, request->cadence, &steps, &segment, &head, &result)) {
            return false;
        }
        keep_liquidation(spec, GROW_PLACE_CADENTIAL, steps, segment, head, &result);
    }
    for (GrowPlace idea = GROW_PLACE_FRAGMENT_1; idea < GROW_PLACE_COUNT; idea++) {
        if (!keep_idea(spec, idea)) {
            return false;
        }
    }
    set_continuation_harmony(request, spec);
    /* A key opens on a phrase model's pivot chord. */
    return !request->has_end_change || spec->ending_shape == PLAN_HARMONY_SHAPE_PHRASE_MODEL;
}

/* A continuation by the settings of request->continuation (R-452): the fragments by draw_accelerating_fragments, else the leading ones. */
static bool draw_accelerating_continuation(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec,
                                           size_t first, size_t second) {
    const GrowContinuation *settings = &request->continuation;
    bool liquidated = false;
    size_t avoid = SIZE_MAX;
    size_t from = 1;
    if (settings->liquidate && spec->segment_room && !grow_idea_own(spec, GROW_PLACE_FRAGMENT_2)) {
        /* Fragment 2 first: m0 liquidated until a residue fits its bar and keeps a segment; the cadential idea takes it further. */
        size_t steps;
        size_t segment;
        bool head;
        Motive result;
        if (!liquidate_segments(spec, request->development, 1, idea_room(spec, GROW_PLACE_FRAGMENT_2),
                                false, request->cadence, &steps, &segment, &head, &result)) {
            return false;
        }
        keep_liquidation(spec, GROW_PLACE_FRAGMENT_2, steps, segment, head, &result);
        liquidated = true;
        from = steps;
        avoid = settings->every_segment ? segment : SIZE_MAX;
    }
    if (!draw_accelerating_fragments(rng, request, spec, avoid) &&
        (fitting_fragment(spec, 0, first, spec->layout.length[GROW_PLACE_FRAGMENT_1]) == 0 ||
         (!liquidated &&
          fitting_fragment(spec, 1, second, spec->layout.length[GROW_PLACE_FRAGMENT_2]) == 0))) {
        return false;
    }
    return finish_continuation(request, spec, from);
}

static bool draw_continuation(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    size_t first = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t second = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    if (continuation_on(&request->continuation)) {
        return draw_accelerating_continuation(request, rng, spec, first, second);
    }
    /* Fragments are segments of m0 when it has them that fit, else its leading notes. */
    if (!draw_segment_fragments(rng, request->development, spec) &&
        (fitting_fragment(spec, 0, first, spec->layout.length[GROW_PLACE_FRAGMENT_1]) == 0 ||
         fitting_fragment(spec, 1, second, spec->layout.length[GROW_PLACE_FRAGMENT_2]) == 0)) {
        return false;
    }
    /* The cadential idea liquidates the segments of m0 until a residue fits, keeps a segment and meets the cadence; a draw
       with no such residue fails (R-345). */
    return finish_continuation(request, spec, 1);
}

/* A consequent restates the basic idea and ends with a contrasting idea that is not the antecedent's. */
static bool draw_consequent(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    const MotiveRelationStep *avoid = spec->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? &spec->contrast : NULL;
    /* It ends in the cadence's tone: the tonic for a perfect authentic cadence, the third for an imperfect one. */
    AnalysisKey ends_in = request->has_end_change ? request->end_key : request->key;
    int tonic = analysis_key_tonic_pitch_class(&ends_in);
    int want = request->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? tonic
               : request->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ? (tonic + (ends_in.mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 4)) % 12
                                                      : -1;
    /* A contrasting idea with a motive of its own is that motive as it is: nothing to draw. */
    bool own_contrast = grow_idea_own(spec, GROW_PLACE_CONSEQUENT_CONTRAST);
    if (own_contrast) {
        spec->consequent_contrast.kind = MOTIVE_RELATION_EXACT;
    }
    if (!keep_idea(spec, GROW_PLACE_RESTATED) ||
        (!own_contrast &&
         (!grow_shorten_source(spec, GROW_PLACE_CONSEQUENT_CONTRAST) ||
          !grow_draw_contrast(request, rng, spec, GROW_PLACE_CONSEQUENT_CONTRAST, grow_idea_source(spec, GROW_PLACE_CONSEQUENT_CONTRAST), want, avoid,
                             &spec->consequent_contrast))) ||
        !keep_idea(spec, GROW_PLACE_CONSEQUENT_CONTRAST)) {
        return false;
    }
    size_t cells = spec->layout.ending_bars * request->cells_per_bar;
    spec->ending_shape = cells >= harmony_phrase_room_cells(request->cadence, request->cells_per_bar)
                             ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                             : PLAN_HARMONY_SHAPE_CADENTIAL;
    spec->ending_split = grow_split_possible(request, spec->layout.ending_bars, spec->ending_shape, request->cadence);
    return !request->has_end_change || spec->ending_shape == PLAN_HARMONY_SHAPE_PHRASE_MODEL;
}

/* A cadential ending is two ideas liquidated from m0, the second at least as much as the first. */
static bool draw_cadential(const GrowThemeRequest *request, GrowThemeSpec *spec) {
    /* Each idea without a motive of its own liquidates the segments of m0, the second at least as far as the first; a draw
       where either has no residue that keeps a segment fails (R-345). */
    static const GrowPlace IDEAS[2] = {GROW_PLACE_CADENTIAL_1, GROW_PLACE_CADENTIAL_2};
    size_t from = 1;
    for (size_t k = 0; k < 2; k++) {
        size_t steps;
        size_t segment;
        bool head;
        Motive made;
        if (grow_idea_own(spec, IDEAS[k])) {
            continue;
        }
        if (!liquidate_segments(spec, request->development, from, idea_room(spec, IDEAS[k]), k == 1, request->cadence,
                                &steps, &segment, &head, &made)) {
            return false;
        }
        keep_liquidation(spec, IDEAS[k], steps, segment, head, &made);
        from = steps;
    }
    spec->ending_shape = PLAN_HARMONY_SHAPE_CADENTIAL;
    spec->ending_split = grow_split_possible(request, spec->layout.ending_bars, spec->ending_shape, request->cadence);
    /* A cadential progression has no pivot to open a key with. */
    return !request->has_end_change && keep_idea(spec, GROW_PLACE_CADENTIAL_1) &&
           keep_idea(spec, GROW_PLACE_CADENTIAL_2);
}

bool grow_ending_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    switch (spec->pairing.ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return draw_continuation(request, rng, spec);
    case PLAN_THEME_ENDING_CONSEQUENT:
        return draw_consequent(request, rng, spec);
    case PLAN_THEME_ENDING_CADENTIAL:
        return draw_cadential(request, spec);
    }
    return false;
}

/*
 * The second half is its own phrase after an antecedent, and a part of the theme's one phrase after any other
 * beginning. The container the ending's parts go in is that phrase, or a subphrase of the phrase when `subphrase` names
 * one. It is the node the chords are written over.
 */
static PlanResult make_container(GrowBuild *build, const char *phrase_name, const char *subphrase, NodeId *out) {
    PlanResult result = PLAN_OK;
    NodeId parent = build->phrase;
    if (parent == PLAN_NODE_NONE) {
        result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, build->layout.beginning, build->layout.ending,
                               phrase_name, &parent);
        if (result == PLAN_OK) {
            result = plan_node_add_constraint(build->plan, parent, &build->cadence);
        }
        if (result != PLAN_OK || !subphrase) {
            *out = parent;
            return result;
        }
    }
    return result == PLAN_OK
               ? grow_add_node(build, parent, PLAN_NODE_SUBPHRASE, build->layout.beginning, build->layout.ending, subphrase, out)
               : result;
}

/*
 * A chord a cell through a sentence's continuation (R-465): the setting is on, the ending is a continuation with no
 * key change, its fragments are a bar or more (an 8-bar theme's; a construction's of whole bars), and a bar has two cells
 * or more. In 2/4 and 3/4 a bar is one cell and nothing changes.
 */
static bool faster_harmony(const GrowBuild *build, size_t first_slot, size_t last_slot) {
    return build->spec->continuation.faster_harmony && first_slot == GROW_PLACE_FRAGMENT_1 &&
           last_slot == GROW_PLACE_CADENTIAL && !build->request->has_end_change &&
           rational_cmp(build->layout.length[GROW_PLACE_FRAGMENT_1], build->request->bar) >= 0 &&
           build->request->cells_per_bar >= 2;
}

/* An idea of the second half in time order: one of the pairing's, or a repeat of one (R-468). */
typedef struct {
    NodeId node;
    size_t slot;      /* the place of the idea, or of the idea a repeat follows */
    size_t extension; /* the layout extension the idea is, SIZE_MAX for an idea of the pairing */
} Part;

/*
 * One target an idea in place of the continuation's single one, each with a chord of at most a cell that crosses no
 * barline: the fragments (and their repeats) prolong the tonic to the dominant (I then V, V6, V65 or V43, R-344), the
 * cadential idea is the cadential progression and takes the cadence.
 */
static PlanResult add_faster_targets(GrowBuild *build, const Part *parts, size_t count, size_t last_slot) {
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        bool last = parts[i].extension == SIZE_MAX && parts[i].slot == last_slot;
        PlanHarmonyShape shape;
        memset(&shape, 0, sizeof(shape));
        shape.max_cells_per_chord = 1;
        shape.split_at_bars = true;
        if (last) {
            shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
        } else {
            shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
            shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
            shape.ends_on = PLAN_HARMONY_ANCHOR_DOMINANT;
        }
        result = grow_add_chord_targets(build, parts[i].node, &shape, last ? &build->cadence : NULL);
    }
    return result;
}

/* The chords over the container, and a melody span for each idea: the motive nodes m<slot> are made first. */
static PlanResult build_parts(GrowBuild *build, NodeId container, const PlanHarmonyShape *shape,
                              const char *const *names, size_t first_slot, size_t last_slot) {
    const GrowThemeSpec *spec = build->spec;
    NodeId motives[GROW_PLACE_COUNT];
    PlanMotiveRef refs[GROW_PLACE_COUNT];
    Part parts[GROW_PLACE_COUNT + PLAN_CONSTRUCTION_EXTENSIONS];
    size_t count = 0;
    PlanResult result = PLAN_OK;

    for (size_t slot = first_slot; result == PLAN_OK && slot <= last_slot; slot++) {
        NodeId idea;
        result = grow_add_node(build, container, PLAN_NODE_IDEA, build->layout.start[slot], build->layout.length[slot],
                               names[slot], &idea);
        parts[count].node = idea;
        parts[count].slot = slot;
        parts[count++].extension = SIZE_MAX;
        /* A repeat follows the idea it repeats (R-468). */
        for (size_t k = 0; result == PLAN_OK && k < build->layout.extension_count; k++) {
            if (build->layout.extensions[k].kind == PLAN_EXTENSION_REPEAT && build->layout.extensions[k].place == slot) {
                result = grow_add_repeat_node(build, container, k, &parts[count].node);
                parts[count].slot = slot;
                parts[count++].extension = k;
            }
        }
    }
    /* A motive in the material for each idea, each made from m0. */
    for (size_t slot = first_slot; result == PLAN_OK && slot <= last_slot; slot++) {
        grow_idea_ref(spec, (GrowPlace)slot, grow_idea_node(build, (GrowPlace)slot), &refs[slot]);
        result = grow_add_motive(build, slot, &refs[slot], &motives[slot]);
    }
    if (result == PLAN_OK && faster_harmony(build, first_slot, last_slot)) {
        result = add_faster_targets(build, parts, count, last_slot);
    } else if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, container, shape, &build->cadence);
    }
    /* The melody to write, one span for each idea, the last asking for the cadence. */
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        PlanMotiveRef ref;
        if (parts[i].extension != SIZE_MAX) {
            grow_extension_ref(build, parts[i].extension, &ref);
        } else {
            ref = refs[parts[i].slot];
            ref.motive = motives[parts[i].slot];
            /* The segment and the steps are recorded on the motive node; the melody asks for the node. */
            ref.segment[0] = '\0';
            ref.steps = 0;
        }
        result = grow_add_melody_target(build, parts[i].node, &ref, i + 1 == count ? &build->cadence : NULL);
    }
    return result;
}

/* One Realized motive node in the material (the run's own, R-346), `at` bars after the first of the bars grow-theme made motives go in. */
static PlanResult add_made_node(GrowBuild *build, size_t at, const Motive *motive, const char *name, NodeId *out) {
    Rational start;
    PlanResult result = plan_result_of_rational(
        rational_add(build->material_span.start, grow_scaled(build->request->bar, build->result_base + at), &start));
    return result == PLAN_OK ? motive_attach_as(build->plan, build->material, start, motive, name, PLAN_STATE_REALIZED, out, NULL) : result;
}

/* The motive nodes of what grow-theme made: the segments the fragments are made from, the liquidated ideas. */
static PlanResult add_made_nodes(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    size_t at = 0;
    PlanResult result = PLAN_OK;
    for (size_t k = 0; k < 2; k++) {
        if (!spec->fragment_from_segment[k]) {
            continue;
        }
        if (k == 1 && spec->fragment_from_segment[0] && spec->fragment_segment[1] == spec->fragment_segment[0]) {
            build->segment_node[1] = build->segment_node[0];
            continue;
        }
        char name[32];
        snprintf(name, sizeof(name), "segment %s", spec->segments[spec->fragment_segment[k]].label);
        result = add_made_node(build, at++, &spec->fragment_motive[k], name, &build->segment_node[k]);
        if (result != PLAN_OK) {
            return result;
        }
    }
    for (GrowPlace idea = GROW_PLACE_FRAGMENT_1; idea < GROW_PLACE_COUNT; idea++) {
        if (spec->has_liquidated[idea]) {
            char name[32];
            snprintf(name, sizeof(name), "liquidated %zu", (size_t)idea - GROW_PLACE_FRAGMENT_1 + 1);
            result = add_made_node(build, at++, &spec->liquidated[idea], name, &build->liquid_node[idea]);
            if (result != PLAN_OK) {
                return result;
            }
        }
    }
    return PLAN_OK;
}

static PlanResult build_continuation(GrowBuild *build) {
    static const char *const NAMES[GROW_PLACE_COUNT] = {NULL, NULL, "fragment 1", "fragment 2", "cadential"};
    NodeId continuation;
    PlanHarmonyShape shape;
    PlanResult result = add_made_nodes(build);
    result = result == PLAN_OK ? make_container(build, "second phrase", "continuation", &continuation) : result;
    memset(&shape, 0, sizeof(shape));
    shape.kind = build->spec->ending_shape;
    shape.starts_on = build->request->has_end_change ? PLAN_HARMONY_ANCHOR_PIVOT : PLAN_HARMONY_ANCHOR_ANY;
    shape.max_cells_per_chord = build->spec->ending_cap;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, continuation, &shape, NAMES, GROW_PLACE_FRAGMENT_1, GROW_PLACE_CADENTIAL)
                             : result;
}

static PlanResult build_consequent(GrowBuild *build) {
    static const char *const NAMES[GROW_PLACE_COUNT] = {NULL, NULL, "consequent basic idea",
                                                       "consequent contrasting idea", NULL};
    NodeId consequent;
    PlanHarmonyShape shape;
    PlanResult result = make_container(build, "consequent", build->phrase == PLAN_NODE_NONE ? NULL : "consequent",
                                       &consequent);
    memset(&shape, 0, sizeof(shape));
    shape.kind = build->spec->ending_shape;
    shape.starts_on = build->request->has_end_change ? PLAN_HARMONY_ANCHOR_PIVOT : PLAN_HARMONY_ANCHOR_ANY;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, consequent, &shape, NAMES, GROW_PLACE_RESTATED, GROW_PLACE_CONSEQUENT_CONTRAST)
                             : result;
}

static PlanResult build_cadential(GrowBuild *build) {
    static const char *const NAMES[GROW_PLACE_COUNT] = {NULL, NULL, "cadential 1", "cadential 2", NULL};
    NodeId cadential;
    PlanHarmonyShape shape;
    PlanResult result = add_made_nodes(build);
    result = result == PLAN_OK ? make_container(build, "second phrase", "cadential", &cadential) : result;
    memset(&shape, 0, sizeof(shape));
    shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, cadential, &shape, NAMES, GROW_PLACE_CADENTIAL_1, GROW_PLACE_CADENTIAL_2)
                             : result;
}

/*
 * The addition after the cadence (R-468): a phrase of its own after the theme's, a tonic prolonged from tonic to tonic
 * with no cadence, its melody the motive made for it.
 */
static PlanResult build_addition(GrowBuild *build, size_t k) {
    const GrowLayoutExtension *extension = &build->layout.extensions[k];
    PlanHarmonyShape shape;
    PlanMotiveRef ref;
    NodeId phrase;
    NodeId idea;
    PlanResult result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, extension->start, extension->length,
                                      "addition", &phrase);
    if (result == PLAN_OK) {
        result = grow_add_node(build, phrase, PLAN_NODE_IDEA, extension->start, extension->length, "addition idea", &idea);
    }
    memset(&shape, 0, sizeof(shape));
    shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, phrase, &shape, NULL);
    }
    grow_extension_ref(build, k, &ref);
    return result == PLAN_OK ? grow_add_melody_target(build, idea, &ref, &build->cadence) : result;
}

PlanResult grow_ending_build(GrowBuild *build) {
    PlanResult result = PLAN_ERR_BAD_PAYLOAD;
    switch (build->spec->pairing.ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        result = build_continuation(build);
        break;
    case PLAN_THEME_ENDING_CONSEQUENT:
        result = build_consequent(build);
        break;
    case PLAN_THEME_ENDING_CADENTIAL:
        result = build_cadential(build);
        break;
    }
    for (size_t k = 0; result == PLAN_OK && k < build->layout.extension_count; k++) {
        if (build->layout.extensions[k].kind == PLAN_EXTENSION_ADDITION) {
            result = build_addition(build, k);
        }
    }
    return result;
}
