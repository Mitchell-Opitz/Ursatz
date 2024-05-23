#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <texture_rules.h>

#include "texture/texture_internal.h"

/* The widest a voicing of three notes spans, in semitones, and the narrowest gap between two of its notes. */
#define VOICING_SPAN 16
#define VOICING_GAP 2
/* How many semitones worse than the best a voicing may be and still be drawn for the first beat. */
#define FIRST_SLACK 4
#define VOICING_MAX 4096

/* The order of the notes of a figure, as indexes into a voicing from low to high, by the eighth of the bar mod 4. */
static const int ARPEGGIO[4] = {0, 1, 2, 1};
static const int ALBERTI[4] = {0, 2, 1, 2};

const char *accomp_pattern_name(PlanAccompanimentPattern pattern) {
    static const char *const NAMES[] = {"any", "block", "arpeggiated", "alberti", "oom-pah"};
    return pattern >= PLAN_ACCOMPANIMENT_ANY && pattern <= PLAN_ACCOMPANIMENT_OOM_PAH ? NAMES[pattern] : "?";
}

size_t accomp_pattern_voices(PlanAccompanimentPattern pattern) {
    return pattern == PLAN_ACCOMPANIMENT_BLOCK || pattern == PLAN_ACCOMPANIMENT_OOM_PAH ? 3 : 1;
}

bool accomp_pattern_allowed(const AccompContext *context, PlanAccompanimentPattern pattern) {
    if (pattern == PLAN_ACCOMPANIMENT_ALBERTI && context->compound) {
        return false;
    }
    if (context->pattern != PLAN_ACCOMPANIMENT_ANY) {
        return pattern == context->pattern;
    }
    if (context->texture >= 0) {
        StandardTexture texture = (StandardTexture)context->texture;
        if (!standard_texture_is_homophonic(texture)) {
            return false;
        }
        if (texture == STANDARD_TEXTURE_HOMORHYTHM) {
            return pattern == PLAN_ACCOMPANIMENT_BLOCK;
        }
    }
    return pattern >= PLAN_ACCOMPANIMENT_BLOCK && pattern <= PLAN_ACCOMPANIMENT_OOM_PAH;
}

/* ------------------------------------------------------------- voicing */

typedef struct {
    int pitch[3]; /* low to high */
    size_t size;  /* 2 or 3 */
    long cost;
} Voicing;

/* What a voicing is penalised for in semitones, each: a chord tone of the three the chord is made of that is left out. */
#define MISSING_PENALTY 12

/* The distance in semitones between two voicings, voice by voice from the top. */
long accomp_voicing_distance(const int *a, size_t a_size, const int *b, size_t b_size) {
    long distance = 0;
    for (size_t k = 0; k < (a_size < b_size ? a_size : b_size); k++) {
        distance += labs((long)a[a_size - 1 - k] - b[b_size - 1 - k]);
    }
    return distance;
}

/* Adds the voicing p of `size` notes to out when every note is a chord tone and, of two or more, one is the third. */
static size_t consider(const int p[3], size_t size, const int pcs[3], const int *previous, size_t previous_size,
                       int center, Voicing *out, size_t count) {
    unsigned used = 0;
    for (size_t k = 0; k < size; k++) {
        bool tone = false;
        for (int c = 0; c < 3; c++) {
            if (p[k] % 12 == pcs[c]) {
                tone = true;
                used |= 1u << c;
            }
        }
        if (!tone) {
            return count;
        }
    }
    if ((size >= 2 && !(used & 2u)) || count >= VOICING_MAX) {
        return count;
    }
    Voicing *voicing = &out[count];
    memset(voicing->pitch, 0, sizeof(voicing->pitch));
    memcpy(voicing->pitch, p, size * sizeof(int));
    voicing->size = size;
    int missing = 3 - (int)((used & 1u) + ((used >> 1) & 1u) + ((used >> 2) & 1u));
    voicing->cost = (long)missing * MISSING_PENALTY;
    if (previous) {
        voicing->cost += accomp_voicing_distance(voicing->pitch, size, previous, previous_size);
    } else {
        long sum = 0;
        for (size_t k = 0; k < size; k++) {
            sum += p[k];
        }
        voicing->cost += labs(sum - (long)size * center) + (p[size - 1] - p[0]);
    }
    return count + 1;
}

/*
 * Every voicing of `size` chord tones in the window, low to high, with its cost. Of two or more, the third is always
 * in it (it gives the chord its quality); the root may be left out, since the bass sounds it, and a tone may be
 * doubled, but each tone left out of the root, third and fifth (the seventh in place of the fifth) costs
 * MISSING_PENALTY, so a voicing of all three is chosen unless the window leaves no room for one. Returns how many.
 */
static size_t voicings(const int pcs[3], size_t size, int low, int high, int gap, const int *previous,
                       size_t previous_size, int center, Voicing *out) {
    size_t count = 0;
    for (int p1 = low; p1 <= high; p1++) {
        if (size == 1) {
            int p[3] = {p1, 0, 0};
            count = consider(p, size, pcs, previous, previous_size, center, out, count);
            continue;
        }
        for (int p2 = p1 + gap; p2 <= high; p2++) {
            if (size == 2) {
                int p[3] = {p1, p2, 0};
                count = consider(p, size, pcs, previous, previous_size, center, out, count);
                continue;
            }
            for (int p3 = p2 + gap; p3 <= high && p3 - p1 <= VOICING_SPAN; p3++) {
                int p[3] = {p1, p2, p3};
                count = consider(p, size, pcs, previous, previous_size, center, out, count);
            }
        }
    }
    return count;
}

/*
 * The voicing of a chord in the window: three notes, or two or one when the window, which the bass and the melody
 * close in, has no room for more, or none (*out_size 0) when it has no chord tone at all: the accompaniment is silent
 * for the beat. The voicing is the one nearest the previous voicing (or, for the first, the middle of the window), a
 * tie settled by rng. slack lets a voicing that much worse than the best be drawn instead.
 */
static bool choose_voicing(const int pcs[3], int low, int high, const int *previous, size_t previous_size, long slack,
                           FillRng *rng, int out[3], size_t *out_size) {
    Voicing *all = malloc(VOICING_MAX * sizeof(*all));
    size_t count = 0;
    size_t size = 3;
    if (!all) {
        return false;
    }
    *out_size = 0;
    for (; count == 0 && size >= 1; size--) {
        for (int gap = VOICING_GAP; count == 0 && gap >= 1; gap--) {
            count = voicings(pcs, size, low, high, gap, previous, previous_size, (low + high) / 2, all);
            if (size == 1) {
                break;
            }
        }
    }
    if (count == 0) {
        free(all);
        return true;
    }
    long best = all[0].cost;
    for (size_t i = 1; i < count; i++) {
        best = all[i].cost < best ? all[i].cost : best;
    }
    size_t near = 0;
    for (size_t i = 0; i < count; i++) {
        near += all[i].cost <= best + slack;
    }
    size_t pick = (size_t)fill_rng_below(rng, near);
    for (size_t i = 0; i < count; i++) {
        if (all[i].cost <= best + slack && pick-- == 0) {
            memcpy(out, all[i].pitch, sizeof(all[i].pitch));
            *out_size = all[i].size;
            break;
        }
    }
    free(all);
    return true;
}

/* The three pitch classes a chord is voiced in: its root, third and fifth, or, with a seventh, the seventh for the fifth. */
static void voiced_pcs(const AccompContext *context, size_t chord, int out[3]) {
    const int *pcs = context->chord_pcs[chord];
    out[0] = pcs[0];
    out[1] = pcs[1];
    out[2] = context->chord_pc_count[chord] > 3 ? pcs[3] : pcs[2];
}

/* The window the notes of the beat from start to start + beat have to keep to: above the bass, below the melody. */
static void beat_window(const AccompContext *context, long start, int *low, int *high) {
    long end = start + context->beat;
    *low = context->low;
    *high = context->high;
    for (size_t i = 0; i < context->bass_count; i++) {
        const AccompLine *line = &context->bass[i];
        if (line->onset < end && line->end > start && line->pitch + 1 > *low) {
            *low = line->pitch + 1;
        }
    }
    for (size_t i = 0; i < context->melody_count; i++) {
        const AccompLine *line = &context->melody[i];
        if (line->onset < end && line->end > start && line->pitch - 1 < *high) {
            *high = line->pitch - 1;
        }
    }
}

/* Whether the beat is one an oom-pah rests on: the strong beats, the first of a bar and the third of four. */
static bool rests_on(const AccompContext *context, long start) {
    long beats_in_bar = context->bar / context->beat;
    long index = ((start + context->phase) % context->bar) / context->beat;
    return beats_in_bar % 2 == 0 ? index % 2 == 0 : index == 0;
}

static bool add_note(AccompCandidate *candidate, size_t voice, long onset, long duration, int pitch) {
    if (candidate->note_count[voice] >= ACCOMP_MAX_NOTES) {
        return false;
    }
    AccompNote *note = &candidate->notes[voice][candidate->note_count[voice]++];
    note->onset = onset;
    note->duration = duration;
    note->pitch = pitch;
    return true;
}

bool accomp_build(const AccompContext *context, PlanAccompanimentPattern pattern, FillRng *rng, AccompCandidate *out) {
    memset(out, 0, sizeof(*out));
    out->magic = ACCOMP_PAYLOAD_MAGIC;
    out->pattern = pattern;
    out->voices = accomp_pattern_voices(pattern);
    if (!accomp_pattern_allowed(context, pattern) || context->beat <= 0 || context->length % context->beat != 0 ||
        context->length / context->beat > ACCOMP_MAX_BEATS) {
        return false;
    }
    out->beat_count = (size_t)(context->length / context->beat);

    /* The chord on each beat, in the position nearest the one before. */
    const int *previous = NULL;
    size_t previous_size = 0;
    for (size_t b = 0; b < out->beat_count; b++) {
        long start = (long)b * context->beat;
        int pcs[3];
        int low;
        int high;
        if (pattern == PLAN_ACCOMPANIMENT_OOM_PAH && rests_on(context, start)) {
            continue;
        }
        voiced_pcs(context, accomp_chord_at(context, start), pcs);
        beat_window(context, start, &low, &high);
        if (!choose_voicing(pcs, low, high, previous, previous_size, previous ? 0 : FIRST_SLACK, rng, out->voicing[b],
                            &out->voicing_size[b])) {
            return false;
        }
        if (out->voicing_size[b] == 0) {
            continue;
        }
        out->voiced[b] = true;
        previous = out->voicing[b];
        previous_size = out->voicing_size[b];
    }

    switch (pattern) {
    case PLAN_ACCOMPANIMENT_BLOCK:
    case PLAN_ACCOMPANIMENT_OOM_PAH:
        for (size_t b = 0; b < out->beat_count; b++) {
            for (size_t voice = 0; out->voiced[b] && voice < out->voicing_size[b]; voice++) {
                /* Voice 1 is the top of the chord. */
                if (!add_note(out, voice, (long)b * context->beat, context->beat,
                              out->voicing[b][out->voicing_size[b] - 1 - voice])) {
                    return false;
                }
            }
        }
        return true;
    case PLAN_ACCOMPANIMENT_ARPEGGIATED:
    case PLAN_ACCOMPANIMENT_ALBERTI: {
        const int *figure = pattern == PLAN_ACCOMPANIMENT_ARPEGGIATED ? ARPEGGIO : ALBERTI;
        for (long at = 0; at < context->length; at += ACCOMP_EIGHTH) {
            long eighth = ((at + context->phase) % context->bar) / ACCOMP_EIGHTH;
            size_t b = (size_t)(at / context->beat);
            /* A voicing of two notes is the low one and the high one over again, one of a single note. */
            size_t size = out->voicing_size[b];
            int index = size == 3 ? figure[eighth % 4] : (size == 2 ? (int)(eighth % 2) : 0);
            if (!out->voiced[b]) {
                continue;
            }
            if (!add_note(out, 0, at, ACCOMP_EIGHTH, out->voicing[b][index])) {
                return false;
            }
        }
        return true;
    }
    case PLAN_ACCOMPANIMENT_ANY:
        break;
    }
    return false;
}
