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

/* Of the first options within max_distance, the nearest most often, else the next. */
static bool pick(const Option *options, size_t count, int max_distance, FillRng *rng, int *pitch) {
    size_t usable = 0;
    while (usable < count && options[usable].distance <= max_distance) {
        usable++;
    }
    uint64_t draw = fill_rng_below(rng, NEAREST_OF);
    if (usable == 0) {
        return false;
    }
    *pitch = options[usable > 1 && draw >= NEAREST_IN ? 1 : 0].pitch;
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
    long bound = last > at ? last : context->length;
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
        out->notes[i].strong = out->notes[i].onset % context->cell == 0;
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
    return true;
}

bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i) {
    const MelodyNote *note = &melody->notes[i];
    return params->nct_types == 0 || i == 0 || i + 1 == melody->count || note->strong ||
           note->duration >= context->cell || note->onset + note->duration > context->chord_start[note->chord + 1];
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
        } else if (note->motive_index >= 0 && out->notes[before].motive_index >= 0) {
            size_t k = (size_t)note->motive_index;
            size_t then = (size_t)out->notes[before].motive_index;
            ideal = out->notes[before].pitch + context->motive_pitch[k] - context->motive_pitch[then];
            reach = params->tolerance * (int)(i - before);
        } else {
            ideal = melody_contour_ideal(context, &contour, out, i, before);
            /* A leap of a fourth or more is answered by a step back, unless this is the climax or the end. */
            if (adjacent && before > 0 && i + 1 < out->count) {
                int leap = out->notes[before].pitch - out->notes[before - 1].pitch;
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
            options[kept++] = options[j];
        }
        if (climax) {
            note->pitch = contour.climax_pitch;
            if (adjacent && !leap_allowed(note->pitch - out->notes[before].pitch)) {
                return false;
            }
        } else if (!pick(options, kept, reach, rng, &note->pitch)) {
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

bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out) {
    memset(out, 0, sizeof(*out));
    out->magic = MELODY_PAYLOAD_MAGIC;
    out->context = *context;
    out->params = *params;
    /* A melody with two notes as high as its highest has no one climax. */
    return layout(context, params, out) && assign(context, params, rng, out) && melody_contour_unique(out);
}
