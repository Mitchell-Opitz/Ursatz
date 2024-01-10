#include "melody/melody_internal.h"

#include <string.h>

/* The most the climax is placed above the top of the motive, past the pitch above it, in semitones. */
#define CLIMAX_RISE 4
#define CLIMAX_SCALE 1000
/* A climax within this many thousandths of the place asked for is at that place. */
#define POSITION_SLACK 150
#define SOFT_RULES 4

static int absolute(int value) {
    return value < 0 ? -value : value;
}

bool melody_contour_plan(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                         size_t first, FillRng *rng, MelodyContour *out) {
    unsigned position = params->climax_position > CLIMAX_SCALE ? CLIMAX_SCALE : params->climax_position;
    long wanted = context->length * (long)position / CLIMAX_SCALE;
    size_t best = melody->count;
    long best_distance = 0;
    int top = 0;
    memset(out, 0, sizeof(*out));
    out->made = true;
    for (size_t i = 0; i < first; i++) {
        top = melody->notes[i].pitch > top ? melody->notes[i].pitch : top;
    }
    out->motive_top = top;
    out->in_motive = true;
    out->cap = top - 1;

    /* The climax is the anchor nearest the place asked for, other than the last note, which is the cadence's. */
    for (size_t j = first; j + 1 < melody->count; j++) {
        long distance = melody->notes[j].onset > wanted ? melody->notes[j].onset - wanted : wanted - melody->notes[j].onset;
        if (melody_is_anchor(context, params, melody, j) && (best == melody->count || distance < best_distance)) {
            best = j;
            best_distance = distance;
        }
    }
    int peak = top + 2 + (int)fill_rng_below(rng, CLIMAX_RISE);
    if (best == melody->count) {
        return true;
    }
    unsigned mask = melody_need_mask(context, params, &melody->notes[best]);
    int found = -1;
    for (int pitch = top + 1; pitch <= params->range_high; pitch++) {
        if (((mask >> (((pitch % 12) + 12) % 12)) & 1u) && (found < 0 || absolute(pitch - peak) < absolute(found - peak))) {
            found = pitch;
        }
    }
    if (found >= 0) {
        out->in_motive = false;
        out->climax = best;
        out->climax_pitch = found;
        out->cap = found - 1;
    }
    return true;
}

int melody_contour_ideal(const MelodyContext *context, const MelodyContour *contour, const MelodyCandidate *melody,
                         size_t i, size_t before) {
    const MelodyNote *note = &melody->notes[i];
    const MelodyNote *prev = &melody->notes[before];
    if (contour->in_motive) {
        return prev->pitch;
    }
    const MelodyNote *climax = &melody->notes[contour->climax];
    if (i < contour->climax) {
        /* Rising: part of the way from the note before to the climax, by time. */
        long span = climax->onset - prev->onset;
        return span > 0 ? prev->pitch + (int)((long)(contour->climax_pitch - prev->pitch) * (note->onset - prev->onset) / span)
                        : prev->pitch;
    }
    /* Falling: from the climax back toward where the motive began, by time. */
    long span = context->length - climax->onset;
    int target = melody->notes[0].pitch;
    return span > 0 ? contour->climax_pitch + (int)((long)(target - contour->climax_pitch) * (note->onset - climax->onset) / span)
                    : prev->pitch;
}

bool melody_contour_unique(const MelodyCandidate *melody) {
    int top = -1;
    size_t count = 0;
    for (size_t i = 0; i < melody->count; i++) {
        if (melody->notes[i].pitch > top) {
            top = melody->notes[i].pitch;
            count = 1;
        } else if (melody->notes[i].pitch == top) {
            count++;
        }
    }
    return count == 1;
}

void melody_contour_score(const MelodyCandidate *melody, unsigned *held, unsigned *total) {
    unsigned score = 0;
    size_t top = 0;
    size_t repeats = 0;
    bool recovered = true;
    for (size_t i = 1; i < melody->count; i++) {
        top = melody->notes[i].pitch > melody->notes[top].pitch ? i : top;
        repeats += melody->notes[i].pitch == melody->notes[i - 1].pitch;
    }
    score += melody_contour_unique(melody);

    /* The climax falls where it was asked to, within the slack. */
    if (melody->count > 0 && melody->context.length > 0) {
        long position = melody->notes[top].onset * CLIMAX_SCALE / melody->context.length;
        long asked = melody->params.climax_position > CLIMAX_SCALE ? CLIMAX_SCALE : melody->params.climax_position;
        score += (position > asked ? position - asked : asked - position) <= POSITION_SLACK;
    }

    /* A leap of a fourth or more is followed by a step the other way. */
    for (size_t i = 1; i + 1 < melody->count; i++) {
        int leap = melody->notes[i].pitch - melody->notes[i - 1].pitch;
        int next = melody->notes[i + 1].pitch - melody->notes[i].pitch;
        if (absolute(leap) >= MELODY_RECOVER_FROM && !(absolute(next) >= 1 && absolute(next) <= 2 && (leap > 0) != (next > 0))) {
            recovered = false;
        }
    }
    score += recovered;

    /* No more than one note in six repeats the one before. */
    score += repeats * 6 <= melody->count;
    *held = score;
    *total = SOFT_RULES;
}
