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

/* The notes of the phrase: the motive's, then notes that fill the rest, one to a cell. */
static bool layout(const MelodyContext *context, const MelodyFillParams *params, MelodyCandidate *out) {
    long start = (long)params->start_cell * context->cell;
    if (context->motive_count == 0 || start + context->motive_length > context->length) {
        return false;
    }
    for (size_t k = 0; k < context->motive_count; k++) {
        MelodyNote *note = &out->notes[out->count++];
        note->onset = start + context->motive_onset[k];
        note->duration = context->motive_duration[k];
        note->motive_index = (int)k;
    }

    /* A note that ends the phrase starts with the last chord, so the cadence arrives with it. */
    long at = start + context->motive_length;
    long last = context->chord_start[context->count - 1];
    long bound = last > at ? last : context->length;
    while (at < bound) {
        long next = (at / context->cell + 1) * context->cell;
        if (next > bound) {
            next = bound;
        }
        if (out->count >= MELODY_MAX_NOTES) {
            return false;
        }
        MelodyNote *note = &out->notes[out->count++];
        note->onset = at;
        note->duration = next - at;
        note->motive_index = -1;
        at = next;
    }
    if (bound < context->length) {
        if (out->count >= MELODY_MAX_NOTES) {
            return false;
        }
        MelodyNote *note = &out->notes[out->count++];
        note->onset = bound;
        note->duration = context->length - bound;
        note->motive_index = -1;
    }
    for (size_t i = 0; i < out->count; i++) {
        out->notes[i].strong = out->notes[i].onset % context->cell == 0;
        out->notes[i].chord = melody_chord_at(context, out->notes[i].onset);
    }
    return true;
}

/*
 * Gives every note a pitch. A note on a strong beat takes a chord tone, and so does every other note here. A
 * motive note takes the chord tone that keeps its interval to the note before nearest to the motive's own, within
 * the tolerance. The first takes the chord tone nearest the motive's first pitch, which is that pitch itself when it
 * is a chord tone of the first chord (its scale degree and octave kept) and a transposition of the motive otherwise.
 * A note after the motive takes a chord tone near the note before it, by a leap standard-counterpoint allows.
 */
static bool assign(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out) {
    int low = params->range_low;
    int high = params->range_high;
    for (size_t i = 0; i < out->count; i++) {
        MelodyNote *note = &out->notes[i];
        Option options[MAX_OPTIONS];
        unsigned mask = melody_need_mask(context, params, note);
        int ideal;
        int reach = MAX_MOVE;
        if (i == 0) {
            ideal = context->motive_pitch[0];
            reach = 127;
        } else if (note->motive_index >= 0) {
            size_t k = (size_t)note->motive_index;
            ideal = out->notes[i - 1].pitch + context->motive_pitch[k] - context->motive_pitch[k - 1];
            reach = params->tolerance;
        } else {
            ideal = out->notes[i - 1].pitch;
        }
        size_t count = options_for(mask, low, high, ideal, options);
        size_t kept = 0;
        for (size_t j = 0; j < count; j++) {
            int move = options[j].pitch - (i > 0 ? out->notes[i - 1].pitch : options[j].pitch);
            bool repeat = note->motive_index < 0 && i > 0 && move == 0;
            if ((i > 0 && !leap_allowed(move)) || repeat) {
                continue;
            }
            options[kept++] = options[j];
        }
        if (!pick(options, kept, reach, rng, &note->pitch)) {
            return false;
        }
    }
    return true;
}

bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out) {
    memset(out, 0, sizeof(*out));
    out->magic = MELODY_PAYLOAD_MAGIC;
    out->context = *context;
    out->params = *params;
    return layout(context, params, out) && assign(context, params, rng, out);
}
