#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/*
 * A free phrase (R-170): a section of any length in beats, one that is not a theme of 4 or 8 whole bars. grow-theme lays
 * one idea over each bar of it, the part bar at either end joining the idea next to it (a section that does not start or
 * end on a barline, or is shorter than a bar, has fewer ideas than barlines). The first idea is the section's motive
 * exact, the middle ones continuation forms of it, the last a cadential form: the motives stage scores them by function
 * and connection through the idea's name, as it does the ideas of a theme (grow_form.c).
 *
 * One phrase holds the ideas, with the cadence the section ends in, and the chords and the bass over all of it.
 */

/* The most ideas a free phrase has: a section of more bars than this has several bars to an idea. */
#define MAX_IDEAS 32
/* The pitches a form may use, as in a theme's contrasting idea (grow_theme.c). */
#define FORM_LOW 58
#define FORM_HIGH 84
#define MAX_BARLINES 4096
/* Draws of a middle idea's development before the draw is given up. */
#define IDEA_DRAWS 12
#define DRAWS_PER_CANDIDATE 6
/* Chords a phrase model has between its first and its last: up to two predominants and a dominant. */
#define PHRASE_MODEL_SLOTS 4

struct GrowFreeSpec {
    unsigned long magic;
    size_t count;                       /* the ideas */
    PlanSpan span[MAX_IDEAS];           /* where each lies */
    MotiveDevSpec dev[MAX_IDEAS];       /* its development of the motive; [0] is unused: the first idea is exact */
    Motive developed[MAX_IDEAS];        /* what the development makes, to be judged */
    PlanCadenceType cadence;
    PlanHarmonyShapeKind shape;
    int cap;                            /* max_cells_per_chord of the harmony; 0 for none */
    Motive source;
    bool has_cadential;                 /* the last idea is a cadential form of the motive (more than one idea) */
};

/* ------------------------------------------------------------- the request */

typedef struct {
    GrowThemeRequest theme; /* the section, its span, meter, key, source and accompaniment, as a theme's are */
    PlanSpan ideas[MAX_IDEAS];
    size_t idea_count;
    size_t cells;           /* the cells of the section, part cells included */
    PlanHarmonyShapeKind shape;
    int cap;
} FreeRequest;

static bool has_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        if (plan_node_constraint_at(plan, id, i)->kind == kind) {
            return true;
        }
    }
    return false;
}

/*
 * The ideas over the section: one for each bar, cut at the barlines inside it; the part bar at the start joins the idea
 * after it and the part bar at the end the idea before. A section of more bars than MAX_IDEAS has several bars to an
 * idea. The barlines are the meter lane span's.
 */
static bool lay_ideas(const GrowThemeRequest *theme, PlanTime meter_start, FreeRequest *out) {
    PlanSpan span = theme->span;
    PlanTime end;
    PlanTime line = meter_start;
    PlanTime cuts[MAX_BARLINES];
    size_t cut_count = 0;
    bool start_on = false;
    bool end_on = false;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return false;
    }
    for (size_t k = 0; k < MAX_BARLINES; k++) {
        int to_end = plan_time_compare(line, end);
        int to_start = plan_time_compare(line, span.start);
        if (to_end > 0) {
            break;
        }
        start_on = start_on || to_start == 0;
        end_on = end_on || to_end == 0;
        if (to_start > 0 && to_end < 0) {
            cuts[cut_count++] = line;
        }
        if (plan_time_add(line, theme->bar, &line) != PLAN_OK) {
            return false;
        }
    }
    /* The part bars join their neighbours: drop the cut that ends the part bar at the start, and the one that begins
       the part bar at the end. */
    size_t first = 0;
    size_t last = cut_count;
    if (!start_on && first < last) {
        first++;
    }
    if (!end_on && first < last) {
        last--;
    }
    size_t usable = last - first;
    size_t ideas = usable + 1;
    size_t group = (ideas + MAX_IDEAS - 1) / MAX_IDEAS;
    PlanTime from = span.start;
    out->idea_count = 0;
    for (size_t at = group;; at += group) {
        PlanTime stop = at >= ideas ? end : cuts[first + at - 1];
        PlanSpan idea = {from, {0, 1}};
        if (plan_time_make(stop.num * (long)from.den - from.num * (long)stop.den, stop.den * from.den, &idea.length) !=
                PLAN_OK ||
            idea.length.num <= 0 || out->idea_count >= MAX_IDEAS) {
            return false;
        }
        out->ideas[out->idea_count++] = idea;
        from = stop;
        if (at >= ideas) {
            break;
        }
    }
    return true;
}

static bool read_free(const PiecePlan *plan, FillTarget target, FreeRequest *out) {
    FreeRequest request;
    GrowThemeRequest *theme = &request.theme;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0 ||
        !has_constraint(plan, target.id, PLAN_CONSTRAINT_FREE_PHRASE) ||
        has_constraint(plan, target.id, PLAN_CONSTRAINT_ARCHETYPE) ||
        has_constraint(plan, target.id, PLAN_CONSTRAINT_THEME_PAIRING) || form_return_handles(plan, target.id) ||
        has_constraint(plan, target.id, PLAN_CONSTRAINT_HOLD)) {
        return false;
    }
    theme->section = target.id;
    theme->span = plan_node_span(plan, target.id);
    bool cadence = false;
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE && !cadence) {
            cadence = true;
            theme->cadence = constraint->params.cadence;
        } else if (constraint->kind == PLAN_CONSTRAINT_ACCOMPANIMENT) {
            theme->has_accompaniment = true;
            theme->accompaniment = constraint->params.accompaniment;
        } else if (constraint->kind == PLAN_CONSTRAINT_VOICING) {
            theme->has_voicing = true;
            theme->voicing = constraint->params.voicing;
        }
    }
    LaneSpanId lane[2];
    if (!cadence ||
        (theme->cadence != PLAN_CADENCE_PAC && theme->cadence != PLAN_CADENCE_IAC && theme->cadence != PLAN_CADENCE_HC) ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, theme->span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), theme->span)) {
        return false;
    }
    PlanTime meter_start = plan_lane_span_span(plan, lane[0]).start;
    theme->meter = plan_lane_span_payload(plan, lane[0])->meter;
    if (!grow_read_keys(plan, theme) || theme->has_start_change || theme->has_end_change) {
        return false;
    }
    theme->bar = grow_bar_length(&theme->meter);
    theme->source = grow_find_source(plan);
    if (theme->source == PLAN_NODE_NONE || !grow_cells_per_bar(&theme->meter, &theme->cells_per_bar) ||
        theme->bar.num == 0 || !lay_ideas(theme, meter_start, &request)) {
        return false;
    }
    /* The chords: a phrase model if the section has room for one, else the cadential progression alone. */
    PhraseMetre metre;
    if (!harmony_read_metre(plan, theme->span, &metre)) {
        return false;
    }
    size_t cells = metre.cells;
    /* A span of whole bars from a barline ends in a chord of the whole last bar, which asks for more room than one that
       starts or ends between barlines (harmony_phrase_min_cells, harmony_phrase_room_cells). */
    bool whole_bars = metre.lead.num == 0 && metre.tail.num == 0 && metre.offset.num == 0 &&
                      cells % theme->cells_per_bar == 0;
    size_t least = whole_bars ? harmony_phrase_room_cells(theme->cadence, theme->cells_per_bar)
                              : harmony_phrase_min_cells(theme->cadence, &theme->meter);
    request.cells = cells;
    if (least == 0 || cells + (whole_bars ? 0 : 1) < least) {
        return false;
    }
    request.shape = cells >= least ? PLAN_HARMONY_SHAPE_PHRASE_MODEL : PLAN_HARMONY_SHAPE_CADENTIAL;
    /* Chords no longer than they must be, so a long phrase changes harmony and does not hold one for bars: the cells
       before the last chord dealt out to the chords a phrase model has, with half again to spare. */
    if (cells > 2 * theme->cells_per_bar) {
        size_t room = cells - theme->cells_per_bar;
        size_t cap = (room * 3 / 2 + PHRASE_MODEL_SLOTS - 1) / PHRASE_MODEL_SLOTS;
        request.cap = (int)(cap > theme->cells_per_bar ? cap : theme->cells_per_bar);
    }
    *out = request;
    return true;
}

bool grow_free_applies(const PiecePlan *plan, FillTarget target) {
    FreeRequest request;
    return read_free(plan, target, &request);
}

/* ------------------------------------------------------------- candidates */

/* Every pitch within the range a form may have, so the melody has room to place it. */
static bool in_range(const Motive *motive) {
    for (size_t i = 0; i < motive->note_count; i++) {
        if (motive->notes[i].midi_pitch < FORM_LOW || motive->notes[i].midi_pitch > FORM_HIGH) {
            return false;
        }
    }
    return true;
}

/*
 * The development of m0 a middle idea is: a leading fragment, a transposition in the key's steps (a sequence), an
 * inversion or an interval changed, drawn from rng. It has to fit the idea and the range. False when no draw does.
 */
static bool draw_continuation(const Motive *source, PlanTime room, FillRng *rng, MotiveDevSpec *dev, Motive *result) {
    for (size_t draw = 0; draw < IDEA_DRAWS; draw++) {
        memset(dev, 0, sizeof(*dev));
        switch (fill_rng_below(rng, 4)) {
        case 0: {
            size_t most = source->note_count - 1;
            size_t wanted = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
            dev->kind = MOTIVE_DEV_FRAGMENT;
            dev->position = PLAN_FRAGMENT_LEADING;
            dev->length = wanted < most ? wanted : most;
            dev->min_length = 1;
            break;
        }
        case 1: {
            int steps = (int)fill_rng_below(rng, 2) + 1;
            dev->kind = MOTIVE_DEV_TRANSPOSE;
            dev->tonal = true;
            dev->amount = fill_rng_below(rng, 2) == 0 ? steps : -steps;
            break;
        }
        case 2:
            dev->kind = MOTIVE_DEV_INVERT;
            dev->tonal = true;
            dev->has_axis = true;
            dev->axis = source->notes[0].midi_pitch;
            break;
        default: {
            int amount = (int)fill_rng_below(rng, 2) + 1;
            dev->kind = MOTIVE_DEV_INTERVAL_CHANGE;
            dev->interval = (int)fill_rng_below(rng, source->note_count - 1) + 1;
            dev->amount = fill_rng_below(rng, 2) == 0 ? amount : -amount;
            break;
        }
        }
        if (motive_develop(source, dev, result) == MOTIVE_DEV_OK && result->note_count >= 1 &&
            plan_time_compare(result->length, room) <= 0 && in_range(result)) {
            return true;
        }
    }
    return false;
}

/* The development of m0 the last idea is: m0 liquidated, as a theme's cadential idea is. */
static bool draw_cadential(const Motive *source, PlanTime room, FillRng *rng, MotiveDevSpec *dev, Motive *result) {
    size_t count = source->note_count;
    size_t drop_most = count / 2 > 1 ? count / 2 : 1;
    memset(dev, 0, sizeof(*dev));
    dev->kind = MOTIVE_DEV_LIQUIDATE;
    dev->drop = 1 + (size_t)fill_rng_below(rng, drop_most);
    if (dev->drop >= count) {
        dev->drop = count - 1;
    }
    dev->min_length = 1;
    return dev->drop >= 1 && motive_develop(source, dev, result) == MOTIVE_DEV_OK &&
           plan_time_compare(result->length, room) <= 0;
}

static bool draw_free(const PiecePlan *plan, const FreeRequest *request, FillRng *rng, GrowFreeSpec *spec) {
    const GrowThemeRequest *theme = &request->theme;
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_FREE_MAGIC;
    spec->count = request->idea_count;
    spec->cadence = theme->cadence;
    spec->shape = request->shape;
    spec->cap = request->cap;
    memcpy(spec->span, request->ideas, request->idea_count * sizeof(PlanSpan));
    if (!grow_read_motive(plan, theme->source, &theme->meter, &theme->key, &spec->source) || spec->source.note_count < 2 ||
        plan_time_compare(spec->source.length, request->ideas[0].length) > 0) {
        return false;
    }
    spec->developed[0] = spec->source;
    for (size_t i = 1; i < spec->count; i++) {
        bool last = i + 1 == spec->count;
        bool drawn = last ? draw_cadential(&spec->source, request->ideas[i].length, rng, &spec->dev[i], &spec->developed[i])
                          : draw_continuation(&spec->source, request->ideas[i].length, rng, &spec->dev[i],
                                              &spec->developed[i]);
        if (!drawn) {
            return false;
        }
    }
    spec->has_cadential = spec->count > 1;
    return true;
}

static bool same_free(const GrowFreeSpec *a, const GrowFreeSpec *b) {
    for (size_t i = 1; i < a->count; i++) {
        if (!motive_equal(&a->developed[i], &b->developed[i])) {
            return false;
        }
    }
    return true;
}

static void free_label(const GrowFreeSpec *spec, char *label, size_t size) {
    size_t used = (size_t)snprintf(label, size, "free phrase %zu ideas", spec->count);
    if (used < size && spec->count > 1) {
        char dev[32];
        motive_dev_label(&spec->dev[spec->count - 1], dev, sizeof(dev));
        snprintf(label + used, size - used, " ending %s", dev);
    }
}

bool grow_free_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, const GrowContext *context,
                        FillCandidateList *out) {
    FreeRequest request;
    if (!read_free(plan, target, &request)) {
        return true;
    }
    size_t wanted = context && context->candidates > 0 ? context->candidates : GROW_DEFAULT_CANDIDATES;
    GrowFreeSpec *kept[16];
    size_t kept_count = 0;
    if (wanted > 16) {
        wanted = 16;
    }
    for (size_t draw = 0; kept_count < wanted && draw < wanted * DRAWS_PER_CANDIDATE; draw++) {
        GrowFreeSpec *spec = malloc(sizeof(*spec));
        if (!spec) {
            return false;
        }
        bool repeat = false;
        if (!draw_free(plan, &request, rng, spec)) {
            free(spec);
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_free(kept[i], spec);
        }
        if (repeat) {
            free(spec);
            continue;
        }
        char label[FILL_LABEL_MAX];
        free_label(spec, label, sizeof(label));
        if (!fill_candidates_add(out, label, spec, free)) {
            return false;
        }
        kept[kept_count++] = spec;
    }
    return true;
}

/*
 * The rules of a free phrase. Hard: the ideas fit their bars (drawn so), and the cadential idea is shorter than m0.
 * Soft, the score the share that hold: no middle idea has the contour of the idea before it (a development that
 * repeats its neighbour is not a continuation), and the ideas get no longer toward the end.
 */
const char *grow_free_judge(const GrowFreeSpec *spec, unsigned *held, unsigned *total) {
    *held = 0;
    *total = 0;
    if (!spec || spec->count == 0) {
        return "no ideas";
    }
    if (spec->has_cadential && plan_time_compare(spec->developed[spec->count - 1].length, spec->source.length) >= 0) {
        return "cadential idea not shorter than the basic idea";
    }
    for (size_t i = 1; i + 1 < spec->count; i++) {
        (*total)++;
        *held += grow_same_contour(&spec->developed[i - 1], &spec->developed[i]) &&
                         motive_equal(&spec->developed[i - 1], &spec->developed[i])
                     ? 0u
                     : 1u;
    }
    if (spec->has_cadential) {
        (*total)++;
        *held += plan_time_compare(spec->developed[spec->count - 1].length, spec->source.length) < 0 ? 1u : 0u;
    }
    return NULL;
}

/* ------------------------------------------------------------- applying */

static const char *idea_name(size_t index, size_t count) {
    if (index == 0) {
        return "basic idea";
    }
    if (index + 1 == count) {
        return "cadential";
    }
    return index % 2 == 1 ? "fragment 1" : "fragment 2";
}

PlanResult grow_free_apply(PiecePlan *plan, FillTarget target, const GrowFreeSpec *spec) {
    FreeRequest request;
    if (!spec || spec->magic != GROW_FREE_MAGIC || !read_free(plan, target, &request) ||
        request.idea_count != spec->count) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    GrowBuild build;
    memset(&build, 0, sizeof(build));
    build.plan = plan;
    build.request = &request.theme;
    build.cadence.kind = PLAN_CONSTRAINT_CADENCE;
    build.cadence.params.cadence = request.theme.cadence;
    build.material = plan_node_parent(plan, request.theme.source);
    build.material_span = plan_node_span(plan, build.material);
    build.source = request.theme.source;
    build.slot_base = grow_first_free_bar(plan, build.material, build.material_span, request.theme.bar, spec->count - 1);

    /* The phrase over the section, with the chords and the bass over all of it. */
    NodeId phrase;
    PlanResult result = plan_add_node(plan, request.theme.section, PLAN_NODE_PHRASE, request.theme.span, "phrase", &phrase);
    if (result == PLAN_OK) {
        result = plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED);
    }
    if (result == PLAN_OK) {
        result = plan_node_add_constraint(plan, phrase, &build.cadence);
    }
    PlanHarmonyShape shape;
    memset(&shape, 0, sizeof(shape));
    shape.kind = spec->shape;
    shape.max_cells_per_chord = spec->cap;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(&build, phrase, &shape, &build.cadence);
    }

    /* The ideas, each with its motive node (all but the first) and its melody. */
    for (size_t i = 0; result == PLAN_OK && i < spec->count; i++) {
        NodeId idea;
        PlanMotiveRef ref;
        bool last = i + 1 == spec->count;
        result = plan_add_node(plan, phrase, PLAN_NODE_IDEA, spec->span[i], idea_name(i, spec->count), &idea);
        if (result == PLAN_OK) {
            result = plan_node_set_state(plan, idea, PLAN_STATE_REALIZED);
        }
        if (result != PLAN_OK) {
            break;
        }
        if (i == 0) {
            memset(&ref, 0, sizeof(ref));
            ref.motive = build.source;
            ref.transform = PLAN_TRANSFORM_NONE;
        } else {
            NodeId motive;
            PlanSpan bar = {{0, 1}, request.theme.bar};
            char name[16];
            PlanConstraint constraint;
            grow_dev_ref(&spec->dev[i], build.source, &ref);
            snprintf(name, sizeof(name), "m%zu", i);
            result = plan_time_add(build.material_span.start, grow_scaled(request.theme.bar, build.slot_base + i - 1),
                                   &bar.start);
            if (result == PLAN_OK) {
                result = plan_add_node(plan, build.material, PLAN_NODE_MOTIVE, bar, name, &motive);
            }
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
            constraint.params.motive = ref;
            if (result == PLAN_OK) {
                result = plan_node_add_constraint(plan, motive, &constraint);
            }
            ref.motive = motive;
        }
        if (result == PLAN_OK) {
            result = grow_add_melody_target(&build, idea, &ref, last ? &build.cadence : NULL);
        }
    }
    return result;
}
