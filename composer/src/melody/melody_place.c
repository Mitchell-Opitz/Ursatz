#include "melody/melody_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <model/core/rational.h>
#include "util/util.h"

#include <theory/counterpoint/melodic_rules.h>
#include <theory/tonality/tonal_key.h>

#define MAX_OPTIONS 128
/* The widest a note after the motive moves from the one before it, in semitones. */
#define MAX_MOVE 12

typedef struct {
    int pitch;
    int distance;
} Option;

/* The moves standard-counterpoint allows: no tritone, no seventh, the sixth only as the ascending minor one. */
static bool leap_allowed(int distance) {
    return standard_counterpoint_melodic_semitones_allowed(distance);
}

/* The pitches in range whose pitch class is in mask, nearest to ideal first. */
static size_t options_for(unsigned mask, int low, int high, int ideal, Option *out) {
    size_t count = 0;
    for (int pitch = low; pitch <= high && count < MAX_OPTIONS; pitch++) {
        if ((mask >> util_pitch_class(pitch)) & 1u) {
            Option option = {pitch, util_absolute(pitch - ideal)};
            size_t at = count++;
            while (at > 0 && (out[at - 1].distance > option.distance)) {
                out[at] = out[at - 1];
                at--;
            }
            out[at] = option;
        }
    }
    return count;
}

/* Of the first options within max_distance, the nearest most often, else the next (params->second_nearest of the draws). */
static bool pick(const Option *options, size_t count, int max_distance, const MelodyFillParams *params, FillRng *rng,
                 int *pitch) {
    size_t usable = 0;
    while (usable < count && options[usable].distance <= max_distance) {
        usable++;
    }
    unsigned long denominator = params->second_nearest.den;
    uint64_t draw = fill_rng_below(rng, denominator);
    if (usable == 0) {
        return false;
    }
    *pitch = options[usable > 1 && draw >= denominator - params->second_nearest.num ? 1 : 0].pitch;
    return true;
}

static MelodyNote *push(MelodyCandidate *out, Rational onset, Rational duration) {
    if (out->count >= MELODY_MAX_NOTES) {
        return NULL;
    }
    MelodyNote *note = &out->notes[out->count++];
    note->onset = onset;
    note->duration = duration;
    note->motive_index = -1;
    return note;
}

/*
 * The room after a liquidated residue (R-345): the residue's rhythm again from its first note, as many whole times as fit
 * before the last chord when the phrase ends in a cadence (else before the span's end), the last note held up to there, and
 * a note from the last chord to the span's end, so the cadence arrives with its chord. No even-note filler.
 */
static bool layout_residue(const MelodyContext *context, Rational start, MelodyCandidate *out) {
    Rational first = context->motive_onset[0];
    Rational cycle = position_sub(context->motive_length, first);
    Rational at = position_add(start, context->motive_length);
    Rational last = context->chord_start[context->count - 1];
    Rational bound = context->has_cadence && rational_cmp(last, at) > 0 ? last : context->length;
    while (position_is_positive(cycle) && rational_cmp(position_add(at, cycle), bound) <= 0) {
        for (size_t k = 0; k < context->motive_count; k++) {
            Rational offset = position_sub(context->motive_onset[k], first);
            if (!push(out, position_add(at, offset), context->motive_duration[k])) {
                return false;
            }
        }
        at = position_add(at, cycle);
    }
    MelodyNote *held = &out->notes[out->count - 1];
    held->duration = position_sub(bound, held->onset);
    if (rational_cmp(bound, context->length) < 0 && !push(out, bound, position_sub(context->length, bound))) {
        return false;
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
    return true;
}

/*
 * The notes of the phrase: the motive's, then notes that fill the rest. A motive as long as the phrase (an idea it fills
 * whole) is all there is: its notes are placed as written and nothing is filled after it. A liquidated residue is
 * followed by its own rhythm (layout_residue). With a density the rest is notes of an even length, that many to a cell;
 * without one it is the motive's rhythm over again, each note as long as the time from one onset of the motive to the
 * next, the last as long as it is.
 */
static bool layout(const MelodyContext *context, const MelodyFillParams *params, MelodyCandidate *out) {
    Rational start = position_mul(context->cell, (long long)params->start_cell);
    if (context->motive_count == 0 || rational_cmp(position_add(start, context->motive_length), context->length) > 0) {
        return false;
    }
    for (size_t k = 0; k < context->motive_count; k++) {
        MelodyNote *note = push(out, position_add(start, context->motive_onset[k]), context->motive_duration[k]);
        if (!note) {
            return false;
        }
        note->motive_index = (int)k;
    }
    if (context->liquidated) {
        return layout_residue(context, start, out);
    }

    /* A note that ends the phrase starts with the last chord, so the cadence arrives with it. */
    Rational at = position_add(start, context->motive_length);
    Rational last = context->chord_start[context->count - 1];
    Rational bound = context->has_cadence && rational_cmp(last, at) > 0 ? last : context->length;
    /* A density divides the cell only into notes no shorter than the shortest the operator writes. */
    Rational shortest = rational_ratio(1, MELODY_FINEST_NOTE_DENOMINATOR);
    Rational share = params->density > 0 ? position_div(context->cell, (long long)params->density) : context->cell;
    Rational step = params->density > 0 && position_is_multiple(share, shortest) ? share : context->cell;
    size_t k = 0;
    while (rational_cmp(at, bound) < 0) {
        Rational next;
        if (params->density == 0) {
            size_t m = k++ % context->motive_count;
            next = position_add(at, m + 1 < context->motive_count
                                        ? position_sub(context->motive_onset[m + 1], context->motive_onset[m])
                                        : context->motive_duration[m]);
        } else {
            next = position_mul(step, position_floor_units(at, step) + 1);
        }
        if (rational_cmp(next, bound) > 0) {
            next = bound;
        }
        if (!push(out, at, position_sub(next, at))) {
            return false;
        }
        at = next;
    }
    if (rational_cmp(bound, context->length) < 0 && !push(out, bound, position_sub(context->length, bound))) {
        return false;
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
    return true;
}

bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i) {
    const MelodyNote *note = &melody->notes[i];
    /* A motive's note is realized before the others are placed (melody_realize): a fixed anchor. */
    if (note->motive_index >= 0) {
        return true;
    }
    return params->nct_types == 0 || i == 0 || i + 1 == melody->count || note->strong ||
           rational_cmp(note->duration, context->cell) >= 0 ||
           rational_cmp(position_add(note->onset, note->duration), context->chord_start[note->chord + 1]) > 0;
}

/*
 * Gives the notes after the motive their pitches; the motive's own notes are realized already (melody_realize) and are
 * the anchors they follow. An anchor after the motive follows the arch planned for the phrase (melody_contour_plan): the
 * climax anchor takes the climax pitch, no other is as high, and the others take the chord tone nearest the arch, by a
 * leap standard-counterpoint allows. A leap of a fourth or more is followed, where the chord allows, by a step the other
 * way. The notes between two anchors are then filled (melody_fill_between).
 */
static bool assign(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out) {
    int low = params->range_low;
    int high = params->range_high;
    MelodyContour contour;
    size_t before = 0; /* the anchor before this one */
    memset(&contour, 0, sizeof(contour));
    for (size_t i = 0; i < out->count; i++) {
        MelodyNote *note = &out->notes[i];
        Option options[MAX_OPTIONS];
        bool adjacent = i > 0 && before + 1 == i;
        bool climax = false;
        unsigned mask;
        int ideal;
        int top = high;
        if (!melody_is_anchor(context, params, out, i)) {
            continue;
        }
        if (note->motive_index >= 0) {
            before = i;
            continue;
        }
        mask = melody_need_mask(context, params, note);
        if (i + 1 == out->count) {
            mask &= melody_cadence_mask(context);
        }
        /* Only generated notes are placed here: a motive's notes are realized already and are the anchors they follow. */
        if (!contour.made && !melody_contour_plan(context, params, out, i, rng, &contour)) {
            return false;
        }
        climax = !contour.in_motive && i == contour.climax;
        top = climax ? high : (contour.cap < high ? contour.cap : high);
        ideal = melody_contour_ideal(context, &contour, out, i, before);
        /* A leap of a fourth or more is answered by a step back, unless this is the climax or the end. */
        if (adjacent && (before > 0 || context->lead_in_count > 0) && i + 1 < out->count) {
            int leap = out->notes[before].pitch -
                       (before > 0 ? out->notes[before - 1].pitch : context->lead_in[context->lead_in_count - 1]);
            if (leap >= MELODY_RECOVER_FROM || leap <= -MELODY_RECOVER_FROM) {
                ideal = out->notes[before].pitch + (leap > 0 ? -2 : 2);
            }
        }
        size_t count = options_for(mask, low, top, ideal, options);
        size_t kept = 0;
        size_t strong_before = SIZE_MAX; /* the last note before this one that starts on a strong beat, when it has a pitch */
        for (size_t q = i; q > 0 && strong_before == SIZE_MAX; q--) {
            if (out->notes[q - 1].strong && (out->notes[q - 1].motive_index >= 0 || q - 1 <= before)) {
                strong_before = q - 1;
            }
        }
        for (size_t j = 0; j < count; j++) {
            int move = options[j].pitch - out->notes[before].pitch;
            bool repeat = adjacent && move == 0;
            if ((adjacent && !leap_allowed(move)) || repeat) {
                continue;
            }
            /* Not in parallel octaves or fifths with the bass against the note before. */
            if (adjacent && melody_parallel_perfects(out->notes[before].pitch, options[j].pitch,
                                                     melody_bass_at(context, out->notes[before].onset),
                                                     melody_bass_at(context, note->onset))) {
                continue;
            }
            /* Nor against the last strong beat before it, which is where the Analyzer reads parallels between the notes. */
            if (strong_before != SIZE_MAX && strong_before != before &&
                melody_parallel_perfects(out->notes[strong_before].pitch, options[j].pitch,
                                         melody_bass_at(context, out->notes[strong_before].onset),
                                         melody_bass_at(context, note->onset))) {
                continue;
            }
            /* A chord tone that is dissonant against the bass is not written (the checker rejects it). */
            if (melody_dissonant_over_bass(context, options[j].pitch, note->onset, position_add(note->onset, note->duration))) {
                continue;
            }
            /* The last note of a target that comes before written melody leads to its first note by a leap allowed. */
            if (i + 1 == out->count && context->has_lead_out && !leap_allowed(context->lead_out - options[j].pitch)) {
                continue;
            }
            options[kept++] = options[j];
        }
        /* Relaxed (R-338): a climax that cannot be reached by an allowed leap, or an anchor with no option, takes the chord
           tones of the whole range that are an allowed leap from the note before and to the lead-out, not a repeat. */
        bool fall_back = out->relaxed && ((climax && adjacent && !leap_allowed(contour.climax_pitch - out->notes[before].pitch)) ||
                                          (!climax && kept == 0));
        if (fall_back) {
            count = options_for(mask, out->params.range_low, out->params.range_high, ideal, options);
            kept = 0;
            for (size_t j = 0; j < count; j++) {
                int move = options[j].pitch - out->notes[before].pitch;
                if (adjacent && (!leap_allowed(move) || move == 0)) {
                    continue;
                }
                if (i + 1 == out->count && context->has_lead_out && !leap_allowed(context->lead_out - options[j].pitch)) {
                    continue;
                }
                options[kept++] = options[j];
            }
            /* Of those, the ones that are not in parallel with the bass and not dissonant over it, when there are any. */
            size_t clean = 0;
            for (size_t j = 0; j < kept; j++) {
                bool parallel = adjacent && melody_parallel_perfects(out->notes[before].pitch, options[j].pitch,
                                                                     melody_bass_at(context, out->notes[before].onset),
                                                                     melody_bass_at(context, note->onset));
                if (!parallel &&
                    !melody_dissonant_over_bass(context, options[j].pitch, note->onset, position_add(note->onset, note->duration))) {
                    options[clean++] = options[j];
                }
            }
            kept = clean > 0 ? clean : kept;
            if (!pick(options, kept, 127, params, rng, &note->pitch)) {
                return false;
            }
        } else if (climax) {
            note->pitch = contour.climax_pitch;
            if (adjacent && !leap_allowed(note->pitch - out->notes[before].pitch)) {
                return false;
            }
        } else if (!pick(options, kept, MAX_MOVE, params, rng, &note->pitch)) {
            return false;
        }
        if (i > 0 && before + 1 < i) {
            int between[MELODY_MAX_NOTES];
            /* Notes between the motive's last anchor and the first note after it may rise to the motive's top. */
            int cap = high;
            if (contour.made) {
                cap = out->notes[before].motive_index >= 0 && contour.motive_top > contour.cap ? contour.motive_top
                                                                                                : contour.cap;
            }
            for (size_t j = before + 1; j < i; j++) {
                between[j] = out->notes[before].pitch +
                             (note->pitch - out->notes[before].pitch) * (int)(j - before) / (int)(i - before);
            }
            if (!melody_fill_between(context, params, out->notes, out->count, before, i, between, cap, rng, out->relaxed)) {
                return false;
            }
        }
        before = i;
    }
    return true;
}

/*
 * The range a target keeps to in its phrase's arch: a target before the phrase's climax stays three semitones under
 * the top of the range, so the climax has room above it; one after the climax stays under the climax note.
 */
static MelodyFillParams arch_params(const MelodyContext *context, const MelodyFillParams *params) {
    MelodyFillParams adjusted = *params;
    MelodyRole role = melody_role(context, params);
    if (role == MELODY_ROLE_BEFORE) {
        adjusted.range_high = params->range_high - 3;
    } else if (role == MELODY_ROLE_AFTER && context->phrase_has_top && context->phrase_top - 1 < adjusted.range_high) {
        adjusted.range_high = context->phrase_top - 1;
    }
    return adjusted;
}

/* The pitch a step (a semitone or a tone, within the scale) from `from` in a direction, in range; -1 when none. */
static int scale_step(unsigned scale, int from, int direction, int low, int high) {
    int pitch = standard_tonality_mask_step(scale, from, direction);
    return pitch != from && util_absolute(pitch - from) <= 2 && pitch >= low && pitch <= high ? pitch : -1;
}

/*
 * The anacrusis (PLAN_CONSTRAINT_PICKUP): 1 to MELODY_MAX_PICKUP notes ending where the first downbeat note starts. They
 * are equal beats (or the biggest equal part of one that divides the pickup), the first as long as what is left over
 * when there are too many. The last is a step from the first downbeat note, or a chord tone of the chord before when
 * one sounds there; those before it come by step toward it, mostly from below.
 */
static bool place_pickup(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng,
                         MelodyCandidate *out) {
    Rational beat_time;
    if (rational_is_zero(context->pickup)) {
        return true;
    }
    if (rational_make(1, (long long)context->metre.meter.beat_unit, &beat_time) != RATIONAL_OK) {
        return false;
    }
    Rational unit = position_common_unit(context->pickup, beat_time);
    long long count = position_floor_units(context->pickup, unit);
    count = count > MELODY_MAX_PICKUP ? MELODY_MAX_PICKUP : count;
    if (count < 1) {
        return false;
    }
    Rational first_length = position_sub(context->pickup, position_mul(unit, count - 1));
    Rational onset = position_sub(POSITION_ZERO, context->pickup);
    for (long long k = 0; k < count; k++) {
        Rational length = k == 0 ? first_length : unit;
        out->pickup_notes[k].onset = onset;
        out->pickup_notes[k].duration = length;
        out->pickup_notes[k].motive_index = -1;
        out->pickup_notes[k].chord = 0;
        out->pickup_notes[k].strong = false;
        onset = position_add(onset, length);
    }
    out->pickup_count = (size_t)count;

    /* The last note: a step from the downbeat note, or a chord tone of the chord before within a fourth of it. */
    int target = out->notes[0].pitch;
    unsigned scale = melody_scale_mask(context, 0);
    int options[16];
    size_t steps = 0;
    size_t tones = 0;
    for (int pitch = target - 2; pitch <= target + 2; pitch++) {
        if (pitch != target && pitch >= params->range_low && pitch <= params->range_high &&
            ((scale >> util_pitch_class(pitch)) & 1u)) {
            options[steps++] = pitch;
        }
    }
    int chord_tones[16];
    for (int pitch = target - 5; context->pickup_mask != 0 && pitch <= target + 5; pitch++) {
        if (pitch != target && util_absolute(pitch - target) > 2 && pitch >= params->range_low &&
            pitch <= params->range_high && ((context->pickup_mask >> util_pitch_class(pitch)) & 1u)) {
            chord_tones[tones++] = pitch;
        }
    }
    if (steps == 0 && tones == 0) {
        return false;
    }
    /* A step in two draws of three; a chord tone otherwise, when there is one. */
    bool step = tones == 0 || (steps > 0 && fill_rng_below(rng, 3) > 0);
    int last = step ? options[fill_rng_below(rng, steps)] : chord_tones[fill_rng_below(rng, tones)];
    out->pickup_notes[count - 1].pitch = last;
    int direction = last < target ? 1 : -1;
    for (long long k = count - 2; k >= 0; k--) {
        /* Going back from the last note, away from the downbeat note: the line leans into it. */
        int pitch = scale_step(scale, out->pickup_notes[k + 1].pitch, -direction, params->range_low, params->range_high);
        if (pitch < 0) {
            pitch = scale_step(scale, out->pickup_notes[k + 1].pitch, direction, params->range_low, params->range_high);
        }
        if (pitch < 0) {
            return false;
        }
        out->pickup_notes[k].pitch = pitch;
    }
    return true;
}

/* How many times the closure rules count for this target (R-478): its weight when it ends a cadence, else 0. */
unsigned melody_closure_weight(const MelodyContext *context, const MelodyFillParams *params) {
    if (!context->has_cadence) {
        return 0;
    }
    return context->last_cadence ? params->closure_final : params->closure_first;
}

/*
 * Closure (phase L4, R-478): the notes from a cell line c on become one long last note, so the phrase ends on a note at
 * least twice as long as its mean note value, and the bar before it settles: a note shorter than the one before it takes
 * its time. c is the latest cell line at or before the last note's onset that leaves 2M, M being the phrase's mean note
 * value; not before the target's last bar, and not inside a note of the motive, so a motive stays as written. No change
 * when no such line exists.
 */
static void settle_ending(const MelodyContext *context, MelodyCandidate *out) {
    if (out->count < 2) {
        return;
    }
    size_t last = out->count - 1;
    size_t before = context->phrase_before_notes + out->count - 1;
    Rational mean = position_div(position_sub(out->notes[last].onset, context->phrase_start), (long long)before);
    Rational room = position_sub(context->length, position_mul(mean, 2));
    Rational latest = rational_cmp(out->notes[last].onset, room) < 0 ? out->notes[last].onset : room;
    Rational low = position_sub(context->length, context->metre.bar_length);
    if (rational_cmp(low, POSITION_ZERO) < 0) {
        low = POSITION_ZERO;
    }
    for (size_t i = 0; i < out->count; i++) {
        if (out->notes[i].motive_index >= 0) {
            Rational end = position_add(out->notes[i].onset, out->notes[i].duration);
            if (rational_cmp(end, low) > 0) {
                low = end;
            }
        }
    }
    Rational at = position_sub(
        position_mul(context->cell, position_floor_units(position_add(latest, context->phase), context->cell)),
        context->phase);
    if (rational_cmp(at, low) < 0) {
        return;
    }
    size_t first = 0;
    while (first < out->count && rational_cmp(out->notes[first].onset, at) < 0) {
        first++;
    }
    if (first == 0) {
        return;
    }
    out->notes[first - 1].duration = position_sub(at, out->notes[first - 1].onset);
    out->notes[first].onset = at;
    out->notes[first].duration = position_sub(context->length, at);
    out->notes[first].motive_index = -1;
    out->count = first + 1;
    /* The bar of the last note: a shorter note takes the time of the note before it, the rhythm settling into the end. */
    Rational bar = position_sub(context->length, context->metre.bar_length);
    for (size_t j = 1; j + 1 < out->count;) {
        MelodyNote *note = &out->notes[j];
        MelodyNote *prior = &out->notes[j - 1];
        if (rational_cmp(prior->onset, bar) >= 0 && note->motive_index < 0 &&
            rational_cmp(note->duration, prior->duration) < 0) {
            prior->duration = position_add(prior->duration, note->duration);
            memmove(note, note + 1, (out->count - j - 1) * sizeof(*note));
            out->count--;
            if (j > 1) {
                j--;
            }
        } else {
            j++;
        }
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
}

/*
 * A held cadence note (P67, R-712, cadence_long): the notes from the target's last downbeat on become one note held to the target's
 * end, a restated motive's too; a note sounding over the downbeat ends there. The last downbeat is the latest start of a bar, counted
 * from the cell line before the target, that is before its end. The note is a note of the phrase (it carries no motive note), so the
 * assignment gives it the cadence's tone.
 */
Rational melody_last_downbeat(const MelodyContext *context) {
    Rational bar = context->metre.bar_length;
    if (!position_is_positive(bar) || !position_is_positive(context->cell)) {
        return POSITION_ZERO;
    }
    Rational span = position_sub(position_add(context->length, context->phase), context->cell);
    Rational at = position_sub(position_mul(bar, position_floor_units(span, bar)), context->phase);
    return rational_cmp(at, POSITION_ZERO) < 0 ? POSITION_ZERO : at;
}

static void hold_cadence(const MelodyContext *context, MelodyCandidate *out) {
    if (out->count == 0 || !position_is_positive(context->metre.bar_length)) {
        return;
    }
    Rational at = melody_last_downbeat(context);
    size_t first = 0;
    while (first < out->count && rational_cmp(out->notes[first].onset, at) < 0) {
        first++;
    }
    if (first == out->count && first >= MELODY_MAX_NOTES) {
        return;
    }
    if (first > 0 && rational_cmp(position_add(out->notes[first - 1].onset, out->notes[first - 1].duration), at) > 0) {
        out->notes[first - 1].duration = position_sub(at, out->notes[first - 1].onset);
    }
    MelodyNote held = out->notes[first < out->count ? first : out->count - 1];
    held.onset = at;
    held.duration = position_sub(context->length, at);
    held.motive_index = -1;
    out->notes[first] = held;
    out->count = first + 1;
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
}

/*
 * Rhythm repetition (phase L4, R-479): a bar of 2 or more notes with the rhythm of the bar before adds 1 to the run of
 * bars with one rhythm, carried in from the bars before the target; a bar that would pass the cap merges its last two
 * notes (the earlier takes the later's time) and the run starts again at 1. A merge never takes a note of the motive,
 * so a given motive stays as written. No random number. A target that starts inside a bar is left as it is.
 */
static bool bar_notes(const MelodyCandidate *out, Rational start, Rational bar_length, size_t *first, size_t *count) {
    Rational end = position_add(start, bar_length);
    *first = 0;
    while (*first < out->count && rational_cmp(out->notes[*first].onset, start) < 0) {
        (*first)++;
    }
    *count = 0;
    while (*first + *count < out->count && rational_cmp(out->notes[*first + *count].onset, end) < 0) {
        (*count)++;
    }
    return *count <= MELODY_BAR_NOTES;
}

/* Whether the notes from `first`, `count` of them, have the rhythm (onsets from the bar's start, durations) given. */
static bool same_rhythm(const MelodyCandidate *out, Rational start, size_t first, size_t count, const Rational *onset,
                        const Rational *duration, size_t given) {
    bool same = given == count;
    for (size_t i = 0; same && i < count; i++) {
        same = rational_cmp(position_sub(out->notes[first + i].onset, start), onset[i]) == 0 &&
               rational_cmp(out->notes[first + i].duration, duration[i]) == 0;
    }
    return same;
}

/*
 * Varies the bar: its last two notes merge when neither carries the motive (the earlier takes the later's time), else the
 * last two neighbouring notes of different lengths trade lengths, so the motive keeps its notes; the target's last note
 * is never one of them, so a cadence still arrives with it. False when neither can be done.
 */
static bool vary_bar(MelodyCandidate *out, size_t first, size_t count) {
    MelodyNote *last = &out->notes[first + count - 1];
    MelodyNote *earlier = last - 1;
    if (last->motive_index < 0 && earlier->motive_index < 0) {
        earlier->duration = position_add(earlier->duration, last->duration);
        memmove(last, last + 1, (out->count - (first + count)) * sizeof(*last));
        out->count--;
        return true;
    }
    size_t top = first + count == out->count ? count - 1 : count;
    for (size_t i = top; i-- > 1;) {
        MelodyNote *a = &out->notes[first + i - 1];
        MelodyNote *b = &out->notes[first + i];
        if (rational_cmp(a->duration, b->duration) != 0) {
            Rational held = a->duration;
            a->duration = b->duration;
            b->duration = held;
            b->onset = position_add(a->onset, a->duration);
            return true;
        }
    }
    return false;
}

static void limit_rhythm_repeats(const MelodyContext *context, const MelodyFillParams *params, MelodyCandidate *out) {
    Rational bar = context->metre.bar_length;
    bool varied[MELODY_MAX_NOTES] = {false};
    if (params->rhythm_repeat_bars == 0 || !context->repeat_aligned || !position_is_positive(bar)) {
        return;
    }
    /* Walk the bars with the run carried in; at the first bar that passes the cap, vary it or the latest bar before it in
       the run and in the target that can be varied (each bar once), and walk again. */
    for (bool again = true; again;) {
        Rational onset[MELODY_BAR_NOTES];
        Rational duration[MELODY_BAR_NOTES];
        size_t previous = context->repeat_run > 0 ? context->repeat_count : 0;
        size_t run = context->repeat_run;
        size_t index = 0;
        again = false;
        for (size_t i = 0; i < previous; i++) {
            onset[i] = context->repeat_onset[i];
            duration[i] = context->repeat_duration[i];
        }
        for (Rational start = POSITION_ZERO; !again && rational_cmp(start, context->length) < 0;
             start = position_add(start, bar), index++) {
            size_t first;
            size_t count;
            if (!bar_notes(out, start, bar, &first, &count) || count < 2) {
                previous = 0;
                run = 0;
                continue;
            }
            run = same_rhythm(out, start, first, count, onset, duration, previous) ? run + 1 : 1;
            if (run > params->rhythm_repeat_bars) {
                size_t lowest = index + 1 >= run ? index + 1 - run : 0;
                for (size_t back = index + 1; !again && back-- > lowest && index < MELODY_MAX_NOTES;) {
                    size_t f;
                    size_t c;
                    Rational at = position_mul(bar, (long long)back);
                    if (!varied[back] && bar_notes(out, at, bar, &f, &c) && c >= 2 && vary_bar(out, f, c)) {
                        varied[back] = true;
                        again = true;
                    }
                }
            }
            previous = count;
            for (size_t i = 0; i < count; i++) {
                onset[i] = position_sub(out->notes[first + i].onset, start);
                duration[i] = out->notes[first + i].duration;
            }
        }
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
}

/*
 * A held cadence over a motive (R-712): the motive's notes from the target's last downbeat on are not kept, so the last bar holds one
 * note of the phrase; a motive note sounding over the downbeat ends there. A motive with no note before the downbeat is left alone.
 */
static void trim_motive_for_cadence(MelodyContext *context) {
    Rational at = melody_last_downbeat(context);
    size_t kept = 0;
    while (kept < context->motive_count && rational_cmp(context->motive_onset[kept], at) < 0) {
        kept++;
    }
    if (kept == 0) {
        return;
    }
    context->motive_count = kept;
    Rational end = position_add(context->motive_onset[kept - 1], context->motive_duration[kept - 1]);
    if (rational_cmp(end, at) > 0) {
        context->motive_duration[kept - 1] = position_sub(at, context->motive_onset[kept - 1]);
        end = at;
    }
    context->motive_length = end;
}

bool melody_build(const MelodyContext *original, const MelodyFillParams *params, int shift, bool keep_figures,
                  bool relaxed, FillRng *rng, MelodyCandidate *out) {
    MelodyContext held_context;
    const MelodyContext *context = original;
    if (params->cadence_long && original->has_cadence) {
        held_context = *original;
        trim_motive_for_cadence(&held_context);
        context = &held_context;
    }
    MelodyFillParams arch = arch_params(context, params);
    memset(out, 0, sizeof(*out));
    out->magic = MELODY_PAYLOAD_MAGIC;
    out->relaxed = relaxed;
    out->context = *context;
    /* The checker holds every note to the voice's range: the arch caps are for the notes that are generated (R-327). */
    out->params = *params;
    /* A melody with two notes as high as its highest has no one climax. */
    bool laid = layout(context, &arch, out);
    /* Closure: half the layouts of a target that ends a cadence settle into a long last note; no draw at weight 0. */
    if (laid && melody_closure_weight(context, params) > 0 && fill_rng_below(rng, 2) == 0) {
        settle_ending(context, out);
    }
    /* The cadence held to one last note from the last downbeat, when it is on (R-712); it draws no random number. */
    if (laid && params->cadence_long && context->has_cadence) {
        hold_cadence(context, out);
    }
    /* The rhythm repetition cap, when it is on (R-479); it draws no random number. */
    if (laid && params->rhythm_repeat_bars > 0) {
        limit_rhythm_repeats(context, params, out);
    }
    bool assigned = laid && melody_realize(context, params, shift, keep_figures, out) &&
                    assign(context, &arch, rng, out) && place_pickup(context, &arch, rng, out);
    /* The phrase has one highest note: the whole phrase, or the target with its climax, has one, when it generates notes
       (R-335). A target of realized notes alone is scored by melody_contour_score instead. */
    MelodyRole role = melody_role(context, &arch);
    bool unique = assigned && (relaxed || (role != MELODY_ROLE_WHOLE && role != MELODY_ROLE_CLIMAX) ||
                               !melody_has_generated(out) || melody_contour_unique(out));
    return unique;
}
