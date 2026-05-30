#include "melody/melody_internal.h"

#include <string.h>

/* The most the climax is placed above the top of the motive, past the pitch above it, in semitones. */
#define CLIMAX_RISE 4
#define CLIMAX_SCALE 1000
/* The most a climax of a span on beats rises above the last anchor of its motive, in semitones: a fifth. */
#define CLIMB_REACH 7
/* A climax within this many thousandths of the place asked for is at that place. */
#define POSITION_SLACK 150
#define SOFT_RULES 4

static int absolute(int value) {
    return value < 0 ? -value : value;
}

static long absolute_long(long value) {
    return value < 0 ? -value : value;
}

long melody_climax_tick(const MelodyContext *context, const MelodyFillParams *params) {
    unsigned position = params->climax_position > CLIMAX_SCALE ? CLIMAX_SCALE : params->climax_position;
    if (!context->idea) {
        return context->length * (long)position / CLIMAX_SCALE;
    }
    return context->phrase_start + context->phrase_length * (long)position / CLIMAX_SCALE;
}

MelodyRole melody_role(const MelodyContext *context, const MelodyFillParams *params) {
    if (!context->idea) {
        return MELODY_ROLE_WHOLE;
    }
    long tick = melody_climax_tick(context, params);
    /* The phrase's last tick belongs to the last target. */
    bool at_phrase_end = context->phrase_start + context->phrase_length == context->length;
    if (tick < 0) {
        return MELODY_ROLE_AFTER;
    }
    return tick < context->length || (at_phrase_end && tick == context->length) ? MELODY_ROLE_CLIMAX
                                                                                  : MELODY_ROLE_BEFORE;
}

bool melody_contour_plan(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                         size_t first, FillRng *rng, MelodyContour *out) {
    long wanted = melody_climax_tick(context, params);
    wanted = wanted < 0 ? 0 : (wanted > context->length ? context->length : wanted);
    int top = 0;
    memset(out, 0, sizeof(*out));
    out->made = true;
    for (size_t i = 0; i < first; i++) {
        top = melody->notes[i].pitch > top ? melody->notes[i].pitch : top;
    }
    out->motive_top = top;
    /* The climax of a phrase is higher than every note of the phrase written before it. */
    if (context->idea && context->phrase_has_top && context->phrase_top > top) {
        top = context->phrase_top;
    }
    out->in_motive = true;
    out->cap = top - 1;

    /*
     * The climax is the anchor nearest the place asked for, other than the last note, which is the cadence's. A target
     * that is one idea of a phrase has the cadence's note only when it asks for a cadence, and takes the nearest anchor
     * that has a chord tone to rise to.
     */
    /* An idea that is not the phrase's climax peaks only a little over its motive, so the climax has room above it. */
    bool low_peak = context->idea && melody_role(context, params) != MELODY_ROLE_CLIMAX;
    int peak = low_peak ? top + 1 + (int)fill_rng_below(rng, 2) : top + 2 + (int)fill_rng_below(rng, CLIMAX_RISE);
    size_t order[MELODY_MAX_NOTES];
    size_t candidates = 0;
    bool idea_last = context->idea && !context->has_cadence;
    for (size_t j = first; j < melody->count; j++) {
        if ((j + 1 < melody->count || idea_last) && melody_is_anchor(context, params, melody, j)) {
            order[candidates++] = j;
        }
    }
    /* Nearest the wanted place first; the sort is stable, so earlier notes win a tie. */
    for (size_t a = 1; a < candidates; a++) {
        size_t moving = order[a];
        long moving_distance = absolute_long(melody->notes[moving].onset - wanted);
        size_t at = a;
        while (at > 0 && absolute_long(melody->notes[order[at - 1]].onset - wanted) > moving_distance) {
            order[at] = order[at - 1];
            at--;
        }
        order[at] = moving;
    }
    size_t limit = context->idea ? candidates : (candidates > 0 ? 1 : 0);
    /* An idea that is not the phrase's climax stays in the lower half of the range, if its motive allows, so the
       climax has the upper half to rise in. */
    int ceiling = params->range_high;
    if (low_peak) {
        int middle = (params->range_low + params->range_high) / 2;
        ceiling = top + 2 > middle ? top + 2 : middle;
        ceiling = ceiling < params->range_high ? ceiling : params->range_high;
    }
    /*
     * A span that does not run from a barline over whole bars has few anchors after its motive, and a climax more than a
     * fifth above the last anchor of the motive cannot be reached by notes that leap as the rules allow: the climax is
     * left in the motive then, as it is when no pitch is free.
     */
    bool beat_span = context->pickup > 0 || context->metre.offset.num != 0 || context->metre.lead.num != 0 ||
                     context->metre.tail.num != 0;
    bool has_reach = false;
    int reach_from = 0;
    for (size_t j = 0; beat_span && j < first; j++) {
        if (melody_is_anchor(context, params, melody, j)) {
            reach_from = melody->notes[j].pitch;
            has_reach = true;
        }
    }
    for (size_t k = 0; k < limit; k++) {
        size_t best = order[k];
        unsigned mask = melody_need_mask(context, params, &melody->notes[best]);
        int found = -1;
        for (int pitch = top + 1; pitch <= ceiling; pitch++) {
            if (has_reach && pitch - reach_from > CLIMB_REACH) {
                continue;
            }
            if (((mask >> (((pitch % 12) + 12) % 12)) & 1u) &&
                (found < 0 || absolute(pitch - peak) < absolute(found - peak))) {
                found = pitch;
            }
        }
        if (found >= 0) {
            out->in_motive = false;
            out->climax = best;
            out->climax_pitch = found;
            out->cap = found - 1;
            break;
        }
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

    /* The climax falls where it was asked to, within the slack. A target that is one idea of a phrase is not asked. */
    if (melody->context.idea) {
        score += 1;
    } else if (melody->count > 0 && melody->context.length > 0) {
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
