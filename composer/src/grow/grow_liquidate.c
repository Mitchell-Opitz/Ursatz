#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "util/plan_util.h"
#include "util/position.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include <ursatz_composer/constraint.h>
#include "motive/motive_internal.h"

/*
 * A liquidation (R-494): four bars at home over a dominant pedal that take the pattern apart. Five ideas: the basic idea
 * (the motive exact), its repetition a step down the key's scale, the motive liquidated, then half a bar of a scale line
 * and half a bar of a broken chord of V that ends the half cadence. One phrase holds them, with the cadence the section
 * ends in, and one prolongation of V over all of it with the pedal (as a presentation under a pedal, grow_beginning.c).
 *
 * The scale line and the broken chord are motives this operator makes (literal notes, like the head residue of
 * grow_ending.c); they take a bar each in the material, as the ideas' own motive nodes do.
 */

/* The pitches a form may use, as in grow_free.c. */
#define FORM_LOW 58
#define FORM_HIGH 84
#define DRAWS_PER_CANDIDATE 6
#define IDEAS 5
/* The motive bars the ideas take in the material: the repetition, the liquidation, the scale line and the broken chord. */
#define MOTIVE_BARS 4
/* Eighths in a half bar: 3 in 3/4 and 6/8, 2 in 2/4. */
#define MAX_RUN 3

struct GrowLiquidateSpec {
    unsigned long magic;
    PlanSpan span[IDEAS];
    Motive source;
    MotiveRelationStep repetition;   /* the transposition one step down the key's scale */
    Motive repeated;
    MotiveRelationStep liquidation;  /* as the plan names it (value[2] 0); the draw's result is in `liquidated` */
    Motive liquidated;
    Motive scale;                    /* the scale line */
    Motive broken;                   /* the broken chord */
    size_t drop;
    bool down;
    int first_degree;                /* 2 or 4 */
};

typedef struct {
    GrowThemeRequest theme;
    PlanSpan ideas[IDEAS];
    size_t run;                      /* eighths in the scale line and in the broken chord */
} LiquidateRequest;

static bool read_liquidate(const PiecePlan *plan, FillTarget target, LiquidateRequest *out) {
    LiquidateRequest request;
    GrowThemeRequest *theme = &request.theme;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0 ||
        !plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_LIQUIDATION) ||
        plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_ARCHETYPE) ||
        plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_THEME_PAIRING) ||
        plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_RETURN) ||
        plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_HOLD) || form_return_handles(plan, target.id)) {
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
    if (!cadence || theme->cadence != CADENCE_TYPE_HALF ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, theme->span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), theme->span)) {
        return false;
    }
    Rational meter_start = plan_lane_span_span(plan, lane[0]).start;
    theme->meter = plan_lane_span_payload(plan, lane[0])->meter;
    if (!grow_read_keys(plan, theme) || theme->has_start_change || theme->has_end_change) {
        return false;
    }
    theme->bar = grow_bar_length(&theme->meter);
    theme->source = grow_find_source(plan);
    if (theme->source == PLAN_NODE_NONE || !grow_cells_per_bar(&theme->meter, &theme->cells_per_bar) ||
        theme->bar.num == 0) {
        return false;
    }
    /* Whole bars from a barline: the section starts on one of the meter span's barlines and is 4 bars. */
    Rational four;
    Rational offset;
    Rational bars_in;
    if (rational_mul_int(theme->bar, 4, &four) != RATIONAL_OK || rational_cmp(theme->span.length, four) != 0 ||
        rational_sub(theme->span.start, meter_start, &offset) != RATIONAL_OK ||
        rational_div(offset, theme->bar, &bars_in) != RATIONAL_OK || bars_in.den != 1) {
        return false;
    }
    /* The ideas: 1, 1, 1, 1/2 and 1/2 bars from the start. */
    Rational half;
    if (rational_make(theme->bar.num, theme->bar.den * 2, &half) != RATIONAL_OK) {
        return false;
    }
    Rational at = theme->span.start;
    for (size_t i = 0; i < IDEAS; i++) {
        Rational length = i < 3 ? theme->bar : half;
        request.ideas[i].start = at;
        request.ideas[i].length = length;
        if (rational_add(at, length, &at) != RATIONAL_OK) {
            return false;
        }
    }
    /* The eighths a half bar holds: 3 in 3/4 and 6/8, 2 in 2/4. */
    Rational eighths;
    if (rational_div(half, (Rational){1, 8}, &eighths) != RATIONAL_OK || eighths.den != 1 || eighths.num < 2 ||
        eighths.num > MAX_RUN) {
        return false;
    }
    request.run = (size_t)eighths.num;
    *out = request;
    return true;
}

bool grow_liquidate_applies(const PiecePlan *plan, FillTarget target) {
    LiquidateRequest request;
    return read_liquidate(plan, target, &request);
}

/* ------------------------------------------------------------- the made motives */

static bool minor_key(const AnalysisKey *key) {
    return key->mode == KEY_STRUCTURE_MODE_MINOR;
}

/* The pitch of the step `position` of the key's scale (7 to an octave), the seventh raised in minor (the harmonic minor). */
static int step_pitch(const AnalysisKey *key, long position) {
    long degree = ((position % 7) + 7) % 7;
    return motive_position_pitch(key, position, minor_key(key) && degree == 6 ? 1 : 0);
}

/* The step of degree (1 to 7) in the octave that puts its pitch nearest `pitch`; the nearer below on a tie. */
static long nearest_step(const AnalysisKey *key, int degree, int pitch) {
    long position;
    int alteration;
    motive_scale_position(key, pitch, &position, &alteration);
    long octave = position >= 0 ? position / 7 : -((-position + 6) / 7);
    long best = octave * 7 + (degree - 1);
    int best_gap = abs(step_pitch(key, best) - pitch);
    for (long around = octave - 1; around <= octave + 1; around++) {
        long candidate = around * 7 + (degree - 1);
        int gap = abs(step_pitch(key, candidate) - pitch);
        if (gap < best_gap || (gap == best_gap && candidate < best)) {
            best = candidate;
            best_gap = gap;
        }
    }
    return best;
}

/*
 * A run of `count` eighths as a literal motive in m0's meter: a copy of base with its notes, length and no features,
 * tone labels and V for its own harmony, the pitches the steps of the key's scale `from`, `from + gap`, ... A run outside MIDI FORM_LOW to
 * FORM_HIGH is shifted an octave; false when it cannot be placed.
 */
static bool make_run(const Motive *base, const AnalysisKey *key, size_t count, long from, long gap, Motive *out) {
    for (int shift = 0; shift < 3; shift++) {
        long octave = shift == 0 ? 0 : (shift == 1 ? -7 : 7);
        Motive made = *base;
        bool fits = true;
        made.note_count = count;
        for (size_t i = 0; i < count; i++) {
            made.notes[i].pitch = step_pitch(key, from + octave + gap * (long)i);
            made.notes[i].onset = position_mul((Rational){1, 8}, (long long)i);
            made.notes[i].duration = (Rational){1, 8};
            fits = fits && made.notes[i].pitch >= FORM_LOW && made.notes[i].pitch <= FORM_HIGH;
        }
        made.length = position_mul((Rational){1, 8}, (long long)count);
        made.pickup = POSITION_ZERO;
        made.feature_count = 0;
        /* Its own harmony is V over all of it, so the notes that are not tones of V are heard as passing tones over the
           pedal and the melody keeps the line. */
        made.chord_count = 1;
        memset(&made.chords[0], 0, sizeof(made.chords[0]));
        made.chords[0].onset = POSITION_ZERO;
        made.chords[0].degree = 5;
        made.tone_override_count = 0;
        if (fits) {
            *out = made;
            return true;
        }
    }
    return false;
}

/* The scale line, then the broken chord of V from the degree 5 nearest the line's last note. */
static bool make_runs(GrowLiquidateSpec *spec, const AnalysisKey *key, size_t run) {
    int last_pitch = motive_pitch(&spec->liquidated, spec->liquidated.note_count - 1);
    long first = nearest_step(key, spec->first_degree, last_pitch);
    if (!make_run(&spec->source, key, run, first, spec->down ? -1 : 1, &spec->scale)) {
        return false;
    }
    long fifth = nearest_step(key, 5, motive_pitch(&spec->scale, spec->scale.note_count - 1));
    /* Degrees 5, 7, 2: every other step up, the last a tone of V (2 or 7). */
    return make_run(&spec->source, key, run, fifth, 2, &spec->broken);
}

/* ------------------------------------------------------------- candidates */

static bool in_range(const Motive *motive) {
    for (size_t i = 0; i < motive->note_count; i++) {
        if (motive_pitch(motive, i) < FORM_LOW || motive_pitch(motive, i) > FORM_HIGH) {
            return false;
        }
    }
    return true;
}

static bool draw_liquidate(const PiecePlan *plan, const LiquidateRequest *request, FillRng *rng, GrowLiquidateSpec *spec) {
    const GrowThemeRequest *theme = &request->theme;
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_LIQUIDATE_MAGIC;
    memcpy(spec->span, request->ideas, sizeof(spec->span));
    if (!grow_read_motive(plan, theme->source, &theme->meter, &theme->key, &spec->source) || spec->source.note_count < 2 ||
        rational_cmp(spec->source.length, theme->bar) > 0 || !in_range(&spec->source)) {
        return false;
    }
    /* The repetition: one step down the key's scale. */
    memset(&spec->repetition, 0, sizeof(spec->repetition));
    spec->repetition.kind = MOTIVE_RELATION_TRANSPOSE;
    spec->repetition.value[0] = 1;
    spec->repetition.value[1] = -1;
    if (motive_develop(&spec->source, &spec->repetition, &spec->repeated) != MOTIVE_DEV_OK ||
        rational_cmp(spec->repeated.length, theme->bar) > 0 || !in_range(&spec->repeated)) {
        return false;
    }
    /* The liquidation: 1 to half the notes dropped, fewer than the notes. */
    size_t count = spec->source.note_count;
    size_t most = count / 2 > 1 ? count / 2 : 1;
    size_t drop = 1 + (size_t)fill_rng_below(rng, most);
    if (drop >= count) {
        drop = count - 1;
    }
    MotiveRelationStep drawn;
    memset(&drawn, 0, sizeof(drawn));
    drawn.kind = MOTIVE_RELATION_LIQUIDATE;
    drawn.value[0] = (long)drop;
    drawn.value[2] = 1;
    if (motive_develop(&spec->source, &drawn, &spec->liquidated) != MOTIVE_DEV_OK ||
        spec->liquidated.note_count >= count || rational_cmp(spec->liquidated.length, theme->bar) > 0 ||
        !in_range(&spec->liquidated)) {
        return false;
    }
    spec->liquidation = drawn;
    spec->liquidation.value[2] = 0;
    spec->drop = drop;
    spec->down = fill_rng_below(rng, 2) == 0;
    spec->first_degree = fill_rng_below(rng, 2) == 0 ? 2 : 4;
    return make_runs(spec, &theme->key, request->run);
}

static bool same_liquidate(const GrowLiquidateSpec *a, const GrowLiquidateSpec *b) {
    return motive_equal(&a->liquidated, &b->liquidated) && motive_equal(&a->scale, &b->scale) &&
           motive_equal(&a->broken, &b->broken);
}

static void liquidate_label(const GrowLiquidateSpec *spec, char *label, size_t size) {
    snprintf(label, size, "liquidation %zu dropped, scale %s", spec->drop, spec->down ? "down" : "up");
}

bool grow_liquidate_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, const GrowContext *context,
                             FillCandidateList *out) {
    LiquidateRequest request;
    if (!read_liquidate(plan, target, &request)) {
        return true;
    }
    size_t wanted = context && context->candidates > 0 ? context->candidates : GROW_DEFAULT_CANDIDATES;
    GrowLiquidateSpec *kept[16];
    size_t kept_count = 0;
    if (wanted > 16) {
        wanted = 16;
    }
    for (size_t draw = 0; kept_count < wanted && draw < wanted * DRAWS_PER_CANDIDATE; draw++) {
        GrowLiquidateSpec *spec = malloc(sizeof(*spec));
        if (!spec) {
            return false;
        }
        bool repeat = false;
        if (!draw_liquidate(plan, &request, rng, spec)) {
            free(spec);
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_liquidate(kept[i], spec);
        }
        if (repeat) {
            free(spec);
            continue;
        }
        char label[FILL_LABEL_MAX];
        liquidate_label(spec, label, sizeof(label));
        if (!fill_candidates_add(out, label, spec, free)) {
            return false;
        }
        kept[kept_count++] = spec;
    }
    return true;
}

/*
 * The rules of a liquidation. Hard: every idea fits its span (drawn so) and the liquidated idea has fewer notes than the
 * basic idea. Soft, the share of ideas shorter than the one before them: shorter in its span, or in its notes when the
 * spans are as long (the spans are 1, 1, 1, 1/2 and 1/2 bars).
 */
const char *grow_liquidate_judge(const GrowLiquidateSpec *spec, unsigned *held, unsigned *total) {
    *held = 0;
    *total = 0;
    if (!spec || spec->magic != GROW_LIQUIDATE_MAGIC) {
        return "no liquidation";
    }
    const Motive *ideas[IDEAS] = {&spec->source, &spec->repeated, &spec->liquidated, &spec->scale, &spec->broken};
    for (size_t i = 0; i < IDEAS; i++) {
        if (rational_cmp(ideas[i]->length, spec->span[i].length) > 0) {
            return "an idea is longer than its bars";
        }
    }
    if (spec->liquidated.note_count >= spec->source.note_count) {
        return "liquidated idea not shorter than the basic idea";
    }
    for (size_t i = 1; i < IDEAS; i++) {
        int by_span = rational_cmp(spec->span[i].length, spec->span[i - 1].length);
        (*total)++;
        *held += by_span < 0 || (by_span == 0 && ideas[i]->note_count < ideas[i - 1]->note_count) ? 1u : 0u;
    }
    return NULL;
}

/* ------------------------------------------------------------- applying */

static const char *const IDEA_NAMES[IDEAS] = {"basic idea", "repetition", "fragment 1", "fragment 2", "cadential"};

PlanResult grow_liquidate_apply(PiecePlan *plan, FillTarget target, const GrowLiquidateSpec *spec) {
    LiquidateRequest request;
    if (!spec || spec->magic != GROW_LIQUIDATE_MAGIC || !read_liquidate(plan, target, &request)) {
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
    build.slot_base = grow_first_free_bar(plan, build.material, build.material_span, request.theme.bar, MOTIVE_BARS);

    /* The phrase over the section, with the chords and the bass over all of it: V prolonged over the pedal. */
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
    shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    shape.starts_on = PLAN_HARMONY_ANCHOR_DOMINANT;
    shape.ends_on = PLAN_HARMONY_ANCHOR_DOMINANT;
    shape.pedal = true;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(&build, phrase, &shape, &build.cadence);
    }

    /* The ideas: each with its melody; all but the first have a motive node in the material, bar by bar from slot_base. */
    for (size_t i = 0; result == PLAN_OK && i < IDEAS; i++) {
        NodeId idea;
        NodeId node = PLAN_NODE_NONE;
        PlanMotiveRef ref;
        memset(&ref, 0, sizeof(ref));
        ref.motive = build.source;
        result = plan_add_node(plan, phrase, PLAN_NODE_IDEA, spec->span[i], IDEA_NAMES[i], &idea);
        if (result == PLAN_OK) {
            result = plan_node_set_state(plan, idea, PLAN_STATE_REALIZED);
        }
        if (result == PLAN_OK && i > 0) {
            Rational start;
            result = plan_result_of_rational(rational_add(
                build.material_span.start, grow_scaled(request.theme.bar, build.slot_base + i - 1), &start));
            if (result == PLAN_OK && i < 3) {
                /* The repetition and the liquidated idea: m0 with a step, named by a motive node. */
                PlanSpan bar = {start, request.theme.bar};
                char name[16];
                PlanConstraint constraint;
                PlanMotiveRef made;
                memset(&made, 0, sizeof(made));
                made.motive = build.source;
                made.has_step = true;
                made.step = i == 1 ? spec->repetition : spec->liquidation;
                snprintf(name, sizeof(name), "m%zu", i);
                result = plan_add_node(plan, build.material, PLAN_NODE_MOTIVE, bar, name, &node);
                memset(&constraint, 0, sizeof(constraint));
                constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
                constraint.params.motive = made;
                if (result == PLAN_OK) {
                    result = plan_node_add_constraint(plan, node, &constraint);
                }
            } else if (result == PLAN_OK) {
                /* The scale line and the broken chord: literal motives of the run's own, Realized, as grow-ending makes. */
                result = motive_attach_as(plan, build.material, start, i == 3 ? &spec->scale : &spec->broken,
                                          i == 3 ? "scale line" : "broken chord", PLAN_STATE_REALIZED, &node, NULL);
            }
            ref.motive = node;
        }
        if (result == PLAN_OK && i == 3) {
            /* The scale line is written as it is: over the pedal its notes are not all tones of V, which the melody stage
               would move to chord tones. */
            result = grow_add_melody_written(&build, idea, &ref, &spec->scale);
        } else if (result == PLAN_OK) {
            result = grow_add_melody_target(&build, idea, &ref, i + 1 == IDEAS ? &build.cadence : NULL);
        }
    }
    return result;
}
