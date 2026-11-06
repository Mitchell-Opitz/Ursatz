#include "melody/melody_internal.h"

#include <stdlib.h>
#include <string.h>
#include <core/rational.h>
#include "util/util.h"

#include <standard-counterpoint/melodic_rules.h>

#define MAX_OPTIONS 128
/* The motive's first note and the notes after the motive pick the nearest pitch this often in three; else the next. */
#define NEAREST_IN 2
#define NEAREST_OF 3
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

/* Of the first options within max_distance, the nearest most often, else the next; always the nearest when asked. */
static bool pick(const Option *options, size_t count, int max_distance, FillRng *rng, bool nearest, int *pitch) {
    size_t usable = 0;
    while (usable < count && options[usable].distance <= max_distance) {
        usable++;
    }
    uint64_t draw = fill_rng_below(rng, NEAREST_OF);
    if (usable == 0) {
        return false;
    }
    *pitch = options[usable > 1 && draw >= NEAREST_IN && !nearest ? 1 : 0].pitch;
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
 * The notes of the phrase: the motive's, then notes that fill the rest. A motive as long as the phrase (an idea it fills
 * whole) is all there is: its notes are placed as written and nothing is filled after it. With a density the rest is notes of an
 * even length, that many to a cell; without one it is the motive's rhythm over again, each note as long as the
 * time from one onset of the motive to the next, the last as long as it is.
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

/* The bit of params.nct_types for a non-chord tone the operator writes; 0 for one it does not (retardation, escape...). */
static unsigned figure_bit(int tone) {
    switch (tone) {
    case NON_CHORD_TONE_PASSING:
        return MELODY_NCT_PASSING;
    case NON_CHORD_TONE_NEIGHBOR:
        return MELODY_NCT_NEIGHBOR;
    case NON_CHORD_TONE_APPOGGIATURA:
        return MELODY_NCT_APPOGGIATURA;
    case NON_CHORD_TONE_SUSPENSION:
        return MELODY_NCT_SUSPENSION;
    case NON_CHORD_TONE_ANTICIPATION:
        return MELODY_NCT_ANTICIPATION;
    default:
        return 0;
    }
}

/*
 * Whether the note is a non-chord tone of the motive that resolves to the next note, a chord tone of the motive, in a
 * figure the operator writes, unless the parameters allow no non-chord tone at all (R-215). It is not an anchor, whether it
 * is on a strong beat or not: it is given a pitch from the chord tones around it. The first note, the last and every other non-chord tone are placed
 * as they were.
 */
static bool is_figure_of_motive(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *notes,
                                size_t count, size_t i) {
    const MelodyNote *note = &notes[i];
    size_t k = note->motive_index < 0 ? 0 : (size_t)note->motive_index;
    return note->motive_index > 0 && i > 0 && i + 1 < count && k + 1 < context->motive_tone_count &&
           context->motive_tone[k] >= 0 && params->nct_types != 0 && figure_bit(context->motive_tone[k]) != 0 &&
           context->motive_tone[k + 1] == MELODY_TONE_CHORD && notes[i + 1].motive_index == (int)k + 1;
}

/* Whether the note is the chord tone a figure of the motive resolves to: it is placed before the figure is formed. */
static bool resolves_figure(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *notes,
                            size_t count, size_t i) {
    return i > 0 && is_figure_of_motive(context, params, notes, count, i - 1);
}

bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i) {
    const MelodyNote *note = &melody->notes[i];
    /* A motive's note is realized before the others are placed (melody_realize): a fixed anchor. */
    if (note->motive_index >= 0) {
        return true;
    }
    if (is_figure_of_motive(context, params, melody->notes, melody->count, i)) {
        return false;
    }
    if (resolves_figure(context, params, melody->notes, melody->count, i)) {
        return true;
    }
    return params->nct_types == 0 || i == 0 || i + 1 == melody->count || note->strong ||
           rational_cmp(note->duration, context->cell) >= 0 ||
           rational_cmp(position_add(note->onset, note->duration), context->chord_start[note->chord + 1]) > 0;
}

/* Whether a pitch is a tone of every chord the note sounds through. */
static bool chord_tone_of_all(const MelodyContext *context, const MelodyNote *note, int pitch) {
    Rational end = position_add(note->onset, note->duration);
    for (size_t c = note->chord; c < context->count && rational_cmp(context->chord_start[c], end) < 0; c++) {
        if (!((context->chord_mask[c] >> util_pitch_class(pitch)) & 1u)) {
            return false;
        }
    }
    return true;
}

/*
 * The pitch of note j, a non-chord tone of the motive (is_figure_of_motive) between the chord tone before it and the one it
 * resolves to, when both are placed: the figure the motive's tone names, formed on the moved chord tones. A passing tone
 * lies between its two neighbours, a neighbor tone, an appoggiatura and a suspension a step from the chord tone they resolve
 * to, on the side they were on in the motive, an anticipation sounds the chord tone early. Each is tried a step first and
 * then a tone, within the scale of the chord, a non-chord tone before a pitch that is a tone of its chord, and kept when
 * melody_figure_ok holds it as that figure alone. False when none is formed.
 */
static bool figure_pitch(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes, size_t count,
                         size_t j, int *pitch) {
    MelodyNote *note = &notes[j];
    size_t k = (size_t)note->motive_index;
    int type = context->motive_tone[k];
    int before = notes[j - 1].pitch;
    int after = notes[j + 1].pitch;
    int side = util_sign(context->motive_pitch[k] - context->motive_pitch[k + 1]);
    unsigned scale = melody_scale_mask(context, note->chord);
    MelodyFillParams only = *params;
    int candidates[4];
    size_t found = 0;
    only.nct_types = figure_bit(type);
    if (type == NON_CHORD_TONE_ANTICIPATION) {
        candidates[found++] = after;
    } else if (type == NON_CHORD_TONE_PASSING) {
        int way = util_sign(after - before);
        for (int step = 1; way != 0 && step <= 2; step++) {
            candidates[found++] = before + way * step;
        }
    } else if (side != 0) {
        for (int step = 1; step <= 2; step++) {
            candidates[found++] = after + side * step;
        }
    }
    /* A pitch that is not a tone of its chord first, then one that is. */
    for (int pass = 0; pass < 2; pass++) {
        for (size_t c = 0; c < found; c++) {
            int try_pitch = candidates[c];
            bool tone = chord_tone_of_all(context, note, try_pitch);
            if ((pass == 0) == tone || try_pitch < params->range_low || try_pitch > params->range_high ||
                (type != NON_CHORD_TONE_ANTICIPATION && !((scale >> util_pitch_class(try_pitch)) & 1u)) ||
                !leap_allowed(try_pitch - before) || !leap_allowed(after - try_pitch)) {
                continue;
            }
            note->pitch = try_pitch;
            if (melody_figure_ok(context, &only, notes, count, j)) {
                *pitch = try_pitch;
                return true;
            }
        }
    }
    return false;
}

/* The chord tones of the motive among the notes between two anchors take chord tones only (R-215). */
static void mark_chord_tones(const MelodyContext *context, MelodyNote *notes, size_t from, size_t to) {
    for (size_t j = from + 1; j < to; j++) {
        int k = notes[j].motive_index;
        if (k >= 0 && (size_t)k < context->motive_tone_count && context->motive_tone[k] == MELODY_TONE_CHORD) {
            notes[j].chord_only = true;
        }
    }
}

/*
 * Before the notes between two anchors are filled: the figures among them get their pitches from the anchors (they are kept
 * by the search), or, when one cannot be formed, a chord tone (R-215). Returns the types of the figures formed, which the
 * fill and the checker allow in this melody.
 */
static unsigned place_figures(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes, size_t count,
                              size_t from, size_t to) {
    unsigned types = 0;
    for (size_t j = from + 1; j < to; j++) {
        int pitch;
        if (!is_figure_of_motive(context, params, notes, count, j)) {
            continue;
        }
        /* The note before is the anchor or a figure before this one, and the note after is the anchor. */
        bool placed = (j - 1 == from || notes[j - 1].fixed) && j + 1 == to;
        if (placed && figure_pitch(context, params, notes, count, j, &pitch)) {
            notes[j].pitch = pitch;
            notes[j].fixed = true;
            types |= figure_bit(context->motive_tone[notes[j].motive_index]);
        } else {
            notes[j].chord_only = true;
        }
    }
    return types;
}

/* Gives up the figures between two anchors that the search could not keep: they take chord tones. */
static void give_up_figures(MelodyNote *notes, size_t from, size_t to) {
    for (size_t j = from + 1; j < to; j++) {
        if (notes[j].fixed) {
            notes[j].fixed = false;
            notes[j].chord_only = true;
        }
    }
}

/*
 * Gives every note a pitch. The anchors take chord tones. A motive anchor takes the chord tone that keeps its
 * interval to the anchor before nearest to the motive's own between the same two notes, within the tolerance for
 * each note between them. The first takes the chord tone nearest the motive's first pitch, which is that pitch
 * itself when it is a chord tone of the first chord (its scale degree and octave kept) and a transposition of the
 * motive otherwise.
 *
 * An anchor after the motive follows the arch planned for the phrase (melody_contour_plan): the climax anchor takes
 * the climax pitch, no other is as high, and the others take the chord tone nearest the arch, by a leap
 * standard-counterpoint allows. A leap of a fourth or more is followed, where the chord allows, by a step the other
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
        for (size_t j = 0; j < count; j++) {
            int move = options[j].pitch - out->notes[before].pitch;
            bool repeat = adjacent && move == 0;
            if ((adjacent && !leap_allowed(move)) || repeat) {
                continue;
            }
            /* The last note of a target that comes before written melody leads to its first note by a leap allowed. */
            if (i + 1 == out->count && context->has_lead_out && !leap_allowed(context->lead_out - options[j].pitch)) {
                continue;
            }
            options[kept++] = options[j];
        }
        if (climax) {
            note->pitch = contour.climax_pitch;
            if (adjacent && !leap_allowed(note->pitch - out->notes[before].pitch)) {
                return false;
            }
        } else if (!pick(options, kept, MAX_MOVE, rng, false, &note->pitch)) {
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
            if (!melody_fill_between(context, params, out->notes, out->count, before, i, between, cap, rng)) {
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
    for (int distance = 1; distance <= 2; distance++) {
        int pitch = from + direction * distance;
        if (pitch >= low && pitch <= high && ((scale >> util_pitch_class(pitch)) & 1u)) {
            return pitch;
        }
    }
    return -1;
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
    Rational unit = position_gcd(context->pickup, beat_time);
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

bool melody_build(const MelodyContext *context, const MelodyFillParams *params, int shift, bool keep_figures,
                  FillRng *rng, MelodyCandidate *out) {
    MelodyFillParams arch = arch_params(context, params);
    memset(out, 0, sizeof(*out));
    out->magic = MELODY_PAYLOAD_MAGIC;
    out->context = *context;
    /* The checker holds every note to the voice's range: the arch caps are for the notes that are generated (R-327). */
    out->params = *params;
    /* A melody with two notes as high as its highest has no one climax. */
    bool assigned = layout(context, &arch, out) && melody_realize(context, params, shift, keep_figures, out) &&
                    assign(context, &arch, rng, out) && place_pickup(context, &arch, rng, out);
    /* The phrase has one highest note: the whole phrase, or the target with its climax, has one, when it generates notes
       (R-335). A target of realized notes alone is scored by melody_contour_score instead. */
    MelodyRole role = melody_role(context, &arch);
    bool unique = assigned && ((role != MELODY_ROLE_WHOLE && role != MELODY_ROLE_CLIMAX) || !melody_has_generated(out) ||
                               melody_contour_unique(out));
    return unique;
}
