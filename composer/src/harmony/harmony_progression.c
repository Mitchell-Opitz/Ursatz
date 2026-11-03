#include "harmony/harmony_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmonic_function.h>
#include <standard-harmony/harmony_functions.h>

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
/* What may follow a predominant that opens the phrase as a pivot chord: ii after IV. */
static const PredominantRun AFTER_IV[] = {{1, {2}}};
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

/* Deals tried before a request is given up on, for a plagal cadence whose first tonic must land on a downbeat. */
#define DEAL_ATTEMPTS 16
/* More are tried when a harmony shape limits how the cells may be dealt, and when Given material limits the chords. */
#define SHAPED_DEAL_ATTEMPTS 96
#define GIVEN_DEAL_ATTEMPTS 480

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

bool harmony_shape_keeps_in(const PlanHarmonyShape *shape, const size_t *cells, size_t count, const PhraseMetre *metre,
                            bool exempt_last) {
    size_t start = 0;
    for (size_t i = 0; i < count; i++) {
        bool last = i + 1 == count;
        if (shape->max_cells_per_chord > 0 && cells[i] > (size_t)shape->max_cells_per_chord && !(last && exempt_last)) {
            return false;
        }
        /* A chord does not cross a barline: no cell after its first starts on one. */
        for (size_t cell = start + 1; shape->split_at_bars && cell < start + cells[i]; cell++) {
            if (harmony_metre_cell_on_downbeat(metre, cell)) {
                return false;
            }
        }
        start += cells[i];
    }
    return true;
}

/*
 * One progression for the request, or false when the phrase has no room for the
 * chords its cadence needs. Every chord before the last gets at least one cell;
 * the last gets the cells from the last downbeat the span holds (the whole last bar
 * of a span of bars), so the cadence arrives on a downbeat. The
 * cells left over are dealt out to the other chords at random. In a plagal
 * cadence the tonic that ends the authentic cadence also lands on a downbeat. A request that opens a new key starts
 * on a pivot chord, the degree given, in the place of the opening tonic; a predominant pivot is the phrase's
 * predominant (only ii may follow IV).
 */
static bool deal_progression(const PhraseRequest *request, FillRng *rng, int pivot, PhraseProgression *out) {
    bool half = request->cadence == CADENCE_TYPE_HALF;
    bool plagal = request->cadence == CADENCE_TYPE_PLAGAL;
    /* A cadential progression has no opening tonic. */
    bool opening_tonic = !(request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL);
    size_t cap = request->has_shape && request->shape.max_cells_per_chord > 0
                     ? (size_t)request->shape.max_cells_per_chord
                     : 0;
    size_t fixed = (opening_tonic ? 1 : 0) + (half ? 0 : 1) + (plagal ? 2 : 0);
    size_t last_cell = harmony_metre_last_cell(&request->metre);
    size_t last_cells = request->cells - last_cell;
    size_t room = last_cell;
    if (room < fixed) {
        return false;
    }

    ChordSpec opening = {pivot > 0 ? pivot : 1, false};
    PhraseChord probe;
    bool pivot_predominant = pivot > 0 && make_chord(request->mode, opening, 1, &probe) &&
                             probe.function == PLAN_FUNCTION_PREDOMINANT;
    const PredominantRun *runs = request->mode == PLAN_MODE_MINOR ? MINOR_PREDOMINANTS : MAJOR_PREDOMINANTS;
    size_t run_count = request->mode == PLAN_MODE_MINOR ? sizeof(MINOR_PREDOMINANTS) / sizeof(MINOR_PREDOMINANTS[0])
                                                        : sizeof(MAJOR_PREDOMINANTS) / sizeof(MAJOR_PREDOMINANTS[0]);
    if (pivot_predominant) {
        runs = AFTER_IV;
        run_count = pivot == 4 && request->mode == PLAN_MODE_MAJOR ? 1 : 0;
    }
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
        before[before_count++] = opening;
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
    ChordSpec last = half ? (ChordSpec){5, seventh} : (ChordSpec){request->cadence == CADENCE_TYPE_DECEPTIVE ? 6 : 1, false};

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
    /* The last chord runs from the last downbeat, whatever the cap, so the cadence arrives on a downbeat. */
    if (request->has_shape) {
        size_t all[MAX_CHORDS];
        for (size_t i = 0; i < before_count; i++) {
            all[i] = cells[i];
        }
        all[before_count] = last_cells;
        if (!harmony_shape_keeps_in(&request->shape, all, before_count + 1, &request->metre, true)) {
            return false;
        }
    }

    if (plagal) {
        size_t start = 0;
        for (size_t i = 0; i + 2 < before_count; i++) {
            start += cells[i];
        }
        if (!harmony_metre_cell_on_downbeat(&request->metre, start)) {
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
    if (!make_chord(request->mode, last, last_cells, &chords[before_count])) {
        free(chords);
        return false;
    }
    /* Chords that do not stand on the given bass or carry the given melody are dealt again. */
    if (request->has_given && !harmony_given_fits(request, chords, before_count + 1, NULL)) {
        free(chords);
        return false;
    }
    out->request = *request;
    out->chords = chords;
    out->count = before_count + 1;
    return true;
}

static bool build_progression(const PhraseRequest *request, FillRng *rng, int pivot, PhraseProgression *out) {
    size_t attempts = request->has_given ? GIVEN_DEAL_ATTEMPTS : (request->has_shape ? SHAPED_DEAL_ATTEMPTS : DEAL_ATTEMPTS);
    for (size_t attempt = 0; attempt < attempts; attempt++) {
        if (deal_progression(request, rng, pivot, out)) {
            return true;
        }
    }
    return false;
}

bool harmony_same_progression(const PhraseProgression *a, const PhraseProgression *b) {
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

void harmony_progression_label(const PhraseProgression *progression, char *label, size_t size) {
    size_t used = 0;
    label[0] = '\0';
    for (size_t i = 0; i < progression->count && used + 1 < size; i++) {
        size_t length = strlen(progression->chords[i].roman);
        if (used + length + 2 >= size) {
            break;
        }
        if (i) {
            label[used++] = ' ';
        }
        memcpy(label + used, progression->chords[i].roman, length);
        used += length;
        label[used] = '\0';
    }
}

bool harmony_phrase_propose(const PhraseRequest *request, FillRng *rng, FillCandidateList *out) {
    PhraseProgression *kept[CANDIDATE_ATTEMPTS];
    size_t kept_count = 0;
    for (size_t attempt = 0; attempt < CANDIDATE_ATTEMPTS; attempt++) {
        PhraseProgression progression;
        /* The first candidate opens on the best pivot, the others on the other pivots in turn. */
        int pivot = request->pivot ? request->pivot_degrees[attempt % request->pivot_count] : 0;
        if (!build_progression(request, rng, pivot, &progression)) {
            /* A pivot that leaves too few chords for the shape is passed over; without a pivot, none fits. */
            if (request->pivot) {
                continue;
            }
            break;
        }
        bool repeat = false;
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || harmony_same_progression(kept[i], &progression);
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
        harmony_progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            /* fill_candidates_add destroyed the payload. */
            return false;
        }
        kept[kept_count++] = payload;
    }
    return true;
}
