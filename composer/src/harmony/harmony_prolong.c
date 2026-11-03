#include <ursatz_composer/harmony_prolong.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/* Candidates tried per call; ones that repeat an earlier one are dropped. */
#define CANDIDATE_ATTEMPTS 12
/* Deals tried for one figure before it is given up on, and when Given material limits which chords fit. */
#define DEAL_ATTEMPTS 24
#define GIVEN_DEAL_ATTEMPTS 160
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

/* The operator's context is non-NULL when it works around Given material (harmony_prolong_register_given). */
static const char AROUND_GIVEN = 1;

static bool read_request(const PiecePlan *plan, LaneSpanId target, bool around_given, PhraseRequest *out) {
    return around_given ? harmony_prolong_read_request_given(plan, target, out)
                        : harmony_prolong_read_request(plan, target, out);
}

static bool prolong_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseRequest request;
    return harmony_is_target(target) && read_request(plan, (LaneSpanId)target.id, context != NULL, &request);
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
    if (!harmony_shape_keeps_in(&request->shape, cells, figure->count, &request->metre, false)) {
        return false;
    }
    /* A span that ends on a root-position V keeps that V off a downbeat. */
    const FigureChord *last = &figure->chords[figure->count - 1];
    if (figure->ends == ANCHOR_DOMINANT && last->inversion == 0) {
        size_t start = request->cells - cells[figure->count - 1];
        if (harmony_metre_cell_on_downbeat(&request->metre, start)) {
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
    if (request->has_given && !harmony_given_fits(request, chords, figure->count, NULL)) {
        free(chords);
        return false;
    }
    out->request = *request;
    out->chords = chords;
    out->count = figure->count;
    return true;
}

static bool same_shape(const PlanHarmonyShape *a, const PlanHarmonyShape *b) {
    return a->kind == b->kind && a->starts_on == b->starts_on && a->ends_on == b->ends_on &&
           a->max_cells_per_chord == b->max_cells_per_chord && a->split_at_bars == b->split_at_bars;
}

/*
 * The chords written right before the span, when they fill as much time as the span, are the first span's shape and
 * the same shape as the request's: a prolongation that goes on as the one before it did repeats it. False when there
 * is nothing to repeat. The chords come back as the cells the request has, ready to be written again.
 */
static bool previous_chords(const PiecePlan *plan, const PhraseRequest *request, PhraseChord *chords, size_t *count) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    Rational before;
    /* The cells of a span with part cells are not those of the span before it. */
    if (request->lead.num > 0 || request->tail.num > 0) {
        return false;
    }
    if (rational_sub(request->span.start, request->span.length, &before) != RATIONAL_OK ||
        before.num < 0) {
        return false;
    }
    PlanSpan range = {before, request->span.length};
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, ids, HARMONY_MAX_CHORDS);
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    Rational position = before;
    size_t cells = 0;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED || rational_cmp(span.start, position) != 0 ||
            plan_span_end(span, &end) != PLAN_OK ||
            !harmony_read_chord(plan, ids[i], request->mode, request->cell_length, &chords[i])) {
            return false;
        }
        cells += chords[i].cells;
        position = end;
    }
    PlanConstraint shape;
    if (cells != request->cells || !harmony_find_constraint(plan, ids[0], plan_lane_span_owner(plan, ids[0]),
                                                           PLAN_CONSTRAINT_HARMONY_SHAPE, &shape) ||
        !same_shape(&shape.params.harmony_shape, &request->shape)) {
        return false;
    }
    *count = found;
    return true;
}

static bool prolong_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                             FillCandidateList *out) {
    PhraseRequest request;
    if (!harmony_is_target(target) || !read_request(plan, (LaneSpanId)target.id, context != NULL, &request)) {
        return false;
    }
    size_t wanted[FIGURE_COUNT];
    size_t wanted_count = 0;
    for (size_t i = 0; i < FIGURE_COUNT; i++) {
        if (FIGURES[i].count <= request.cells && figure_wanted(&FIGURES[i], &request.shape)) {
            wanted[wanted_count++] = i;
        }
    }
    PhraseProgression *kept[CANDIDATE_ATTEMPTS + 1];
    size_t kept_count = 0;

    /* First the chords of the span before, again, when there are some to repeat. */
    PhraseChord again[HARMONY_MAX_CHORDS];
    size_t again_count = 0;
    bool repeatable = previous_chords(plan, &request, again, &again_count);
    /* Said again, the chords must still stand on the given bass and carry the given melody. */
    bool repeat_fits = repeatable && (!request.has_given || harmony_given_fits(&request, again, again_count, NULL));
    if (repeat_fits) {
        PhraseProgression *payload = malloc(sizeof(*payload));
        PhraseChord *chords = calloc(again_count, sizeof(PhraseChord));
        if (!payload || !chords) {
            free(payload);
            free(chords);
            return false;
        }
        memcpy(chords, again, again_count * sizeof(PhraseChord));
        payload->request = request;
        payload->chords = chords;
        payload->count = again_count;
        payload->repeatable = true;
        payload->repeats = true;
        char label[FILL_LABEL_MAX];
        harmony_progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            return false;
        }
        kept[kept_count++] = payload;
    }
    for (size_t attempt = 0; wanted_count && attempt < CANDIDATE_ATTEMPTS; attempt++) {
        const Figure *figure = &FIGURES[wanted[fill_rng_below(rng, wanted_count)]];
        PhraseProgression progression;
        bool dealt = false;
        for (size_t deal = 0; !dealt && deal < (request.has_given ? GIVEN_DEAL_ATTEMPTS : DEAL_ATTEMPTS); deal++) {
            dealt = deal_figure(&request, figure, rng, &progression);
        }
        if (!dealt) {
            continue;
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
            return false;
        }
        *payload = progression;
        payload->repeatable = repeat_fits;
        payload->repeats = false;
        char label[FILL_LABEL_MAX];
        harmony_progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            return false;
        }
        kept[kept_count++] = payload;
    }
    return true;
}

static PlanResult prolong_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    return harmony_progression_apply(plan, target, candidate,
                                     context ? harmony_prolong_read_request_given : harmony_prolong_read_request);
}

static FillResult register_prolong(FillOperatorRegistry *registry, bool around_given) {
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
    operator_.context = around_given ? (void *)&AROUND_GIVEN : NULL;
    return fill_registry_register(registry, &operator_);
}

FillResult harmony_prolong_register(FillOperatorRegistry *registry) {
    return register_prolong(registry, false);
}

FillResult harmony_prolong_register_given(FillOperatorRegistry *registry) {
    return register_prolong(registry, true);
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
                          true, NULL, NULL, false, &progression->request.metre};
    PhraseVerdict verdict;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    const char *given_hard = NULL;
    if (!verdict.hard && progression->request.around_given &&
        harmony_given_fits(&progression->request, progression->chords, progression->count, &given_hard) &&
        !harmony_given_join(&progression->request, progression->chords, progression->count, false, &given_hard)) {
        return false;
    }
    if (verdict.hard || given_hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard ? verdict.hard : given_hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    /* Going on as the span before did is one more rule, held by the candidate that repeats it. */
    if (progression->repeatable) {
        out->score_numerator += progression->repeats ? 1 : 0;
        out->score_denominator += 1;
    }
    harmony_seam_score(progression, out);
    snprintf(out->reason, sizeof(out->reason), progression->repeats ? "prolongation, repeated" : "prolongation");
    return true;
}
