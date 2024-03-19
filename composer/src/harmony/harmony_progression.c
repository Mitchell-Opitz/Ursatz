#include "harmony/harmony_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <harmonic_function.h>
#include <harmony_functions.h>

/* Candidates tried per call; ones that repeat an earlier one are dropped. */
#define CANDIDATE_ATTEMPTS 8
/* Tb, two PD, D, and the Te and IV of a plagal cadence. */
#define MAX_BEFORE_FINAL 6
#define MAX_CHORDS (MAX_BEFORE_FINAL + 1)

typedef struct {
    int degree;
    bool seventh;
} ChordSpec;

/* The predominants that may stand before V, in the order they are played. IV goes before ii, never after. */
typedef struct {
    size_t count;
    int degrees[2];
} PredominantRun;

static const PredominantRun MAJOR_PREDOMINANTS[] = {{1, {4}}, {1, {2}}, {2, {4, 2}}};
/* The root-position ii in a minor key is a diminished chord that standard-harmony prefers in first inversion. */
static const PredominantRun MINOR_PREDOMINANTS[] = {{1, {4}}};

static PlanHarmonicFunction plan_function_of(HarmonicFunction function) {
    switch (function) {
    case HARMONIC_FUNCTION_TONIC:
        return PLAN_FUNCTION_TONIC;
    case HARMONIC_FUNCTION_PREDOMINANT:
        return PLAN_FUNCTION_PREDOMINANT;
    case HARMONIC_FUNCTION_DOMINANT:
        return PLAN_FUNCTION_DOMINANT;
    default:
        return PLAN_FUNCTION_UNKNOWN;
    }
}

static bool make_from(PlanKeyMode mode, HarmonyChord chord, size_t cells, PhraseChord *out) {
    HarmonicFunction function;
    memset(out, 0, sizeof(*out));
    if (!harmony_chord_valid(&chord) || !harmony_function_of(harmony_mode_of(mode), &chord, &function) ||
        !harmony_roman_format(mode, &chord, out->roman)) {
        return false;
    }
    out->function = plan_function_of(function);
    out->chord = chord;
    out->cells = cells;
    return true;
}

bool harmony_make_chord(PlanKeyMode mode, int degree, int inversion, bool seventh, bool cadential, size_t cells,
                        PhraseChord *out) {
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = degree;
    chord.inversion = inversion;
    chord.seventh = seventh;
    chord.cadential = cadential;
    return make_from(mode, chord, cells, out);
}

bool harmony_make_leading_tone(PlanKeyMode mode, int inversion, size_t cells, PhraseChord *out) {
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = 7;
    chord.inversion = inversion;
    chord.leading_tone = mode == PLAN_MODE_MINOR;
    return make_from(mode, chord, cells, out);
}

static bool make_chord(PlanKeyMode mode, ChordSpec spec, size_t cells, PhraseChord *out) {
    return harmony_make_chord(mode, spec.degree, 0, spec.seventh, false, cells, out);
}

void harmony_progression_destroy(void *payload) {
    PhraseProgression *progression = payload;
    if (progression) {
        free(progression->chords);
        free(progression);
    }
}

size_t harmony_phrase_min_cells(PlanCadenceType cadence, size_t cells_per_bar) {
    /* A tonic, the dominant unless the phrase ends on it, and for a plagal cadence the tonic and IV after it. */
    size_t before_last = 1 + (cadence == PLAN_CADENCE_HC ? 0 : 1) + (cadence == PLAN_CADENCE_PC ? 2 : 0);
    return before_last + cells_per_bar;
}

/* Deals tried before a request is given up on, for a plagal cadence whose first tonic must land on a downbeat. */
#define DEAL_ATTEMPTS 16
/* More are tried when a harmony shape limits how the cells may be dealt. */
#define SHAPED_DEAL_ATTEMPTS 96

bool harmony_shape_keeps(const PlanHarmonyShape *shape, const size_t *cells, size_t count, size_t cells_per_bar,
                         bool exempt_last) {
    size_t start = 0;
    for (size_t i = 0; i < count; i++) {
        bool last = i + 1 == count;
        if (shape->max_cells_per_chord > 0 && cells[i] > (size_t)shape->max_cells_per_chord && !(last && exempt_last)) {
            return false;
        }
        if (shape->split_at_bars && start % cells_per_bar + cells[i] > cells_per_bar) {
            return false;
        }
        start += cells[i];
    }
    return true;
}

/*
 * One progression for the request, or false when the phrase has no room for the
 * chords its cadence needs. Every chord before the last gets at least one cell;
 * the last gets the whole last bar, so the cadence arrives on a downbeat. The
 * cells left over are dealt out to the other chords at random. In a plagal
 * cadence the tonic that ends the authentic cadence also lands on a downbeat.
 */
static bool deal_progression(const PhraseRequest *request, FillRng *rng, PhraseProgression *out) {
    bool half = request->cadence == PLAN_CADENCE_HC;
    bool plagal = request->cadence == PLAN_CADENCE_PC;
    /* A cadential progression has no opening tonic. */
    bool opening_tonic = !(request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL);
    size_t cap = request->has_shape && request->shape.max_cells_per_chord > 0
                     ? (size_t)request->shape.max_cells_per_chord
                     : 0;
    size_t fixed = (opening_tonic ? 1 : 0) + (half ? 0 : 1) + (plagal ? 2 : 0);
    size_t room = request->cells - request->cells_per_bar;
    if (room < fixed) {
        return false;
    }

    const PredominantRun *runs = request->mode == PLAN_MODE_MINOR ? MINOR_PREDOMINANTS : MAJOR_PREDOMINANTS;
    size_t run_count = request->mode == PLAN_MODE_MINOR ? sizeof(MINOR_PREDOMINANTS) / sizeof(MINOR_PREDOMINANTS[0])
                                                        : sizeof(MAJOR_PREDOMINANTS) / sizeof(MAJOR_PREDOMINANTS[0]);
    /* A phrase usually has a predominant; it goes only when there is no room for one. */
    size_t fitting[3];
    size_t fitting_count = 0;
    for (size_t i = 0; i < run_count; i++) {
        if (fixed + runs[i].count <= room) {
            fitting[fitting_count++] = i;
        }
    }
    const PredominantRun *run = fitting_count ? &runs[fitting[fill_rng_below(rng, fitting_count)]] : NULL;
    bool seventh = fill_rng_below(rng, 2) == 1;

    ChordSpec before[MAX_BEFORE_FINAL];
    size_t before_count = 0;
    if (opening_tonic) {
        before[before_count++] = (ChordSpec){1, false};
    }
    for (size_t i = 0; run && i < run->count; i++) {
        before[before_count++] = (ChordSpec){run->degrees[i], false};
    }
    if (!half) {
        before[before_count++] = (ChordSpec){5, seventh};
    }
    if (plagal) {
        before[before_count++] = (ChordSpec){1, false};
        before[before_count++] = (ChordSpec){4, false};
    }
    ChordSpec last = half ? (ChordSpec){5, seventh} : (ChordSpec){request->cadence == PLAN_CADENCE_DC ? 6 : 1, false};

    size_t cells[MAX_BEFORE_FINAL];
    for (size_t i = 0; i < before_count; i++) {
        cells[i] = 1;
    }
    for (size_t left = room - before_count; left > 0; left--) {
        /* A chord at its cap takes no more cells; the cells go to one that can. */
        size_t open[MAX_BEFORE_FINAL];
        size_t open_count = 0;
        for (size_t i = 0; i < before_count; i++) {
            if (cap == 0 || cells[i] < cap) {
                open[open_count++] = i;
            }
        }
        if (open_count == 0) {
            return false;
        }
        cells[open[fill_rng_below(rng, open_count)]]++;
    }
    /* The last chord is the whole last bar, whatever the cap, so the cadence arrives on a downbeat. */
    if (request->has_shape) {
        size_t all[MAX_CHORDS];
        for (size_t i = 0; i < before_count; i++) {
            all[i] = cells[i];
        }
        all[before_count] = request->cells_per_bar;
        if (!harmony_shape_keeps(&request->shape, all, before_count + 1, request->cells_per_bar, true)) {
            return false;
        }
    }

    if (plagal) {
        size_t start = 0;
        for (size_t i = 0; i + 2 < before_count; i++) {
            start += cells[i];
        }
        if (start % request->cells_per_bar != 0) {
            return false;
        }
    }

    PhraseChord *chords = calloc(MAX_CHORDS, sizeof(PhraseChord));
    if (!chords) {
        return false;
    }
    for (size_t i = 0; i < before_count; i++) {
        if (!make_chord(request->mode, before[i], cells[i], &chords[i])) {
            free(chords);
            return false;
        }
    }
    if (!make_chord(request->mode, last, request->cells_per_bar, &chords[before_count])) {
        free(chords);
        return false;
    }
    out->request = *request;
    out->chords = chords;
    out->count = before_count + 1;
    return true;
}

static bool build_progression(const PhraseRequest *request, FillRng *rng, PhraseProgression *out) {
    size_t attempts = request->has_shape ? SHAPED_DEAL_ATTEMPTS : DEAL_ATTEMPTS;
    for (size_t attempt = 0; attempt < attempts; attempt++) {
        if (deal_progression(request, rng, out)) {
            return true;
        }
    }
    return false;
}

static bool same_progression(const PhraseProgression *a, const PhraseProgression *b) {
    if (a->count != b->count) {
        return false;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (a->chords[i].cells != b->chords[i].cells || strcmp(a->chords[i].roman, b->chords[i].roman) != 0) {
            return false;
        }
    }
    return true;
}

static void progression_label(const PhraseProgression *progression, char *label, size_t size) {
    size_t used = 0;
    label[0] = '\0';
    for (size_t i = 0; i < progression->count && used + 1 < size; i++) {
        int written = snprintf(label + used, size - used, i ? " %s" : "%s", progression->chords[i].roman);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }
}

bool harmony_phrase_propose(const PhraseRequest *request, FillRng *rng, FillCandidateList *out) {
    PhraseProgression *kept[CANDIDATE_ATTEMPTS];
    size_t kept_count = 0;
    for (size_t attempt = 0; attempt < CANDIDATE_ATTEMPTS; attempt++) {
        PhraseProgression progression;
        if (!build_progression(request, rng, &progression)) {
            break;
        }
        bool repeat = false;
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_progression(kept[i], &progression);
        }
        if (repeat) {
            free(progression.chords);
            continue;
        }
        PhraseProgression *payload = malloc(sizeof(*payload));
        if (!payload) {
            free(progression.chords);
            break;
        }
        *payload = progression;
        char label[FILL_LABEL_MAX];
        progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            /* fill_candidates_add destroyed the payload. */
            return false;
        }
        kept[kept_count++] = payload;
    }
    return true;
}
