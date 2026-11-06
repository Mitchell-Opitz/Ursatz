#include "melody/melody_internal.h"

#include <stdlib.h>
#include <string.h>
#include "util/util.h"

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
    bool force[MOTIVE_MAX_NOTES]; /* a chord tone that is dissonant against the bass: it moves to a consonant one */
} Realize;

static bool better(const Pick *a, const Pick *b) {
    if (a->forbidden != b->forbidden) {
        return a->forbidden < b->forbidden;
    }
    if (a->distance != b->distance) {
        return a->distance < b->distance;
    }
    if (a->error != b->error) {
        return a->error < b->error;
    }
    if (a->away != b->away) {
        return a->away > b->away;
    }
    return a->pitch < b->pitch;
}

/* The pitch of the bass sounding at a position, or -1 when none does. */
static int bass_at(const MelodyContext *context, Rational position) {
    int pitch = -1;
    for (size_t b = 0; b < context->bass_count; b++) {
        if (rational_cmp(context->bass[b].onset, position) <= 0 && rational_cmp(position, context->bass[b].end) < 0) {
            pitch = context->bass[b].pitch;
        }
    }
    return pitch;
}

/* Whether two melody notes, with the bass under each, move in parallel octaves or fifths (the checker's hard rule). */
static bool parallel_perfects(int from, int to, int bass_from, int bass_to) {
    int a = util_pitch_class(from - bass_from);
    int b = util_pitch_class(to - bass_to);
    return bass_from >= 0 && bass_to >= 0 && from != to && a == b && (a == 0 || a == 7);
}

/* How a pitch for note k compares: its distance from the target, the hard melodic rules the run of notes around it breaks, and its intervals to the neighbours against the motive's. */
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
    /* A tone that must resolve (the leading tone, a chord's seventh) is not left to the notes that follow when they are
       generated: the chord may change under it before they could resolve it. */
    if (k + 1 == r->count && r->count < r->out->count &&
        (util_pitch_class(pitch) == util_pitch_class(analysis_key_tonic_pitch_class(&context->key) + 11) ||
         (context->chord_seventh[r->out->notes[k].chord] >> util_pitch_class(pitch)) & 1u)) {
        pick.forbidden++;
    }
    pick.forbidden += melody_dissonant_over_bass(context, pitch, r->out->notes[k].onset,
                                                 position_add(r->out->notes[k].onset, r->out->notes[k].duration));
    if (k > 0) {
        int leap = pitch - r->out->notes[k - 1].pitch;
        int own = context->motive_pitch[k] - context->motive_pitch[k - 1];
        pick.error += util_absolute(leap - own);
        pick.away = util_absolute(leap);
        pick.forbidden += parallel_perfects(r->out->notes[k - 1].pitch, pitch,
                                            bass_at(context, r->out->notes[k - 1].onset),
                                            bass_at(context, r->out->notes[k].onset));
    }
    if (k + 1 < r->count) {
        int leap = r->out->notes[k + 1].pitch - pitch;
        int own = context->motive_pitch[k + 1] - context->motive_pitch[k];
        pick.error += util_absolute(leap - own);
        pick.forbidden += parallel_perfects(pitch, r->out->notes[k + 1].pitch, bass_at(context, r->out->notes[k].onset),
                                            bass_at(context, r->out->notes[k + 1].onset));
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
            bool forced = r->force[k];
            if ((r->tone[k] && !forced) || !(loose[k] || strong_clash || forced)) {
                continue;
            }
            bool figure = !forced && keep_figures && pass <= r->count && !r->moved[k] && k < r->context->motive_tone_count &&
                          r->context->motive_tone[k] >= 0 && figure_near(r, k, &pitch);
            if (!figure && !nearest_tone(r, k, &pitch)) {
                return false;
            }
            r->tone[k] = !figure;
            r->force[k] = false;
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

/* The pitch classes note k may be moved to: its chords' tones, and the cadence's for the last note of an idea that ends in one. */
static unsigned moving_mask(const Realize *r, size_t k) {
    unsigned mask = r->mask[k];
    if (r->context->has_cadence && r->count == r->out->count && k + 1 == r->count) {
        mask &= melody_cadence_mask(r->context);
    }
    return mask;
}

/* The hard melodic rules broken by the notes from `from` up to note `upto`. */
static unsigned run_hard(const Realize *r, size_t from, size_t upto) {
    int run[MELODY_WINDOW_MAX];
    size_t used = 0;
    for (size_t k = from; k < upto; k++) {
        run[used++] = r->out->notes[k].pitch;
    }
    return melody_window_hard(r->context, run, used);
}

/* Whether the line holds the hard melodic rules and has no parallel octaves or fifths with the bass. */
static bool line_clean(const Realize *r) {
    for (size_t k = 1; k < r->count; k++) {
        if (parallel_perfects(r->out->notes[k - 1].pitch, r->out->notes[k].pitch,
                              bass_at(r->context, r->out->notes[k - 1].onset), bass_at(r->context, r->out->notes[k].onset))) {
            return false;
        }
    }
    return run_hard(r, 0, r->count) == 0;
}

/* The most notes a repair tries before it leaves the line as it is. */
#define REPAIR_BUDGET 400
#define REPAIR_REACH 7
#define REPAIR_OPTIONS 8

typedef struct {
    Realize *r;
    size_t movable[MOTIVE_MAX_NOTES];
    size_t count;
    unsigned budget;
} Repair;

/* Tries the pitches of the movable notes from the d-th on, nearest the target first, each held to the rules over the notes before it. */
static bool repair_from(Repair *s, size_t d) {
    Realize *r = s->r;
    if (d == s->count) {
        return line_clean(r);
    }
    size_t k = s->movable[d];
    int keep = r->out->notes[k].pitch;
    Pick options[REPAIR_OPTIONS];
    size_t found = 0;
    unsigned mask = moving_mask(r, k);
    for (int p = r->target[k] - REPAIR_REACH; p <= r->target[k] + REPAIR_REACH; p++) {
        if (p >= r->params->range_low && p <= r->params->range_high && ((mask >> util_pitch_class(p)) & 1u)) {
            Pick pick = measure(r, k, p);
            size_t at = found < REPAIR_OPTIONS ? found++ : REPAIR_OPTIONS - 1;
            if (at == REPAIR_OPTIONS - 1 && found == REPAIR_OPTIONS && !better(&pick, &options[at])) {
                continue;
            }
            while (at > 0 && better(&pick, &options[at - 1])) {
                options[at] = options[at - 1];
                at--;
            }
            options[at] = pick;
        }
    }
    for (size_t i = 0; i < found && s->budget > 0; i++) {
        size_t from = k > 3 ? k - 3 : 0;
        s->budget--;
        r->out->notes[k].pitch = options[i].pitch;
        if (run_hard(r, from, k + 1) == 0 &&
            (k == 0 || !parallel_perfects(r->out->notes[k - 1].pitch, options[i].pitch,
                                          bass_at(r->context, r->out->notes[k - 1].onset),
                                          bass_at(r->context, r->out->notes[k].onset))) &&
            repair_from(s, d + 1)) {
            return true;
        }
    }
    r->out->notes[k].pitch = keep;
    return false;
}

/*
 * When the line breaks a hard melodic rule, or runs in parallel fifths or octaves with the bass, after the notes are bent,
 * the notes that moved to chord tones are tried at the other tones near their targets (nearest first, depth first), each held
 * to the framework's rules over the notes before it, until the whole line holds them. When that finds none, every chord tone
 * may move the same way. A line that none makes right is left as it was.
 */
static void repair(Realize *r) {
    int saved[MOTIVE_MAX_NOTES];
    if (line_clean(r)) {
        return;
    }
    for (size_t k = 0; k < r->count; k++) {
        saved[k] = r->out->notes[k].pitch;
    }
    /* First the notes that moved; then, when the motive itself breaks a rule against these chords, any chord tone. */
    for (int all = 0; all < 2; all++) {
        Repair s;
        s.r = r;
        s.count = 0;
        s.budget = REPAIR_BUDGET;
        for (size_t k = 0; k < r->count; k++) {
            if (r->tone[k] && (all || r->moved[k])) {
                s.movable[s.count++] = k;
            }
        }
        if (s.count > 0 && repair_from(&s, 0)) {
            return;
        }
        for (size_t k = 0; k < r->count; k++) {
            r->out->notes[k].pitch = saved[k];
        }
    }
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
        r->force[k] = r->tone[k] && melody_dissonant_over_bass(context, note->pitch, note->onset,
                                                                position_add(note->onset, note->duration));
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
            ok = ok && settle(r, keep_figures);
        }
    }
    if (ok) {
        repair(r);
        ok = settle(r, keep_figures);
    }
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
