#include "melody/melody_internal.h"

#include <stdlib.h>
#include <string.h>
#include "util/util.h"

#include <standard-counterpoint/melodic_rules.h>

/* How far, in semitones, a note moves to re-form its own figure (R-335). */
#define FIGURE_REACH 2
/* The octave shifts a realized idea may take, nearest first, the lower before the higher (R-334). */
#define SHIFT_COUNT 5

/* A pitch a moving note may take, and how it compares with the others. */
typedef struct {
    int pitch;
    int distance; /* from the note's target */
    int forbidden; /* its leaps to the neighbours that standard-counterpoint forbids and the motive does not make */
    int error;    /* how far its intervals to the neighbours are from the motive's */
    int away;     /* its distance from the note before */
} Pick;

typedef struct {
    const MelodyContext *context;
    const MelodyFillParams *params;
    MelodyCandidate *out;
    MelodyCandidate *probe; /* the motive's notes alone, for the labeler */
    size_t count;
    int target[MOTIVE_MAX_NOTES];
    unsigned mask[MOTIVE_MAX_NOTES]; /* the pitch classes the note may be a chord tone of */
    bool tone[MOTIVE_MAX_NOTES];     /* the note is a tone of every chord it sounds through */
    bool moved[MOTIVE_MAX_NOTES];
} Realize;

/* Nearest to the target first (R-328); then fewer rules the neighbours break, a tie-break only (R-338). */
static bool better(const Pick *a, const Pick *b) {
    if (a->distance != b->distance) {
        return a->distance < b->distance;
    }
    if (a->forbidden != b->forbidden) {
        return a->forbidden < b->forbidden;
    }
    if (a->error != b->error) {
        return a->error < b->error;
    }
    if (a->away != b->away) {
        return a->away > b->away;
    }
    return a->pitch < b->pitch;
}

/* How a pitch for note k compares: its distance from the target, the hard melodic rules the run of notes around it breaks and parallels with the bass (ties only), and its intervals to the neighbours against the motive's. */
static Pick measure(const Realize *r, size_t k, int pitch) {
    const MelodyContext *context = r->context;
    Pick pick = {pitch, util_absolute(pitch - r->target[k]), 0, 0, 0};
    int run[MELODY_WINDOW_MAX];
    size_t used = 0;
    /* Two notes either side of it: the join to the idea before is not held (R-335). */
    for (size_t j = k > 2 ? k - 2 : 0; j < k; j++) {
        run[used++] = r->out->notes[j].pitch;
    }
    run[used++] = pitch;
    for (size_t j = k + 1; j < r->count && j <= k + 2; j++) {
        run[used++] = r->out->notes[j].pitch;
    }
    pick.forbidden = (int)util_bits_set(melody_window_hard(context, run, used));
    if (k > 0) {
        int leap = pitch - r->out->notes[k - 1].pitch;
        int own = context->motive_pitch[k] - context->motive_pitch[k - 1];
        pick.error += util_absolute(leap - own);
        pick.away = util_absolute(leap);
        pick.forbidden += melody_parallel_perfects(r->out->notes[k - 1].pitch, pitch,
                                            melody_bass_at(context, r->out->notes[k - 1].onset),
                                            melody_bass_at(context, r->out->notes[k].onset));
    }
    if (k + 1 < r->count) {
        int leap = r->out->notes[k + 1].pitch - pitch;
        int own = context->motive_pitch[k + 1] - context->motive_pitch[k];
        pick.error += util_absolute(leap - own);
        pick.forbidden += melody_parallel_perfects(pitch, r->out->notes[k + 1].pitch, melody_bass_at(context, r->out->notes[k].onset),
                                            melody_bass_at(context, r->out->notes[k + 1].onset));
    }
    return pick;
}

/* Labels the motive's notes as they stand; false when the labeler cannot. */
static bool survey(Realize *r, unsigned *types, bool *loose) {
    for (size_t k = 0; k < r->count; k++) {
        r->probe->notes[k].pitch = r->out->notes[k].pitch;
    }
    return melody_nct_survey(r->probe, types, loose);
}

/* The nearest tone of the note's chords to its target (R-328 step 4); false when the range has none. */
static bool nearest_tone(const Realize *r, size_t k, int *pitch) {
    Pick best = {0, 0, 0, 0, 0};
    bool found = false;
    for (int p = r->params->range_low; p <= r->params->range_high; p++) {
        if ((r->mask[k] >> util_pitch_class(p)) & 1u) {
            Pick pick = measure(r, k, p);
            if (!found || better(&pick, &best)) {
                best = pick;
                found = true;
            }
        }
    }
    *pitch = best.pitch;
    return found;
}

/*
 * A pitch within FIGURE_REACH of the target, not a chord tone, that the labeler gives the same figure as the motive's
 * own tone role names, best first by the ties of R-335. False when none does.
 */
static bool figure_near(Realize *r, size_t k, int *pitch) {
    unsigned want = melody_nct_bit(r->context->motive_tone[k]);
    unsigned types[MELODY_MAX_NOTES];
    bool loose[MELODY_MAX_NOTES];
    Pick tried[2 * FIGURE_REACH];
    size_t count = 0;
    int keep = r->out->notes[k].pitch;
    bool found = false;
    /* A figure that may not sit on a strong beat is not formed there. */
    if (r->out->notes[k].strong && !(want & MELODY_STRONG_FIGURES)) {
        return false;
    }
    for (int p = r->target[k] - FIGURE_REACH; p <= r->target[k] + FIGURE_REACH; p++) {
        if (p != r->target[k] && p >= r->params->range_low && p <= r->params->range_high &&
            !((r->mask[k] >> util_pitch_class(p)) & 1u) && count < 2 * FIGURE_REACH) {
            Pick pick = measure(r, k, p);
            size_t at = count++;
            while (at > 0 && better(&pick, &tried[at - 1])) {
                tried[at] = tried[at - 1];
                at--;
            }
            tried[at] = pick;
        }
    }
    for (size_t i = 0; want != 0 && !found && i < count; i++) {
        r->out->notes[k].pitch = tried[i].pitch;
        if (survey(r, types, loose) && types[k] == want && !loose[k]) {
            *pitch = tried[i].pitch;
            found = true;
        }
    }
    r->out->notes[k].pitch = keep;
    return found;
}

/*
 * Moves every clash the labeler cannot classify (all of them without keep_figures), and labels again, until none moves.
 * A note moves to a pitch that re-forms its own figure the first time, and to the nearest chord tone after that, so the
 * passes end; what is left after them is moved to a chord tone.
 */
static bool settle(Realize *r, bool keep_figures) {
    unsigned types[MELODY_MAX_NOTES] = {0};
    bool loose[MELODY_MAX_NOTES];
    for (size_t pass = 0; pass <= r->count + 1; pass++) {
        bool changed = false;
        if (keep_figures) {
            if (!survey(r, types, loose)) {
                return false;
            }
        } else {
            memset(loose, 0, sizeof(loose));
            for (size_t k = 0; k < r->count; k++) {
                loose[k] = !r->tone[k];
            }
        }
        for (size_t k = 0; k < r->count; k++) {
            int pitch;
            /* A clash on a strong beat stays only as a figure that may be there (the checker's rule, R-335). */
            bool strong_clash = keep_figures && r->out->notes[k].strong && types[k] != 0 &&
                                !(types[k] & MELODY_STRONG_FIGURES);
            if (r->tone[k] || !(loose[k] || strong_clash)) {
                continue;
            }
            bool figure = keep_figures && pass <= r->count && !r->moved[k] && k < r->context->motive_tone_count &&
                          r->context->motive_tone[k] >= 0 && figure_near(r, k, &pitch);
            if (!figure && !nearest_tone(r, k, &pitch)) {
                return false;
            }
            r->tone[k] = !figure;
            r->moved[k] = true;
            r->out->notes[k].pitch = pitch;
            changed = true;
        }
        if (!changed) {
            return true;
        }
    }
    return true;
}

bool melody_realize(const MelodyContext *context, const MelodyFillParams *params, int shift, bool keep_figures,
                    MelodyCandidate *out) {
    unsigned types[MELODY_MAX_NOTES];
    bool loose[MELODY_MAX_NOTES];
    MelodyFillParams need = *params;
    Realize *r = calloc(1, sizeof(*r));
    bool ok = r && context->motive_count > 0 && context->motive_count <= out->count;
    /* A suspension is held into a chord it is not a tone of: here it is a clash, whatever the parameters allow. */
    need.nct_types &= ~MELODY_NCT_SUSPENSION;
    if (ok) {
        r->probe = malloc(sizeof(*r->probe));
        ok = r->probe != NULL;
    }
    if (ok) {
        r->context = context;
        r->params = params;
        r->out = out;
        r->count = context->motive_count;
        *r->probe = *out;
        r->probe->count = r->count;
        r->probe->pickup_count = 0;
        r->probe->params.nct_types = ~0u;
    }
    for (size_t k = 0; ok && k < r->count; k++) {
        MelodyNote *note = &out->notes[k];
        ok = note->motive_index == (int)k;
        r->target[k] = context->motive_pitch[k] + shift;
        ok = ok && r->target[k] >= params->range_low && r->target[k] <= params->range_high;
        note->pitch = r->target[k];
        r->mask[k] = melody_need_mask(context, &need, note);
        if (r->mask[k] == 0) {
            r->mask[k] = context->chord_mask[note->chord];
        }
        r->tone[k] = (r->mask[k] >> util_pitch_class(note->pitch)) & 1u;
    }
    /* Steps a to d: what is a chord tone stays; a clash stays when the labeler gives it a figure, else it moves. */
    ok = ok && settle(r, keep_figures);
    /* Step e: the last note of an idea that ends in a cadence moves to the nearest tone of it (R-316). */
    if (ok && context->has_cadence && r->count == out->count) {
        size_t last = r->count - 1;
        unsigned cadence = melody_cadence_mask(context);
        unsigned wanted = r->mask[last] & cadence ? r->mask[last] & cadence : cadence;
        if (!((wanted >> util_pitch_class(out->notes[last].pitch)) & 1u) || !r->tone[last]) {
            unsigned keep = r->mask[last];
            r->mask[last] = wanted;
            ok = nearest_tone(r, last, &out->notes[last].pitch);
            r->mask[last] = keep | wanted;
            r->tone[last] = true;
        }
    }
    ok = ok && settle(r, keep_figures);
    if (ok && keep_figures) {
        ok = survey(r, types, loose);
        for (size_t k = 0; ok && k < r->count; k++) {
            if (!r->tone[k]) {
                out->params.nct_types |= types[k];
            }
        }
    }
    if (r) {
        free(r->probe);
    }
    free(r);
    return ok;
}

size_t melody_realize_shifts(const MelodyContext *context, const MelodyFillParams *params, int *shifts) {
    static const int order[SHIFT_COUNT] = {0, -12, 12, -24, 24};
    int low = 127;
    int high = 0;
    size_t count = 0;
    bool climax = context->idea && context->phrase_has_top && melody_role(context, params) == MELODY_ROLE_CLIMAX;
    for (size_t k = 0; k < context->motive_count; k++) {
        low = context->motive_pitch[k] < low ? context->motive_pitch[k] : low;
        high = context->motive_pitch[k] > high ? context->motive_pitch[k] : high;
    }
    for (size_t i = 0; context->motive_count > 0 && i < SHIFT_COUNT; i++) {
        if (low + order[i] >= params->range_low && high + order[i] <= params->range_high) {
            /* The climax target's shifts that put its top at the phrase's top, or a little over it, come first. */
            bool top = climax && high + order[i] >= context->phrase_top && high + order[i] <= context->phrase_top + 2;
            size_t at = count++;
            while (at > 0 && top && !(climax && high + shifts[at - 1] >= context->phrase_top &&
                                       high + shifts[at - 1] <= context->phrase_top + 2)) {
                shifts[at] = shifts[at - 1];
                at--;
            }
            shifts[at] = order[i];
        }
    }
    return count;
}
