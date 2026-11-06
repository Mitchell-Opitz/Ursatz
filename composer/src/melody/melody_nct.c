#include "melody/melody_internal.h"

#include <string.h>
#include "util/util.h"

#include <standard-counterpoint/melodic_rules.h>
#include <standard-tonality/tonal_key.h>

/* The most pitches tried for one note, and the most notes visited in one search. */
#define TRIES_PER_NOTE 5
#define SEARCH_BUDGET 4000
/* Distance counts this much against a draw below it, so a draw only orders pitches equally far from the ideal. */
#define JITTER 4

/* A step is a second, a half or whole tone; a leap is anything larger. */
static bool is_step(int move) {
    return standard_counterpoint_is_step(move);
}

static bool is_leap(int move) {
    return move != 0 && !standard_counterpoint_is_step(move);
}

/* The moves standard-counterpoint allows: no tritone, no seventh, the sixth only as the ascending minor one. */
static bool leap_allowed(int move) {
    return standard_counterpoint_melodic_semitones_allowed(move);
}

unsigned melody_scale_mask(const MelodyContext *context, size_t chord) {
    int tonic = analysis_key_tonic_pitch_class(&context->key);
    if (context->key.mode != KEY_STRUCTURE_MODE_MINOR) {
        return util_scale_mask(tonic, false);
    }
    /* The harmonic minor where the chord has the raised seventh (the natural seventh beside it makes no melody), else the natural minor. */
    unsigned raised = 1u << ((tonic + 11) % 12);
    return standard_tonality_mode_pitch_classes(tonic, (context->chord_mask[chord] & raised)
                                                           ? STANDARD_TONALITY_MODE_HARMONIC_MINOR
                                                           : STANDARD_TONALITY_MODE_NATURAL_MINOR);
}

bool melody_figure_ok(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *notes,
                      size_t count, size_t i) {
    const MelodyNote *note = &notes[i];
    unsigned types = params->nct_types;
    unsigned pitch_bit = 1u << util_pitch_class(note->pitch);
    Rational end = position_add(note->onset, note->duration);
    bool crossing = rational_cmp(end, context->chord_start[note->chord + 1]) > 0;
    bool has_next = i + 1 < count;
    int to_next = has_next ? notes[i + 1].pitch - note->pitch : 0;
    int from_prev = i > 0 ? note->pitch - notes[i - 1].pitch : 0;

    /* A chord tone of every chord it sounds through needs no figure. */
    bool tone_of_all = true;
    for (size_t c = note->chord; c < context->count && rational_cmp(context->chord_start[c], end) < 0; c++) {
        tone_of_all = tone_of_all && (context->chord_mask[c] & pitch_bit);
    }
    if (tone_of_all) {
        return true;
    }
    if (crossing) {
        /* Held into a chord it is not a tone of, and left by a step down to a tone of the chord it is held in. */
        return (types & MELODY_NCT_SUSPENSION) && (context->chord_mask[note->chord] & pitch_bit) && has_next &&
               is_step(to_next) && to_next < 0 &&
               (context->chord_mask[notes[i + 1].chord] & (1u << util_pitch_class(notes[i + 1].pitch)));
    }

    /* Dissonant against its own chord: approached and left as a figure the parameters allow. */
    if (i == 0 || !has_next) {
        return false;
    }
    if (!note->strong) {
        if ((types & MELODY_NCT_PASSING) && is_step(from_prev) && is_step(to_next) &&
            util_sign(from_prev) == util_sign(to_next)) {
            return true;
        }
        if ((types & MELODY_NCT_NEIGHBOR) && is_step(from_prev) && is_step(to_next) &&
            util_sign(from_prev) == -util_sign(to_next) && notes[i + 1].pitch == notes[i - 1].pitch) {
            return true;
        }
        if ((types & MELODY_NCT_ANTICIPATION) && to_next == 0 && notes[i + 1].chord != note->chord &&
            (context->chord_mask[notes[i + 1].chord] & pitch_bit)) {
            return true;
        }
    }
    return (types & MELODY_NCT_APPOGGIATURA) && is_leap(from_prev) && is_step(to_next) &&
           util_sign(to_next) == -util_sign(from_prev);
}

typedef struct {
    int pitch;
    int key;
} Try;

/* Gives notes j to to - 1 pitches, the best-fitting first, checking each note once the one after it is set. */
static bool search(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes, size_t count,
                   size_t from, size_t to, size_t j, const int *ideal, int cap, FillRng *rng, int *budget) {
    if (j == to) {
        /* The run from the anchors before the notes to the anchor after them holds the framework's melodic rules (outlines too). */
        int run[MELODY_WINDOW_MAX];
        size_t used = 0;
        for (size_t i = from > 3 ? from - 3 : 0; i <= to; i++) {
            run[used++] = notes[i].pitch;
        }
        return leap_allowed(notes[to].pitch - notes[to - 1].pitch) &&
               (to - 1 == from || melody_figure_ok(context, params, notes, count, to - 1)) &&
               melody_window_hard(context, run, used) == 0;
    }
    if ((*budget)-- <= 0) {
        return false;
    }
    MelodyNote *note = &notes[j];
    unsigned allowed = context->chord_mask[note->chord];
    if (params->nct_types) {
        allowed |= melody_scale_mask(context, note->chord);
    }
    Try tries[TRIES_PER_NOTE];
    size_t kept = 0;
    int high = params->range_high < cap ? params->range_high : cap;
    for (int pitch = params->range_low; pitch <= high; pitch++) {
        if (!((allowed >> util_pitch_class(pitch)) & 1u) || !leap_allowed(pitch - notes[j - 1].pitch)) {
            continue;
        }
        Try candidate = {pitch, util_absolute(pitch - ideal[j]) * JITTER + (int)fill_rng_below(rng, JITTER)};
        if (kept == TRIES_PER_NOTE && tries[kept - 1].key <= candidate.key) {
            continue;
        }
        size_t at = kept < TRIES_PER_NOTE ? kept++ : TRIES_PER_NOTE - 1;
        while (at > 0 && tries[at - 1].key > candidate.key) {
            tries[at] = tries[at - 1];
            at--;
        }
        tries[at] = candidate;
    }
    for (size_t t = 0; t < kept; t++) {
        note->pitch = tries[t].pitch;
        if (j - 1 > from && !melody_figure_ok(context, params, notes, count, j - 1)) {
            continue;
        }
        if (search(context, params, notes, count, from, to, j + 1, ideal, cap, rng, budget)) {
            return true;
        }
    }
    return false;
}

bool melody_fill_between(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes,
                         size_t count, size_t from, size_t to, const int *ideal, int cap, FillRng *rng) {
    int budget = SEARCH_BUDGET;
    if (to <= from + 1) {
        return true;
    }
    return search(context, params, notes, count, from, to, from + 1, ideal, cap, rng, &budget);
}
