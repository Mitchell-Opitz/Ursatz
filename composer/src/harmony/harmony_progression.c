#include "harmony/harmony_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/harmony/harmonic_function.h>
#include <theory/harmony/harmony_functions.h>

/* Candidates tried per call; ones that repeat an earlier one are dropped. */
#define CANDIDATE_ATTEMPTS 8
/* Tb (or a pivot and the new key's V and I), two PD, D, and the Te and IV of a plagal cadence. */
#define MAX_BEFORE_FINAL 8
#define MAX_CHORDS (MAX_BEFORE_FINAL + 1)
/* The most chords a deal holds: a dominant opened on the cadential six-four adds one. */
#define MAX_DEALT (MAX_CHORDS + 1)

typedef struct {
    int degree;
    bool seventh;
    const PitchMaterial *borrowed; /* the material a borrowed chord is taken from (R-729), NULL for a diatonic chord */
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

static bool make_from(const HarmonyKind *kind, HarmonyChord chord, size_t cells, PhraseChord *out) {
    HarmonicFunction function;
    memset(out, 0, sizeof(*out));
    /* A modal key has no leading tone and keeps out the chord its mode avoids (R-725); a borrowed chord keeps out the one its source
       mode avoids. */
    const PitchMaterial *avoiding = chord.borrowed_from != NULL ? chord.borrowed_from : kind->modal;
    if ((avoiding != NULL && pitch_material_avoids(avoiding, chord.degree)) || (kind->modal != NULL && chord.leading_tone)) {
        return false;
    }
    /* A borrowed chord has the function of the key's own chord on its degree: it stands in its place (R-729). */
    HarmonyChord functional = chord;
    functional.borrowed_from = NULL;
    /* A modal key without its cadence kind at hand (a Motive holds no snapshot) has the chord without a function. */
    bool has_function = harmony_function_in(kind, &functional, &function);
    if (!harmony_chord_valid(&chord) || (!has_function && !(kind->modal != NULL && kind->cadence == NULL)) ||
        !harmony_roman_format_in(kind, &chord, out->roman)) {
        return false;
    }
    out->function = has_function ? function : HARMONIC_FUNCTION_TONIC;
    out->has_function = has_function;
    out->chord = chord;
    out->cells = cells;
    return true;
}

bool harmony_make_chord_in(const HarmonyKind *kind, int degree, int inversion, bool seventh, bool cadential, size_t cells,
                           PhraseChord *out) {
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = degree;
    chord.inversion = inversion;
    chord.seventh = seventh;
    chord.cadential = cadential;
    return make_from(kind, chord, cells, out);
}

bool harmony_make_chord(HarmonyMode mode, int degree, int inversion, bool seventh, bool cadential, size_t cells,
                        PhraseChord *out) {
    HarmonyKind kind = {mode, NULL, NULL, NULL};
    return harmony_make_chord_in(&kind, degree, inversion, seventh, cadential, cells, out);
}

bool harmony_make_leading_tone(HarmonyMode mode, int inversion, size_t cells, PhraseChord *out) {
    HarmonyChord chord;
    HarmonyKind kind = {mode, NULL, NULL, NULL};
    memset(&chord, 0, sizeof(chord));
    chord.degree = 7;
    chord.inversion = inversion;
    chord.leading_tone = mode == HARMONY_MODE_MINOR;
    return make_from(&kind, chord, cells, out);
}

static bool make_chord(const HarmonyKind *kind, ChordSpec spec, size_t cells, PhraseChord *out) {
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = spec.degree;
    chord.seventh = spec.seventh;
    chord.borrowed_from = spec.borrowed;
    return make_from(kind, chord, cells, out);
}

/*
 * Whether the triad on `degree` of `source` can stand in for the key's own chord there (R-729): its triad differs from the key's, it is
 * major, minor or diminished, the source does not avoid it, and its numeral is writable (a root within a semitone of the key's step).
 */
static bool borrowable(const HarmonyKind *kind, const PitchMaterial *source, int degree) {
    HarmonyChord plain;
    HarmonyChord borrowed;
    int plain_root;
    int borrowed_root;
    int plain_intervals[4];
    int borrowed_intervals[4];
    size_t plain_count;
    size_t borrowed_count;
    PhraseChord probe;
    memset(&plain, 0, sizeof(plain));
    plain.degree = degree;
    borrowed = plain;
    borrowed.borrowed_from = source;
    return !pitch_material_avoids(source, degree) &&
           composer_chord_intervals(kind->chords, kind->mode, kind->modal, &plain, &plain_root, plain_intervals, &plain_count) &&
           composer_chord_intervals(kind->chords, kind->mode, kind->modal, &borrowed, &borrowed_root, borrowed_intervals, &borrowed_count) &&
           (plain_root != borrowed_root || memcmp(plain_intervals, borrowed_intervals, 3 * sizeof(int)) != 0) &&
           make_chord(kind, (ChordSpec){degree, false, source}, 1, &probe);
}

/*
 * Mixture (R-729): with share above 0, each predominant chord of the run (before[first] to before[first + count - 1]) draws once, and
 * under the share becomes the triad on its degree of the first material of `from` that can stand there; a later predominant of the run
 * takes the material the run already borrows from when it can. The tonic and the approach chords are never borrowed. With share 0
 * nothing is drawn.
 */
static void borrow_predominants(const HarmonyKind *kind, FillRng *rng, ChordSpec *before, size_t first, size_t count) {
    const ComposerMixture *mixture = kind->chords != NULL ? &kind->chords->mixture : NULL;
    if (mixture == NULL || mixture->share.num <= 0 || mixture->share.den <= 0 || count == 0) {
        return;
    }
    const char *words[COMPOSER_MIXTURE_FROM_MAX];
    const PitchMaterial *sources[COMPOSER_MIXTURE_MAX];
    for (size_t i = 0; i < mixture->from_count; i++) {
        words[i] = mixture->from[i];
    }
    size_t source_count = composer_chords_mixture_materials(kind->chords, composer_chords_key_material(kind->chords, kind->mode, kind->modal),
                                                            words, mixture->from_count, true, sources);
    const PitchMaterial *held = NULL;
    for (size_t i = first; i < first + count; i++) {
        if ((long long)fill_rng_below(rng, (size_t)mixture->share.den) >= mixture->share.num) {
            continue;
        }
        const PitchMaterial *pick = held != NULL && borrowable(kind, held, before[i].degree) ? held : NULL;
        for (size_t m = 0; pick == NULL && m < source_count; m++) {
            pick = borrowable(kind, sources[m], before[i].degree) ? sources[m] : NULL;
        }
        if (pick != NULL) {
            before[i].borrowed = pick;
            held = pick;
        }
    }
}

void harmony_progression_destroy(void *payload) {
    PhraseProgression *progression = payload;
    if (progression) {
        free(progression->chords);
        free(progression);
    }
}

/*
 * A dealt V whose first downbeat the Given melody does not hold as a tone of V opens on the cadential six-four, which
 * carries the tonic there, and V keeps the rest of its cells. False when the cadence has no dominant to open, the V has
 * a single cell, or the chords still do not fit.
 */
static bool open_on_cadential_six_four(const PhraseRequest *request, PhraseChord *chords, size_t *count, size_t dominant) {
    PhraseChord six_four;
    if (request->modal != NULL || request->cadence == CADENCE_TYPE_HALF || chords[dominant].cells < 2 || *count >= MAX_DEALT ||
        !harmony_make_chord(request->mode, 1, 2, false, true, 1, &six_four)) {
        return false;
    }
    memmove(&chords[dominant + 1], &chords[dominant], (*count - dominant) * sizeof(PhraseChord));
    chords[dominant] = six_four;
    chords[dominant + 1].cells--;
    (*count)++;
    return harmony_given_fits(request, chords, *count, NULL);
}

/* Deals tried before a request is given up on, for a plagal cadence whose first tonic must land on a downbeat. */
#define DEAL_ATTEMPTS 16
/* More are tried when a harmony shape limits how the cells may be dealt, and when Given material limits the chords. */
#define SHAPED_DEAL_ATTEMPTS 96
#define GIVEN_DEAL_ATTEMPTS 480

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
 * predominant (only ii may follow IV). With `confirm`, a predominant pivot goes on to the new key's V7 and I, which
 * confirm the key, and the phrase's predominant follows the I as after an opening tonic.
 */
static bool deal_progression(const PhraseRequest *request, FillRng *rng, int pivot, bool confirm, PhraseProgression *out) {
    bool half = request->cadence == CADENCE_TYPE_HALF;
    bool plagal = request->cadence == CADENCE_TYPE_PLAGAL;
    /* A cadential progression has no opening tonic. */
    bool opening_tonic = !(request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL);
    size_t cap = request->has_shape && request->shape.max_cells_per_chord > 0
                     ? (size_t)request->shape.max_cells_per_chord
                     : 0;
    HarmonyKind kind = harmony_kind_of(request->composer_chords, request->mode, request->modal);
    if (kind.modal != NULL && (kind.cadence == NULL || request->cadence == CADENCE_TYPE_DECEPTIVE || plagal)) {
        return false;
    }
    /* The dominant chords before the last: V unless the phrase ends on it; a modal cadence's approach chords (all of them, or
       all but the last for a half cadence, which ends on it). */
    size_t approach = kind.modal != NULL ? kind.cadence->approach_count : 1;
    size_t dominants = half ? approach - 1 : approach;
    size_t fixed = (opening_tonic ? 1 : 0) + (confirm ? 2 : 0) + dominants + (plagal ? 2 : 0);
    size_t last_cell = harmony_metre_last_cell(&request->metre);
    size_t last_cells = request->cells - last_cell;
    size_t room = last_cell;
    if (room < fixed) {
        return false;
    }

    ChordSpec opening = {pivot > 0 ? pivot : 1, false, NULL};
    PhraseChord probe;
    bool pivot_predominant = pivot > 0 && make_chord(&kind, opening, 1, &probe) &&
                             probe.has_function && probe.function == HARMONIC_FUNCTION_PREDOMINANT;
    const PredominantRun *runs = request->mode == HARMONY_MODE_MINOR ? MINOR_PREDOMINANTS : MAJOR_PREDOMINANTS;
    size_t run_count = request->mode == HARMONY_MODE_MINOR ? sizeof(MINOR_PREDOMINANTS) / sizeof(MINOR_PREDOMINANTS[0])
                                                        : sizeof(MAJOR_PREDOMINANTS) / sizeof(MAJOR_PREDOMINANTS[0]);
    if (confirm && !pivot_predominant) {
        return false;
    }
    if (pivot_predominant && !confirm) {
        runs = AFTER_IV;
        run_count = pivot == 4 && request->mode == HARMONY_MODE_MAJOR ? 1 : 0;
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
    /* A modal key's predominant is one chord drawn from the mode's primary chords that are neither the tonic nor an approach chord
       (none when none remains); it has no seventh chords. */
    PredominantRun modal_run = {0, {0, 0}};
    if (kind.modal != NULL) {
        int pool[7];
        size_t pool_count = 0;
        seventh = false;
        for (size_t i = 0; i < kind.modal->primary_count; i++) {
            int degree = kind.modal->primary_degrees[i];
            bool is_approach = false;
            for (size_t a = 0; a < kind.cadence->approach_count; a++) {
                is_approach = is_approach || kind.cadence->approach_degrees[a] == degree;
            }
            if (degree != 1 && !is_approach) {
                pool[pool_count++] = degree;
            }
        }
        run = NULL;
        if (pool_count > 0 && fixed + 1 <= room) {
            modal_run.count = 1;
            modal_run.degrees[0] = pool[fill_rng_below(rng, pool_count)];
            run = &modal_run;
        }
    }

    ChordSpec before[MAX_BEFORE_FINAL];
    size_t before_count = 0;
    if (opening_tonic) {
        before[before_count++] = opening;
    }
    if (confirm) {
        before[before_count++] = (ChordSpec){5, true, NULL};
        before[before_count++] = (ChordSpec){1, false, NULL};
    }
    size_t run_first = before_count;
    for (size_t i = 0; run && i < run->count; i++) {
        before[before_count++] = (ChordSpec){run->degrees[i], false, NULL};
    }
    borrow_predominants(&kind, rng, before, run_first, before_count - run_first);
    size_t dominant = before_count;
    if (kind.modal != NULL) {
        for (size_t a = 0; a < dominants; a++) {
            before[before_count++] = (ChordSpec){kind.cadence->approach_degrees[a], false, NULL};
        }
    } else if (!half) {
        before[before_count++] = (ChordSpec){5, seventh, NULL};
    }
    if (plagal) {
        before[before_count++] = (ChordSpec){1, false, NULL};
        before[before_count++] = (ChordSpec){4, false, NULL};
    }
    ChordSpec last = half ? (ChordSpec){kind.modal != NULL ? kind.cadence->approach_degrees[approach - 1] : 5, seventh, NULL}
                          : (ChordSpec){request->cadence == CADENCE_TYPE_DECEPTIVE ? 6 : 1, false, NULL};

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

    PhraseChord *chords = calloc(MAX_DEALT, sizeof(PhraseChord));
    if (!chords) {
        return false;
    }
    for (size_t i = 0; i < before_count; i++) {
        if (!make_chord(&kind, before[i], cells[i], &chords[i])) {
            free(chords);
            return false;
        }
    }
    if (!make_chord(&kind, last, last_cells, &chords[before_count])) {
        free(chords);
        return false;
    }
    /* An approach chord whose root lies a tritone from the arrival's (the locrian V to io) would put a tritone leap in the bass: it is
       written in first inversion, whose bass steps to the arrival (R-725). */
    if (kind.modal != NULL && !half && before_count > 0 && harmony_modal_tritone_approach(&kind)) {
        size_t cells_of = chords[before_count - 1].cells;
        if (!harmony_make_chord_in(&kind, kind.cadence->approach_degrees[approach - 1], 1, false, false, cells_of, &chords[before_count - 1])) {
            free(chords);
            return false;
        }
    }
    size_t count = before_count + 1;
    /* Chords that do not stand on the given bass or carry the given melody are dealt again, after the dominant is
       tried on the cadential six-four. No random number is drawn for it, so a deal that fits is unchanged. */
    if (request->has_given && !harmony_given_fits(request, chords, count, NULL) &&
        !(!half && open_on_cadential_six_four(request, chords, &count, dominant))) {
        free(chords);
        return false;
    }
    out->request = *request;
    out->chords = chords;
    out->count = count;
    return true;
}

static bool build_progression(const PhraseRequest *request, FillRng *rng, int pivot, bool confirm,
                              PhraseProgression *out) {
    size_t attempts = request->has_given ? GIVEN_DEAL_ATTEMPTS : (request->has_shape ? SHAPED_DEAL_ATTEMPTS : DEAL_ATTEMPTS);
    for (size_t attempt = 0; attempt < attempts; attempt++) {
        if (deal_progression(request, rng, pivot, confirm, out)) {
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

typedef enum { KEEP_ADDED, KEEP_REPEAT, KEEP_NO_MEMORY, KEEP_FAILED } KeepResult;

/* Adds the progression to the candidates unless an earlier one is the same (its chords are then freed). */
static KeepResult keep_progression(PhraseProgression *progression, PhraseProgression **kept, size_t *kept_count,
                                   FillCandidateList *out) {
    for (size_t i = 0; i < *kept_count; i++) {
        if (harmony_same_progression(kept[i], progression)) {
            free(progression->chords);
            return KEEP_REPEAT;
        }
    }
    PhraseProgression *payload = malloc(sizeof(*payload));
    if (!payload) {
        free(progression->chords);
        return KEEP_NO_MEMORY;
    }
    *payload = *progression;
    char label[FILL_LABEL_MAX];
    harmony_progression_label(payload, label, sizeof(label));
    if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
        /* fill_candidates_add destroyed the payload. */
        return KEEP_FAILED;
    }
    kept[(*kept_count)++] = payload;
    return KEEP_ADDED;
}

bool harmony_phrase_propose(const PhraseRequest *request, FillRng *rng, FillCandidateList *out) {
    PhraseProgression *kept[CANDIDATE_ATTEMPTS];
    size_t kept_count = 0;
    for (size_t attempt = 0; attempt < CANDIDATE_ATTEMPTS; attempt++) {
        PhraseProgression progression;
        /* The first candidate opens on the best pivot, the others on the other pivots in turn. */
        int pivot = request->pivot ? request->pivot_degrees[attempt % request->pivot_count] : 0;
        if (!build_progression(request, rng, pivot, false, &progression)) {
            /* A pivot that leaves too few chords for the shape is passed over; without a pivot, none fits. */
            if (request->pivot) {
                continue;
            }
            break;
        }
        KeepResult kept_result = keep_progression(&progression, kept, &kept_count, out);
        if (kept_result == KEEP_FAILED) {
            return false;
        }
        if (kept_result == KEEP_NO_MEMORY) {
            return true;
        }
    }
    /* When no pivot opens a phrase that fits (into V of a major key the pivots are its IV and ii, which leave a half
       cadence two chords at most), the predominant pivots go on to the new key's V7 and I (R-512). */
    bool confirm = request->pivot && kept_count == 0;
    for (size_t attempt = 0; confirm && attempt < CANDIDATE_ATTEMPTS; attempt++) {
        PhraseProgression progression;
        int pivot = request->pivot_degrees[attempt % request->pivot_count];
        if (!build_progression(request, rng, pivot, true, &progression)) {
            continue;
        }
        KeepResult kept_result = keep_progression(&progression, kept, &kept_count, out);
        if (kept_result == KEEP_FAILED) {
            return false;
        }
        if (kept_result == KEEP_NO_MEMORY) {
            return true;
        }
    }
    return true;
}
