#include <ursatz_composer/harmony_prolong.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/* Candidates tried per call; ones that repeat an earlier one are dropped. */
#define CANDIDATE_ATTEMPTS 12
/* Deals tried for one figure before it is given up on. */
#define DEAL_ATTEMPTS 24
#define MAX_FIGURE_CHORDS 4

typedef struct {
    int degree;
    int inversion;
    bool seventh;
    bool leading_tone; /* the diminished chord on the raised seventh degree */
} FigureChord;

typedef enum {
    ANCHOR_TONIC,
    ANCHOR_DOMINANT
} Anchor;

typedef struct {
    Anchor starts;
    Anchor ends;
    size_t count;
    FigureChord chords[MAX_FIGURE_CHORDS];
} Figure;

#define I {1, 0, false, false}
#define I6 {1, 1, false, false}
#define IV {4, 0, false, false}
#define V {5, 0, false, false}
#define V6 {5, 1, false, false}
#define V65 {5, 1, true, false}
#define V43 {5, 2, true, false}
#define VIIO6 {7, 1, false, true}

/* The prolongation figures of standard-harmony, and the same turned round for a span that opens on V. */
static const Figure FIGURES[] = {
    {ANCHOR_TONIC, ANCHOR_TONIC, 1, {I}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, V, I}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, V6, I}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, V65, I}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, VIIO6, I6}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, IV, I}},
    {ANCHOR_TONIC, ANCHOR_TONIC, 3, {I, V43, I6}},
    {ANCHOR_TONIC, ANCHOR_DOMINANT, 2, {I, V6}},
    {ANCHOR_TONIC, ANCHOR_DOMINANT, 2, {I, V65}},
    {ANCHOR_TONIC, ANCHOR_DOMINANT, 2, {I, V43}},
    {ANCHOR_TONIC, ANCHOR_DOMINANT, 2, {I, V}},
    {ANCHOR_TONIC, ANCHOR_DOMINANT, 3, {I, IV, V6}},
    {ANCHOR_DOMINANT, ANCHOR_TONIC, 2, {V6, I}},
    {ANCHOR_DOMINANT, ANCHOR_TONIC, 2, {V65, I}},
    {ANCHOR_DOMINANT, ANCHOR_TONIC, 2, {V43, I}},
    {ANCHOR_DOMINANT, ANCHOR_TONIC, 3, {V43, I6, I}},
    {ANCHOR_DOMINANT, ANCHOR_TONIC, 3, {V6, I6, I}},
};

#undef I
#undef I6
#undef IV
#undef V
#undef V6
#undef V65
#undef V43
#undef VIIO6

#define FIGURE_COUNT (sizeof(FIGURES) / sizeof(FIGURES[0]))

static bool is_target(FillTarget target) {
    return target.kind == FILL_TARGET_LANE_SPAN;
}

static bool prolong_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseRequest request;
    (void)context;
    return is_target(target) && harmony_prolong_read_request(plan, (LaneSpanId)target.id, &request);
}

static bool figure_wanted(const Figure *figure, const PlanHarmonyShape *shape) {
    Anchor starts = shape->starts_on == PLAN_HARMONY_ANCHOR_DOMINANT ? ANCHOR_DOMINANT : ANCHOR_TONIC;
    if (figure->starts != starts) {
        return false;
    }
    return shape->ends_on == PLAN_HARMONY_ANCHOR_ANY ||
           (shape->ends_on == PLAN_HARMONY_ANCHOR_DOMINANT) == (figure->ends == ANCHOR_DOMINANT);
}

static bool make_figure_chord(PlanKeyMode mode, const FigureChord *spec, size_t cells, PhraseChord *out) {
    if (spec->leading_tone) {
        return harmony_make_leading_tone(mode, spec->inversion, cells, out);
    }
    return harmony_make_chord(mode, spec->degree, spec->inversion, spec->seventh, false, cells, out);
}

/* One deal of the request's cells to the figure's chords, or false when this deal does not keep to the shape. */
static bool deal_figure(const PhraseRequest *request, const Figure *figure, FillRng *rng, PhraseProgression *out) {
    if (figure->count > request->cells) {
        return false;
    }
    size_t cells[MAX_FIGURE_CHORDS];
    size_t cap = request->shape.max_cells_per_chord > 0 ? (size_t)request->shape.max_cells_per_chord : 0;
    for (size_t i = 0; i < figure->count; i++) {
        cells[i] = 1;
    }
    for (size_t left = request->cells - figure->count; left > 0; left--) {
        size_t open[MAX_FIGURE_CHORDS];
        size_t open_count = 0;
        for (size_t i = 0; i < figure->count; i++) {
            if (cap == 0 || cells[i] < cap) {
                open[open_count++] = i;
            }
        }
        if (open_count == 0) {
            return false;
        }
        cells[open[fill_rng_below(rng, open_count)]]++;
    }
    if (!harmony_shape_keeps(&request->shape, cells, figure->count, request->cells_per_bar, false)) {
        return false;
    }
    /* A span that ends on a root-position V keeps that V off the bar's first cell. */
    const FigureChord *last = &figure->chords[figure->count - 1];
    if (figure->ends == ANCHOR_DOMINANT && last->inversion == 0) {
        size_t start = request->cells - cells[figure->count - 1];
        if (start % request->cells_per_bar == 0) {
            return false;
        }
    }
    PhraseChord *chords = calloc(figure->count, sizeof(PhraseChord));
    if (!chords) {
        return false;
    }
    for (size_t i = 0; i < figure->count; i++) {
        if (!make_figure_chord(request->mode, &figure->chords[i], cells[i], &chords[i])) {
            free(chords);
            return false;
        }
    }
    out->request = *request;
    out->chords = chords;
    out->count = figure->count;
    return true;
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

static bool prolong_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                             FillCandidateList *out) {
    PhraseRequest request;
    (void)context;
    if (!is_target(target) || !harmony_prolong_read_request(plan, (LaneSpanId)target.id, &request)) {
        return false;
    }
    size_t wanted[FIGURE_COUNT];
    size_t wanted_count = 0;
    for (size_t i = 0; i < FIGURE_COUNT; i++) {
        if (FIGURES[i].count <= request.cells && figure_wanted(&FIGURES[i], &request.shape)) {
            wanted[wanted_count++] = i;
        }
    }
    PhraseProgression *kept[CANDIDATE_ATTEMPTS];
    size_t kept_count = 0;
    for (size_t attempt = 0; wanted_count && attempt < CANDIDATE_ATTEMPTS; attempt++) {
        const Figure *figure = &FIGURES[wanted[fill_rng_below(rng, wanted_count)]];
        PhraseProgression progression;
        bool dealt = false;
        for (size_t deal = 0; !dealt && deal < DEAL_ATTEMPTS; deal++) {
            dealt = deal_figure(&request, figure, rng, &progression);
        }
        if (!dealt) {
            continue;
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
            return false;
        }
        *payload = progression;
        char label[FILL_LABEL_MAX];
        progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            return false;
        }
        kept[kept_count++] = payload;
    }
    return true;
}

static PlanResult prolong_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    return harmony_progression_apply(plan, target, candidate, harmony_prolong_read_request);
}

FillResult harmony_prolong_register(FillOperatorRegistry *registry) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_PROLONG_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span with a prolongation shape";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "none";
    operator_.generator.constraints_description = "standard-harmony prolongation figures, no cadence";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = prolong_applies;
    operator_.generate = prolong_generate;
    operator_.apply = prolong_apply;
    return fill_registry_register(registry, &operator_);
}

bool harmony_prolong_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                             const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, HARMONY_PROLONG_OPERATOR_NAME) != 0) {
        return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
    }
    const PhraseProgression *progression = candidate->payload;
    if (!progression || progression->count == 0 || progression->count > HARMONY_MAX_CHORDS) {
        return false;
    }
    /* The recognizer is told no soprano (a half cadence requested implies none), so any cadence it hears is
       heard from the harmony and the beats alone. */
    HeardPhrase phrase = {progression->chords, progression->count, progression->request.mode, PLAN_CADENCE_HC,
                          progression->request.cells_per_bar, progression->request.cell_length, true, NULL, true,
                          true};
    PhraseVerdict verdict;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    snprintf(out->reason, sizeof(out->reason), "prolongation");
    return true;
}
