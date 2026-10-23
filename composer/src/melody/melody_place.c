#include "melody/melody_internal.h"

#include <stdlib.h>
#include <string.h>

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

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static int absolute(int value) {
    return value < 0 ? -value : value;
}

/* The leaps standard-counterpoint allows: up to a fifth, the ascending minor sixth, and the octave. */
static bool leap_allowed(int distance) {
    int size = absolute(distance);
    return size <= 7 || size == 12 || distance == 8;
}

/* The pitches in range whose pitch class is in mask, nearest to ideal first. */
static size_t options_for(unsigned mask, int low, int high, int ideal, Option *out) {
    size_t count = 0;
    for (int pitch = low; pitch <= high && count < MAX_OPTIONS; pitch++) {
        if ((mask >> pitch_class(pitch)) & 1u) {
            Option option = {pitch, absolute(pitch - ideal)};
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

static MelodyNote *push(MelodyCandidate *out, long onset, long duration) {
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
 * The notes of the phrase: the motive's, then notes that fill the rest. With a density the rest is notes of an
 * even length, that many to a cell; without one it is the motive's rhythm over again, each note as long as the
 * time from one onset of the motive to the next, the last as long as it is.
 */
static bool layout(const MelodyContext *context, const MelodyFillParams *params, MelodyCandidate *out) {
    long start = (long)params->start_cell * context->cell;
    if (context->motive_count == 0 || start + context->motive_length > context->length) {
        return false;
    }
    for (size_t k = 0; k < context->motive_count; k++) {
        MelodyNote *note = push(out, start + context->motive_onset[k], context->motive_duration[k]);
        if (!note) {
            return false;
        }
        note->motive_index = (int)k;
    }

    /* A note that ends the phrase starts with the last chord, so the cadence arrives with it. */
    long at = start + context->motive_length;
    long last = context->chord_start[context->count - 1];
    long bound = context->has_cadence && last > at ? last : context->length;
    long step = params->density > 0 && context->cell % (long)params->density == 0
                    ? context->cell / (long)params->density
                    : context->cell;
    size_t k = 0;
    while (at < bound) {
        long next;
        if (params->density == 0) {
            size_t m = k++ % context->motive_count;
            next = at + (m + 1 < context->motive_count ? context->motive_onset[m + 1] - context->motive_onset[m]
                                                      : context->motive_duration[m]);
        } else {
            next = (at / step + 1) * step;
        }
        if (next > bound) {
            next = bound;
        }
        if (!push(out, at, next - at)) {
            return false;
        }
        at = next;
    }
    if (bound < context->length && !push(out, bound, context->length - bound)) {
        return false;
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = melody_on_cell(context, out->notes[i].onset);
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
    return true;
}

/* Whether the note carries a chord tone of a motive with tones (R-215): it lands on a tone of its chord, however weak. */
static bool is_chord_tone_of_motive(const MelodyContext *context, const MelodyNote *note) {
    return note->motive_index >= 0 && (size_t)note->motive_index < context->motive_tone_count &&
           context->motive_tone[note->motive_index] == MELODY_TONE_CHORD;
}

bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i) {
    const MelodyNote *note = &melody->notes[i];
    return params->nct_types == 0 || i == 0 || i + 1 == melody->count || note->strong ||
           note->duration >= context->cell || note->onset + note->duration > context->chord_start[note->chord + 1] ||
           is_chord_tone_of_motive(context, note);
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
    int first_floor = -1; /* the lowest pitch the first note may take, for the target with the phrase's climax */
    memset(&contour, 0, sizeof(contour));
    for (size_t i = 0; i < out->count; i++) {
        MelodyNote *note = &out->notes[i];
        Option options[MAX_OPTIONS];
        bool adjacent = i > 0 && before + 1 == i;
        bool climax = false;
        unsigned mask;
        int ideal;
        int top = high;
        int reach = MAX_MOVE;
        if (!melody_is_anchor(context, params, out, i)) {
            continue;
        }
        mask = melody_need_mask(context, params, note);
        if (i + 1 == out->count) {
            mask &= melody_cadence_mask(context);
        }
        if (note->motive_index < 0) {
            if (!contour.made && !melody_contour_plan(context, params, out, i, rng, &contour)) {
                return false;
            }
            climax = !contour.in_motive && i == contour.climax;
            top = climax ? high : (contour.cap < high ? contour.cap : high);
        }
        if (i == 0) {
            ideal = context->motive_pitch[0];
            reach = 127;
            /* The target with the phrase's climax rises above every note the phrase has had, and the motive may be
               all there is of it, so the motive is placed high enough to be that. */
            if (context->idea && context->phrase_has_top && melody_role(context, params) == MELODY_ROLE_CLIMAX) {
                int rise = 0;
                for (size_t k = 1; k < context->motive_count; k++) {
                    rise = context->motive_pitch[k] - context->motive_pitch[0] > rise
                               ? context->motive_pitch[k] - context->motive_pitch[0]
                               : rise;
                }
                int wanted_top = context->phrase_top + 1 + (int)fill_rng_below(rng, 3);
                ideal = wanted_top - rise > ideal ? wanted_top - rise : ideal;
                ideal = ideal + rise > params->range_high ? params->range_high - rise : ideal;
                first_floor = context->phrase_top + 1 - rise;
            }
        } else if (note->motive_index >= 0 && out->notes[before].motive_index >= 0) {
            size_t k = (size_t)note->motive_index;
            size_t then = (size_t)out->notes[before].motive_index;
            ideal = out->notes[before].pitch + context->motive_pitch[k] - context->motive_pitch[then];
            reach = params->tolerance * (int)(i - before);
        } else {
            ideal = melody_contour_ideal(context, &contour, out, i, before);
            /* A leap of a fourth or more is answered by a step back, unless this is the climax or the end. */
            if (adjacent && (before > 0 || context->lead_in_count > 0) && i + 1 < out->count) {
                int leap = out->notes[before].pitch -
                           (before > 0 ? out->notes[before - 1].pitch : context->lead_in[context->lead_in_count - 1]);
                if (leap >= MELODY_RECOVER_FROM || leap <= -MELODY_RECOVER_FROM) {
                    ideal = out->notes[before].pitch + (leap > 0 ? -2 : 2);
                }
            }
        }
        size_t count = options_for(mask, low, top, ideal, options);
        size_t kept = 0;
        for (size_t j = 0; j < count; j++) {
            int move = options[j].pitch - out->notes[before].pitch;
            bool repeat = note->motive_index < 0 && adjacent && move == 0;
            if (i > 0 && ((adjacent && !leap_allowed(move)) || repeat)) {
                continue;
            }
            if (i == 0 && first_floor >= 0 && options[j].pitch < first_floor) {
                continue;
            }
            /* The first note of a target that follows written melody joins it by a leap standard-counterpoint allows. */
            if (i == 0 && context->lead_in_count > 0 &&
                !leap_allowed(options[j].pitch - context->lead_in[context->lead_in_count - 1])) {
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
        } else if (!pick(options, kept, reach, rng, context->restating, &note->pitch)) {
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
                const MelodyNote *weak = &out->notes[j];
                if (weak->motive_index >= 0 && out->notes[before].motive_index >= 0) {
                    between[j] = out->notes[before].pitch + context->motive_pitch[weak->motive_index] -
                                 context->motive_pitch[out->notes[before].motive_index];
                } else {
                    between[j] = out->notes[before].pitch +
                                 (note->pitch - out->notes[before].pitch) * (int)(j - before) / (int)(i - before);
                }
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

static long gcd(long a, long b) {
    while (b != 0) {
        long rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

/* The pitch a step (a semitone or a tone, within the scale) from `from` in a direction, in range; -1 when none. */
static int scale_step(unsigned scale, int from, int direction, int low, int high) {
    for (int distance = 1; distance <= 2; distance++) {
        int pitch = from + direction * distance;
        if (pitch >= low && pitch <= high && ((scale >> pitch_class(pitch)) & 1u)) {
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
    PlanTime beat_time;
    long beat;
    if (context->pickup == 0) {
        return true;
    }
    if (plan_time_make(1, (unsigned long)context->metre.meter.beat_unit, &beat_time) != PLAN_OK ||
        !melody_ticks(beat_time, &beat)) {
        return false;
    }
    long unit = gcd(context->pickup, beat);
    long count = context->pickup / unit;
    count = count > MELODY_MAX_PICKUP ? MELODY_MAX_PICKUP : count;
    long first_length = context->pickup - (count - 1) * unit;
    long onset = -context->pickup;
    for (long k = 0; k < count; k++) {
        long length = k == 0 ? first_length : unit;
        out->pickup_notes[k].onset = onset;
        out->pickup_notes[k].duration = length;
        out->pickup_notes[k].motive_index = -1;
        out->pickup_notes[k].chord = 0;
        out->pickup_notes[k].strong = false;
        onset += length;
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
            ((scale >> pitch_class(pitch)) & 1u)) {
            options[steps++] = pitch;
        }
    }
    int chord_tones[16];
    for (int pitch = target - 5; context->pickup_mask != 0 && pitch <= target + 5; pitch++) {
        if (pitch != target && absolute(pitch - target) > 2 && pitch >= params->range_low &&
            pitch <= params->range_high && ((context->pickup_mask >> pitch_class(pitch)) & 1u)) {
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
    for (long k = count - 2; k >= 0; k--) {
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

bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out) {
    MelodyFillParams arch = arch_params(context, params);
    memset(out, 0, sizeof(*out));
    out->magic = MELODY_PAYLOAD_MAGIC;
    out->context = *context;
    out->params = arch;
    /* A melody with two notes as high as its highest has no one climax. */
    bool assigned = layout(context, &arch, out) && assign(context, &arch, rng, out) &&
                    place_pickup(context, &arch, rng, out);
    /* The phrase has one highest note: the whole phrase, or the target with its climax, has one. */
    MelodyRole role = melody_role(context, &arch);
    bool unique = assigned && ((role != MELODY_ROLE_WHOLE && role != MELODY_ROLE_CLIMAX) || melody_contour_unique(out));
    return unique;
}
