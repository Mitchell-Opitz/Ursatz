#include <stdlib.h>
#include <string.h>

#include <texture_rules.h>

#include "texture/texture_internal.h"

/* The widest a voicing of three notes spans, in semitones, and the narrowest gap between two of its notes. */
#define VOICING_SPAN 16
#define VOICING_GAP 2
/* How many semitones worse than the best a voicing may be and still be drawn for the first beat. */
#define FIRST_SLACK 4
#define VOICING_MAX 1024

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
    int pitch[3];
    long cost;
} Voicing;

/* Every voicing of the pitch classes in the window, low to high, with its cost; returns how many. */
static size_t voicings(const int pcs[3], int low, int high, int gap, const int *previous, int center, Voicing *out) {
    static const int ORDER[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    size_t count = 0;
    for (int order = 0; order < 6; order++) {
        int a = pcs[ORDER[order][0]];
        int b = pcs[ORDER[order][1]];
        int c = pcs[ORDER[order][2]];
        for (int p1 = low; p1 <= high; p1++) {
            if (p1 % 12 != a) {
                continue;
            }
            for (int p2 = p1 + gap; p2 <= high; p2++) {
                if (p2 % 12 != b) {
                    continue;
                }
                for (int p3 = p2 + gap; p3 <= high && p3 - p1 <= VOICING_SPAN; p3++) {
                    if (p3 % 12 != c || count >= VOICING_MAX) {
                        continue;
                    }
                    Voicing *voicing = &out[count++];
                    voicing->pitch[0] = p1;
                    voicing->pitch[1] = p2;
                    voicing->pitch[2] = p3;
                    if (previous) {
                        voicing->cost = labs((long)p1 - previous[0]) + labs((long)p2 - previous[1]) +
                                        labs((long)p3 - previous[2]);
                    } else {
                        voicing->cost = labs((long)(p1 + p2 + p3) - 3L * center) + (p3 - p1);
                    }
                }
            }
        }
    }
    return count;
}

/*
 * The voicing of a chord's three notes in the window: the one nearest the previous voicing (or, for the first, the
 * middle of the window), a tie settled by rng. slack lets a voicing that much worse than the best be drawn instead.
 */
static bool choose_voicing(const int pcs[3], int low, int high, const int *previous, long slack, FillRng *rng,
                           int out[3]) {
    Voicing *all = malloc(VOICING_MAX * sizeof(*all));
    size_t count = 0;
    if (!all) {
        return false;
    }
    for (int gap = VOICING_GAP; count == 0 && gap >= 1; gap--) {
        count = voicings(pcs, low, high, gap, previous, (low + high) / 2, all);
    }
    if (count == 0) {
        free(all);
        return false;
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
            break;
        }
    }
    free(all);
    return true;
}

/* The three pitch classes a chord is voiced in: its root, third and fifth, or, with a seventh, without the fifth. */
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
        if (!choose_voicing(pcs, low, high, previous, previous ? 0 : FIRST_SLACK, rng, out->voicing[b])) {
            return false;
        }
        out->voiced[b] = true;
        previous = out->voicing[b];
    }

    switch (pattern) {
    case PLAN_ACCOMPANIMENT_BLOCK:
    case PLAN_ACCOMPANIMENT_OOM_PAH:
        for (size_t b = 0; b < out->beat_count; b++) {
            for (size_t voice = 0; out->voiced[b] && voice < 3; voice++) {
                /* Voice 1 is the top of the chord. */
                if (!add_note(out, voice, (long)b * context->beat, context->beat, out->voicing[b][2 - voice])) {
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
            if (!add_note(out, 0, at, ACCOMP_EIGHTH, out->voicing[b][figure[eighth % 4]])) {
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
