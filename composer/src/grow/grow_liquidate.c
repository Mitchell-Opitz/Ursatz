#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/harmony_phrase.h>
#include <plan/plan_state.h>

#include "structure/plan_util.h"
#include "util/position.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include <plan/plan_constraint.h>
#include "motive/motive_internal.h"
#include "util/util.h"

/*
 * A liquidation (R-494): four bars at home over a dominant pedal that take the pattern apart. Five ideas: the basic idea
 * (the motive exact), its repetition a step down the key's scale, the motive liquidated, then half a bar of a scale line
 * and half a bar of a broken chord of V that ends the half cadence. One phrase holds them, with the cadence the section
 * ends in, and one prolongation of V over all of it with the pedal (as a presentation under a pedal, grow_beginning.c).
 *
 * The scale line and the broken chord are motives this operator makes (literal notes, like the head residue of
 * grow_ending.c); they take a bar each in the material, as the ideas' own motive nodes do.
 *
 * A retransition (R-505) is the same four bars and five ideas laid out the other way, for the section that leads home from
 * another region: the basic idea is the residue (m0 liquidated), the repetition the residue one note further, the first
 * fragment a scale line of eighths filling a bar, the second a broken chord of V in half a bar, and the cadential idea the
 * main theme's head (m0's leading notes that fit half a bar, written as they are) on the half cadence. It may open the home key from any region:
 * the prolongation of V over the pedal is the arrival. All five ideas have a motive node in the material.
 */

/* The pitches a form may use, as in grow_free.c. */
#define FORM_LOW 58
#define FORM_HIGH 84
#define IDEAS 5
/* The motive bars the ideas take in the material: the liquidation's repetition, liquidation, scale line and broken chord (4);
   the retransition's two residues, scale line, broken chord and head (5). */
#define MOTIVE_BARS 4
#define RETRANSITION_MOTIVE_BARS 5
/* Eighths in a half bar: 4 in 4/4, 3 in 3/4 and 6/8, 2 in 2/4 (R-505). */
#define MAX_RUN 4

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
    bool retransition;               /* the retransition layout: `liquidated` and `repeated` are its two residues, `head` its last idea */
    Motive head;                     /* the main theme head */
    size_t drop;
    bool down;
    int first_degree;                /* 2 or 4 */
};

typedef struct {
    GrowThemeRequest theme;
    PlanSpan ideas[IDEAS];
    size_t run;                      /* eighths in the broken chord, and in the liquidation's scale line */
    bool retransition;               /* a retransition (R-505) */
    size_t bar_run;                  /* eighths in a bar: the retransition's scale line */
} LiquidateRequest;

static bool read_liquidate(const Plan *plan, FillTarget target, LiquidateRequest *out) {
    LiquidateRequest request;
    GrowThemeRequest *theme = &request.theme;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0 ||
        plan_section_or_empty(plan, target.id)->has_pairing ||
        plan_section_or_empty(plan, target.id)->returns[0] != '\0' ||
        plan_section_or_empty(plan, target.id)->leads_in || form_return_handles(plan, target.id)) {
        return false;
    }
    /* A liquidation, or a retransition: a section whose function is retransition and which has no liquidation of its own. */
    SectionFunction function;
    const bool liquidation = plan_section_has_role(plan, target.id, SECTION_ROLE_LIQUIDATION);
    request.retransition = !liquidation && plan_section_function_of(plan, target.id, &function) &&
                           function.function == SECTION_FUNCTION_RETRANSITION;
    if (!liquidation && !request.retransition) {
        return false;
    }
    Rational meter_start;
    /* A retransition opens home from any region: the key before it is the region's (R-505). */
    if (!grow_read_section_target(plan, target, theme, &meter_start) || theme->cadence != CADENCE_TYPE_HALF ||
        (theme->has_start_change && !request.retransition) || theme->has_end_change) {
        return false;
    }
    /* Whole bars from a barline: the section starts on one of the meter span's barlines and is 4 bars. */
    if (!grow_whole_bars(theme, meter_start, 4)) {
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
    /* The eighths a half bar holds: 4 in 4/4, 3 in 3/4 and 6/8, 2 in 2/4. */
    Rational eighths;
    if (rational_div(half, (Rational){1, 8}, &eighths) != RATIONAL_OK || eighths.den != 1 || eighths.num < 2 ||
        eighths.num > MAX_RUN) {
        return false;
    }
    request.run = (size_t)eighths.num;
    Rational bar_eighths;
    if (rational_div(theme->bar, (Rational){1, 8}, &bar_eighths) != RATIONAL_OK || bar_eighths.den != 1 ||
        bar_eighths.num != 2 * eighths.num) {
        return false;
    }
    request.bar_run = (size_t)bar_eighths.num;
    *out = request;
    return true;
}

bool grow_liquidate_applies(const Plan *plan, FillTarget target) {
    LiquidateRequest request;
    return read_liquidate(plan, target, &request);
}

/* ------------------------------------------------------------- the made motives */

static bool minor_key(const SpelledKey *key) {
    return composer_key_is_minor(key);
}

/* The pitch of the step `position` of the key's scale (7 to an octave), the seventh raised in minor (the harmonic minor). */
static int step_pitch(const SpelledKey *key, long position) {
    long degree = ((position % 7) + 7) % 7;
    return motive_position_pitch(key, position, minor_key(key) && degree == 6 ? 1 : 0);
}

/* The step of degree (1 to 7) in the octave that puts its pitch nearest `pitch`; the nearer below on a tie. */
static long nearest_step(const SpelledKey *key, int degree, int pitch) {
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

/* Its own harmony is V over all of it, so the notes that are not tones of V are heard as passing tones over the pedal and
   the melody keeps the line. */
static void own_harmony_v(Motive *made) {
    made->chord_count = 1;
    memset(&made->chords[0], 0, sizeof(made->chords[0]));
    made->chords[0].onset = POSITION_ZERO;
    made->chords[0].degree = 5;
    made->tone_override_count = 0;
}

/*
 * A run of `count` eighths as a literal motive in m0's meter: a copy of base with its notes, length and no features,
 * tone labels and V for its own harmony, the pitches the steps of the key's scale `from`, `from + gap`, ... A run outside MIDI FORM_LOW to
 * FORM_HIGH is shifted an octave; false when it cannot be placed.
 */
static bool make_line(const Motive *base, const SpelledKey *key, size_t count, long from, const long *offsets, Motive *out) {
    for (int shift = 0; shift < 3; shift++) {
        long octave = shift == 0 ? 0 : (shift == 1 ? -7 : 7);
        Motive made = *base;
        bool fits = true;
        made.note_count = count;
        for (size_t i = 0; i < count; i++) {
            made.notes[i].pitch = step_pitch(key, from + octave + offsets[i]);
            made.notes[i].onset = position_mul((Rational){1, 8}, (long long)i);
            made.notes[i].duration = (Rational){1, 8};
            fits = fits && made.notes[i].pitch >= FORM_LOW && made.notes[i].pitch <= FORM_HIGH;
        }
        made.length = position_mul((Rational){1, 8}, (long long)count);
        made.pickup = POSITION_ZERO;
        made.feature_count = 0;
        own_harmony_v(&made);
        if (fits) {
            *out = made;
            return true;
        }
    }
    return false;
}

static bool make_run(const Motive *base, const SpelledKey *key, size_t count, long from, long gap, Motive *out) {
    long offsets[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < count && i < MOTIVE_MAX_NOTES; i++) {
        offsets[i] = gap * (long)i;
    }
    return make_line(base, key, count, from, offsets, out);
}

/*
 * The broken chord of V: every other step up from the degree 5 (5, 7, 2), and in a run of four eighths (4/4) the root again an
 * octave up (5, 7, 2, 5), so the last note is always a tone of V (the seventh, 4, would end on a dissonance).
 */
static bool make_broken(const Motive *base, const SpelledKey *key, size_t count, long from, Motive *out) {
    long offsets[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < count && i < MOTIVE_MAX_NOTES; i++) {
        offsets[i] = i < 3 ? 2 * (long)i : 7;
    }
    return make_line(base, key, count, from, offsets, out);
}

/* The scale line (`scale_run` eighths), then the broken chord of V (`run` eighths) from the degree 5 nearest the line's last note. */
static bool make_runs(GrowLiquidateSpec *spec, const SpelledKey *key, size_t run, size_t scale_run) {
    /* The line goes on from the idea before it: the liquidated idea, or the retransition's second residue. */
    const Motive *before = spec->retransition ? &spec->repeated : &spec->liquidated;
    int last_pitch = motive_pitch(before, before->note_count - 1);
    long first = nearest_step(key, spec->first_degree, last_pitch);
    if (!make_run(&spec->source, key, scale_run, first, spec->down ? -1 : 1, &spec->scale)) {
        return false;
    }
    long fifth = nearest_step(key, 5, motive_pitch(&spec->scale, spec->scale.note_count - 1));
    /* Degrees 5, 7, 2 (and 5 again in four eighths): the last a tone of V (2, 7 or 5). */
    return make_broken(&spec->source, key, run, fifth, &spec->broken);
}

/* ------------------------------------------------------------- candidates */

/*
 * The main theme head (R-505, and the coda's, R-507): m0's leading notes that fit half a bar, three at most and two at least, as a literal motive
 * (m0 cut to them, V for its own harmony). When the two leading notes do not fit, their onsets and durations are halved, once.
 */
bool grow_main_theme_head(const Motive *source, Rational half, Motive *out) {
    for (int halved = 0; halved < 2; halved++) {
        Motive made = *source;
        size_t count = 0;
        while (count < 3 && count < source->note_count) {
            Rational onset = source->notes[count].onset;
            Rational duration = source->notes[count].duration;
            if (halved) {
                onset = position_div(onset, 2);
                duration = position_div(duration, 2);
            }
            if (rational_cmp(position_add(onset, duration), half) > 0) {
                break;
            }
            made.notes[count].onset = onset;
            made.notes[count].duration = duration;
            count++;
        }
        if (halved && count > 2) {
            count = 2;
        }
        if (count < 2) {
            continue;
        }
        made.note_count = count;
        made.length = position_add(made.notes[count - 1].onset, made.notes[count - 1].duration);
        made.pickup = POSITION_ZERO;
        made.feature_count = 0;
        own_harmony_v(&made);
        *out = made;
        return true;
    }
    return false;
}

/* The retransition's residues, scale line, broken chord and head. False when a draw does not fit (the next is drawn). */
static bool draw_retransition(const LiquidateRequest *request, FillRng *rng, GrowLiquidateSpec *spec) {
    const GrowThemeRequest *theme = &request->theme;
    spec->retransition = true;
    /* The residue: 1 to half the notes dropped. The residue one note further keeps two notes at least (else it is drawn again). */
    if (!grow_draw_residues(&spec->source, rng, theme->bar, &spec->liquidated, &spec->repeated, &spec->liquidation, &spec->repetition,
                            &spec->drop)) {
        return false;
    }
    spec->down = fill_rng_below(rng, 2) == 0;
    spec->first_degree = fill_rng_below(rng, 2) == 0 ? 2 : 4;
    Rational half;
    if (rational_make(theme->bar.num, theme->bar.den * 2, &half) != RATIONAL_OK || !grow_main_theme_head(&spec->source, half, &spec->head) ||
        !grow_motive_in_form_range(&spec->head)) {
        return false;
    }
    return make_runs(spec, &theme->key, request->run, request->bar_run);
}

static bool draw_liquidate(const Plan *plan, const LiquidateRequest *request, FillRng *rng, GrowLiquidateSpec *spec) {
    const GrowThemeRequest *theme = &request->theme;
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_LIQUIDATE_MAGIC;
    memcpy(spec->span, request->ideas, sizeof(spec->span));
    if (!grow_read_motive(plan, theme->source, &theme->meter, &theme->key, &spec->source) || spec->source.note_count < 2 ||
        rational_cmp(spec->source.length, theme->bar) > 0 || !grow_motive_in_form_range(&spec->source)) {
        return false;
    }
    if (request->retransition) {
        return draw_retransition(request, rng, spec);
    }
    /* The repetition: one step down the key's scale. */
    memset(&spec->repetition, 0, sizeof(spec->repetition));
    spec->repetition.kind = MOTIVE_RELATION_TRANSPOSE;
    spec->repetition.value[0] = 1;
    spec->repetition.value[1] = -1;
    if (motive_develop(&spec->source, &spec->repetition, &spec->repeated) != MOTIVE_DEV_OK ||
        rational_cmp(spec->repeated.length, theme->bar) > 0 || !grow_motive_in_form_range(&spec->repeated)) {
        return false;
    }
    /* The liquidation: 1 to half the notes dropped, fewer than the notes. */
    size_t count = spec->source.note_count;
    size_t drop = grow_draw_drop(rng, count);
    MotiveRelationStep drawn;
    memset(&drawn, 0, sizeof(drawn));
    drawn.kind = MOTIVE_RELATION_LIQUIDATE;
    drawn.value[0] = (long)drop;
    drawn.value[2] = 1;
    if (motive_develop(&spec->source, &drawn, &spec->liquidated) != MOTIVE_DEV_OK ||
        spec->liquidated.note_count >= count || rational_cmp(spec->liquidated.length, theme->bar) > 0 ||
        !grow_motive_in_form_range(&spec->liquidated)) {
        return false;
    }
    spec->liquidation = drawn;
    spec->liquidation.value[2] = 0;
    spec->drop = drop;
    spec->down = fill_rng_below(rng, 2) == 0;
    spec->first_degree = fill_rng_below(rng, 2) == 0 ? 2 : 4;
    return make_runs(spec, &theme->key, request->run, request->run);
}

static bool same_liquidate(const GrowLiquidateSpec *a, const GrowLiquidateSpec *b) {
    return motive_equal(&a->liquidated, &b->liquidated) && motive_equal(&a->repeated, &b->repeated) &&
           motive_equal(&a->scale, &b->scale) && motive_equal(&a->broken, &b->broken);
}

static void liquidate_label(const GrowLiquidateSpec *spec, char *label, size_t size) {
    snprintf(label, size, "%s %zu dropped, scale %s", spec->retransition ? "retransition" : "liquidation", spec->drop,
             spec->down ? "down" : "up");
}

/* The kind of spec the candidate loop draws (grow_generate_specs). */
static bool liquidate_draw_spec(const Plan *plan, const void *request, const GrowContext *context, FillRng *rng, void *spec) {
    (void)context;
    return draw_liquidate(plan, (const LiquidateRequest *)request, rng, (GrowLiquidateSpec *)spec);
}

static bool liquidate_same_spec(const void *a, const void *b) {
    return same_liquidate((const GrowLiquidateSpec *)a, (const GrowLiquidateSpec *)b);
}

static void liquidate_label_spec(const void *spec, char *out, size_t size) {
    liquidate_label((const GrowLiquidateSpec *)spec, out, size);
}

static const GrowSpecKind LIQUIDATE_KIND = {sizeof(GrowLiquidateSpec), liquidate_draw_spec, liquidate_same_spec, liquidate_label_spec};

bool grow_liquidate_generate(const Plan *plan, FillTarget target, FillRng *rng, const GrowContext *context,
                             FillCandidateList *out) {
    LiquidateRequest request;
    if (!read_liquidate(plan, target, &request)) {
        return true;
    }
    return grow_generate_specs(plan, &request, context, rng, &LIQUIDATE_KIND, out);
}

/*
 * The rules of a liquidation. Hard: every idea fits its span (drawn so) and the liquidated idea has fewer notes than the
 * basic idea (a retransition: the repetition has fewer notes than the basic idea, which is the first residue). Soft, the share of ideas shorter than the one before them: shorter in its span, or in its notes when the
 * spans are as long (the spans are 1, 1, 1, 1/2 and 1/2 bars).
 */
const char *grow_liquidate_judge(const GrowLiquidateSpec *spec, unsigned *held, unsigned *total) {
    *held = 0;
    *total = 0;
    if (!spec || spec->magic != GROW_LIQUIDATE_MAGIC) {
        return "no liquidation";
    }
    const Motive *ideas[IDEAS] = {&spec->source, &spec->repeated, &spec->liquidated, &spec->scale, &spec->broken};
    if (spec->retransition) {
        /* The residue, the residue one note further, the scale line, the broken chord and the head. */
        ideas[0] = &spec->liquidated;
        ideas[1] = &spec->repeated;
        ideas[2] = &spec->scale;
        ideas[3] = &spec->broken;
        ideas[4] = &spec->head;
    }
    if (!grow_ideas_fit(ideas, spec->span, IDEAS)) {
        return "an idea is longer than its bars";
    }
    if (spec->retransition ? spec->repeated.note_count >= spec->liquidated.note_count
                           : spec->liquidated.note_count >= spec->source.note_count) {
        return spec->retransition ? "repetition not shorter than the basic idea"
                                  : "liquidated idea not shorter than the basic idea";
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

PlanResult grow_liquidate_apply(Plan *plan, FillTarget target, const GrowLiquidateSpec *spec) {
    LiquidateRequest request;
    if (!spec || spec->magic != GROW_LIQUIDATE_MAGIC || !read_liquidate(plan, target, &request)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    GrowBuild build;
    grow_build_init(&build, plan, &request.theme, spec->retransition ? RETRANSITION_MOTIVE_BARS : MOTIVE_BARS);

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

    /* The ideas: each with its melody; all but the first have a motive node in the material, bar by bar from slot_base (a
       retransition's five all do). */
    const bool retransition = spec->retransition;
    const size_t scale_idea = retransition ? 2 : 3;
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
        if (result == PLAN_OK && (i > 0 || retransition)) {
            Rational start;
            result = plan_result_of_rational(rational_add(
                build.material_span.start, position_times(request.theme.bar, build.slot_base + (retransition ? i : i - 1)),
                &start));
            if (result == PLAN_OK && i < (retransition ? 2u : 3u)) {
                /* The repetition and the liquidated idea (a retransition's two residues): m0 with a step, named by a motive
                   node. */
                PlanSpan bar = {start, request.theme.bar};
                char name[16];
                PlanConstraint constraint;
                PlanMotiveRef made;
                memset(&made, 0, sizeof(made));
                made.motive = build.source;
                made.has_step = true;
                made.step = retransition ? (i == 0 ? spec->liquidation : spec->repetition)
                                         : (i == 1 ? spec->repetition : spec->liquidation);
                snprintf(name, sizeof(name), "m%zu", retransition ? i + 1 : i);
                result = plan_add_node(plan, build.material, PLAN_NODE_MOTIVE, bar, name, &node);
                memset(&constraint, 0, sizeof(constraint));
                constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
                constraint.params.motive = made;
                if (result == PLAN_OK) {
                    result = plan_node_add_constraint(plan, node, &constraint);
                }
            } else if (result == PLAN_OK) {
                /* The scale line, the broken chord and a retransition's head: literal motives of the run's own, Realized, as
                   grow-ending makes. */
                const Motive *literal = i == scale_idea ? &spec->scale : (i == 4 && retransition ? &spec->head : &spec->broken);
                const char *name = i == scale_idea ? "scale line" : (i == 4 && retransition ? "main theme head" : "broken chord");
                result = motive_attach_as(plan, build.material, start, literal, name, PLAN_STATE_REALIZED, &node, NULL);
            }
            ref.motive = node;
        }
        if (result == PLAN_OK && i == scale_idea) {
            /* The scale line is written as it is: over the pedal its notes are not all tones of V, which the melody stage
               would move to chord tones. */
            result = grow_add_melody_written(&build, idea, &ref, &spec->scale);
        } else if (result == PLAN_OK && retransition && i + 1 == IDEAS) {
            /* The head is written as it is too: a melody target is made again under the cadence's rules and would not keep
               m0's intervals, which are the hint of the main theme. */
            result = grow_add_melody_written(&build, idea, &ref, &spec->head);
        } else if (result == PLAN_OK) {
            result = grow_add_melody_target(&build, idea, &ref, i + 1 == IDEAS ? &build.cadence : NULL);
        }
    }
    return result;
}
