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

/*
 * A coda (R-507): eight bars at home in three phrases, each ending in a perfect authentic cadence, that take the main
 * theme leave-taking: it is quoted, then liquidated, then only its head and a held tonic are left.
 *
 *   phrase 1, bars 1 to 4: the basic idea (m0 exact), its repetition (m0 exact), and the cadential idea (m0 augmented 2:1 in
 *       bars 3 and 4, or m0 exact when the augmentation is longer than two bars or the meter cannot write it);
 *   phrase 2, bars 5 and 6: fragment 1 (m0 liquidated) and the cadential idea (m0 liquidated one note further);
 *   phrase 3, bars 7 and 8: fragment 2 (the main theme head, written as it is) and the cadential idea (the tonic, a bar long).
 *
 * The liquidated ideas are motive nodes of the material named by a relation to m0; the head and the tonic are motives this
 * operator makes (literal notes, as the retransition's head is, grow_liquidate.c). The coda is one phrase more than the
 * liquidation: its three phrases are 4, 2 and 2 bars, so the section keeps getting shorter.
 */

/* The pitches a form may use, as in grow_liquidate.c. */
#define FORM_LOW 58
#define FORM_HIGH 84
#define DRAWS_PER_CANDIDATE 6
#define BARS 8
#define PHRASES 3
#define IDEAS 7
/* The motive bars the coda takes in the material: m1 (a bar, two when augmented), m2, m3, the head and the tonic. */
#define MOTIVE_BARS 5

/* The ideas, by position: the phrase each belongs to, the bar it starts in and the bars it has. */
static const size_t IDEA_PHRASE[IDEAS] = {0, 0, 0, 1, 1, 2, 2};
static const size_t IDEA_BAR[IDEAS] = {0, 1, 2, 4, 5, 6, 7};
static const size_t IDEA_BARS[IDEAS] = {1, 1, 2, 1, 1, 1, 1};
static const size_t PHRASE_BAR[PHRASES] = {0, 4, 6};
static const size_t PHRASE_BARS[PHRASES] = {4, 2, 2};
static const char *const IDEA_NAMES[IDEAS] = {"basic idea", "repetition", "cadential", "fragment 1",
                                               "cadential",  "fragment 2", "cadential"};

struct GrowCodaSpec {
    unsigned long magic;
    PlanSpan idea_span[IDEAS];
    PlanSpan phrase_span[PHRASES];
    Motive source;
    bool augmented;                  /* the cadential idea of phrase 1 is m0 augmented 2:1, node m1 */
    MotiveRelationStep augmentation;
    Motive augment;
    MotiveRelationStep liquidation;  /* fragment 1, as the plan names it (value[2] 0) */
    Motive liquidated;
    MotiveRelationStep repetition;   /* the cadential idea of phrase 2: one note further */
    Motive repeated;
    Motive head;                     /* fragment 2: the main theme head */
    Motive tone;                     /* the cadential idea of phrase 3: the tonic, a bar long */
    size_t drop;
};

typedef struct {
    GrowThemeRequest theme;
    PlanSpan ideas[IDEAS];
    PlanSpan phrases[PHRASES];
} CodaRequest;

bool grow_motive_in_form_range(const Motive *motive) {
    for (size_t i = 0; i < motive->note_count; i++) {
        if (motive_pitch(motive, i) < FORM_LOW || motive_pitch(motive, i) > FORM_HIGH) {
            return false;
        }
    }
    return true;
}

/* `start` and `count` bars later. */
static Rational bar_offset(Rational start, size_t count, Rational bar) {
    Rational out;
    return rational_add(start, position_times(bar, count), &out) == RATIONAL_OK ? out : start;
}

static bool read_coda(const Plan *plan, FillTarget target, CodaRequest *out) {
    CodaRequest request;
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
    SectionFunction function;
    if (!plan_section_function_of(plan, target.id, &function) || function.function != SECTION_FUNCTION_CODA) {
        return false;
    }
    theme->section = target.id;
    theme->span = plan_node_span(plan, target.id);
    const bool cadence = plan_section_or_empty(plan, target.id)->has_cadence;
    if (cadence) {
        theme->cadence = plan_section_or_empty(plan, target.id)->cadence;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_ACCOMPANIMENT) {
            theme->has_accompaniment = true;
            theme->accompaniment = constraint->params.accompaniment;
        } else if (constraint->kind == PLAN_CONSTRAINT_VOICING) {
            theme->has_voicing = true;
            theme->voicing = constraint->params.voicing;
        }
    }
    LaneSpanId lane[2];
    if (!cadence || theme->cadence != CADENCE_TYPE_PERFECT_AUTHENTIC ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, theme->span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), theme->span)) {
        return false;
    }
    Rational meter_start = plan_lane_span_span(plan, lane[0]).start;
    theme->meter = plan_lane_span_payload(plan, lane[0])->meter;
    /* One key over all eight bars: the coda is at home. */
    if (!grow_read_keys(plan, theme) || theme->has_start_change || theme->has_end_change) {
        return false;
    }
    plan_meter_bar_length(&theme->meter, &theme->bar);
    theme->source = grow_find_source(plan);
    if (theme->source == PLAN_NODE_NONE || !grow_cells_per_bar(&theme->meter, &theme->cells_per_bar) ||
        theme->bar.num == 0) {
        return false;
    }
    /* Eight whole bars from a barline. */
    Rational eight;
    Rational offset;
    Rational bars_in;
    if (rational_mul_int(theme->bar, BARS, &eight) != RATIONAL_OK || rational_cmp(theme->span.length, eight) != 0 ||
        rational_sub(theme->span.start, meter_start, &offset) != RATIONAL_OK ||
        rational_div(offset, theme->bar, &bars_in) != RATIONAL_OK || bars_in.den != 1) {
        return false;
    }
    /* m0 (when it is made yet; the draw reads it again): two notes or more, at most a bar, in range. */
    Motive source;
    if (grow_read_motive(plan, theme->source, &theme->meter, &theme->key, &source) &&
        (source.note_count < 2 || rational_cmp(source.length, theme->bar) > 0 || !grow_motive_in_form_range(&source))) {
        return false;
    }
    for (size_t i = 0; i < IDEAS; i++) {
        request.ideas[i].start = bar_offset(theme->span.start, IDEA_BAR[i], theme->bar);
        request.ideas[i].length = position_times(theme->bar, IDEA_BARS[i]);
    }
    for (size_t i = 0; i < PHRASES; i++) {
        request.phrases[i].start = bar_offset(theme->span.start, PHRASE_BAR[i], theme->bar);
        request.phrases[i].length = position_times(theme->bar, PHRASE_BARS[i]);
    }
    *out = request;
    return true;
}

bool grow_coda_applies(const Plan *plan, FillTarget target) {
    CodaRequest request;
    return read_coda(plan, target, &request);
}

/* ------------------------------------------------------------- the made motives */

/* The tonic of the key in the octave that puts it nearest `pitch` (the lower on a tie), within MIDI FORM_LOW to FORM_HIGH. */
static bool nearest_tonic(const SpelledKey *key, int pitch, int *out) {
    long position;
    int alteration;
    motive_scale_position(key, pitch, &position, &alteration);
    long octave = position >= 0 ? position / 7 : -((-position + 6) / 7);
    bool found = false;
    int best = 0;
    for (long around = octave - 1; around <= octave + 1; around++) {
        int tonic = motive_position_pitch(key, around * 7, 0);
        if (tonic < FORM_LOW || tonic > FORM_HIGH) {
            continue;
        }
        if (!found || abs(tonic - pitch) < abs(best - pitch) || (abs(tonic - pitch) == abs(best - pitch) && tonic < best)) {
            best = tonic;
            found = true;
        }
    }
    *out = best;
    return found;
}

/* The held tonic: one note, a bar long, a copy of m0 with its own harmony I. */
static bool make_tone(const Motive *base, const SpelledKey *key, Rational bar, int near, Motive *out) {
    int pitch;
    if (!nearest_tonic(key, near, &pitch)) {
        return false;
    }
    Motive made = *base;
    made.note_count = 1;
    made.notes[0].pitch = pitch;
    made.notes[0].onset = POSITION_ZERO;
    made.notes[0].duration = bar;
    made.length = bar;
    made.pickup = POSITION_ZERO;
    made.feature_count = 0;
    made.chord_count = 1;
    memset(&made.chords[0], 0, sizeof(made.chords[0]));
    made.chords[0].onset = POSITION_ZERO;
    made.chords[0].degree = 1;
    made.tone_override_count = 0;
    *out = made;
    return true;
}

/* ------------------------------------------------------------- candidates */

static bool draw_coda(const Plan *plan, const CodaRequest *request, FillRng *rng, GrowCodaSpec *spec) {
    const GrowThemeRequest *theme = &request->theme;
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_CODA_MAGIC;
    memcpy(spec->idea_span, request->ideas, sizeof(spec->idea_span));
    memcpy(spec->phrase_span, request->phrases, sizeof(spec->phrase_span));
    if (!grow_read_motive(plan, theme->source, &theme->meter, &theme->key, &spec->source) || spec->source.note_count < 2 ||
        rational_cmp(spec->source.length, theme->bar) > 0 || !grow_motive_in_form_range(&spec->source)) {
        return false;
    }
    const size_t count = spec->source.note_count;
    /* Phrase 1's cadential idea: m0 augmented 2:1 when two bars hold it and the meter can write it, else m0 exact. */
    Rational two;
    memset(&spec->augmentation, 0, sizeof(spec->augmentation));
    spec->augmentation.kind = MOTIVE_RELATION_AUGMENT;
    spec->augmentation.value[0] = 2;
    spec->augmentation.den[0] = 1;
    if (rational_mul_int(theme->bar, 2, &two) != RATIONAL_OK) {
        return false;
    }
    spec->augmented = motive_develop(&spec->source, &spec->augmentation, &spec->augment) == MOTIVE_DEV_OK &&
                      rational_cmp(spec->augment.length, two) <= 0 && grow_motive_in_form_range(&spec->augment);
    /* Phrase 2: m0 liquidated, 1 to half the notes dropped, then one note further (as the retransition's residues). */
    size_t most = count / 2 > 1 ? count / 2 : 1;
    size_t drop = 1 + (size_t)fill_rng_below(rng, most);
    if (drop >= count) {
        drop = count - 1;
    }
    MotiveRelationStep first;
    memset(&first, 0, sizeof(first));
    first.kind = MOTIVE_RELATION_LIQUIDATE;
    first.value[0] = (long)drop;
    first.value[2] = 1;
    MotiveRelationStep second = first;
    second.value[0] = (long)drop + 1;
    second.value[2] = 0;
    if (motive_develop(&spec->source, &first, &spec->liquidated) != MOTIVE_DEV_OK ||
        spec->liquidated.note_count >= count || rational_cmp(spec->liquidated.length, theme->bar) > 0 ||
        !grow_motive_in_form_range(&spec->liquidated) || motive_develop(&spec->source, &second, &spec->repeated) != MOTIVE_DEV_OK ||
        spec->repeated.note_count >= spec->liquidated.note_count || rational_cmp(spec->repeated.length, theme->bar) > 0 ||
        !grow_motive_in_form_range(&spec->repeated)) {
        return false;
    }
    spec->liquidation = first;
    spec->liquidation.value[2] = 0;
    spec->repetition = second;
    spec->drop = drop;
    /* Phrase 3: the main theme head, then the tonic nearest its last note. */
    Rational half;
    if (rational_make(theme->bar.num, theme->bar.den * 2, &half) != RATIONAL_OK ||
        !grow_main_theme_head(&spec->source, half, &spec->head) || !grow_motive_in_form_range(&spec->head)) {
        return false;
    }
    return make_tone(&spec->source, &theme->key, theme->bar, motive_pitch(&spec->head, spec->head.note_count - 1),
                     &spec->tone);
}

static bool same_coda(const GrowCodaSpec *a, const GrowCodaSpec *b) {
    return motive_equal(&a->liquidated, &b->liquidated) && motive_equal(&a->repeated, &b->repeated);
}

static void coda_label(const GrowCodaSpec *spec, char *label, size_t size) {
    snprintf(label, size, "coda %zu dropped", spec->drop);
}

bool grow_coda_generate(const Plan *plan, FillTarget target, FillRng *rng, const GrowContext *context,
                        FillCandidateList *out) {
    CodaRequest request;
    if (!read_coda(plan, target, &request)) {
        return true;
    }
    size_t wanted = context && context->candidates > 0 ? context->candidates : GROW_DEFAULT_CANDIDATES;
    GrowCodaSpec *kept[16];
    size_t kept_count = 0;
    if (wanted > 16) {
        wanted = 16;
    }
    for (size_t draw = 0; kept_count < wanted && draw < wanted * DRAWS_PER_CANDIDATE; draw++) {
        GrowCodaSpec *spec = malloc(sizeof(*spec));
        if (!spec) {
            return false;
        }
        bool repeat = false;
        if (!draw_coda(plan, &request, rng, spec)) {
            free(spec);
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_coda(kept[i], spec);
        }
        if (repeat) {
            free(spec);
            continue;
        }
        char label[FILL_LABEL_MAX];
        coda_label(spec, label, sizeof(label));
        if (!fill_candidates_add(out, label, spec, free)) {
            return false;
        }
        kept[kept_count++] = spec;
    }
    return true;
}

/*
 * The rules of a coda. Hard: every idea fits its span, fragment 1 has fewer notes than m0 and the cadential idea of phrase 2
 * fewer than fragment 1. Soft: the share of phrases as long as or shorter than the one before them (4, 2, 2: both), and of the
 * ideas inside phrases 2 and 3 that have fewer notes than the one before.
 */
const char *grow_coda_judge(const GrowCodaSpec *spec, unsigned *held, unsigned *total) {
    *held = 0;
    *total = 0;
    if (!spec || spec->magic != GROW_CODA_MAGIC) {
        return "no coda";
    }
    const Motive *ideas[IDEAS] = {&spec->source,     &spec->source,   spec->augmented ? &spec->augment : &spec->source,
                                  &spec->liquidated, &spec->repeated, &spec->head,
                                  &spec->tone};
    for (size_t i = 0; i < IDEAS; i++) {
        if (rational_cmp(ideas[i]->length, spec->idea_span[i].length) > 0) {
            return "an idea is longer than its bars";
        }
    }
    if (spec->liquidated.note_count >= spec->source.note_count) {
        return "fragment 1 not shorter than the basic idea";
    }
    if (spec->repeated.note_count >= spec->liquidated.note_count) {
        return "the cadential idea not shorter than fragment 1";
    }
    for (size_t i = 1; i < PHRASES; i++) {
        (*total)++;
        *held += rational_cmp(spec->phrase_span[i].length, spec->phrase_span[i - 1].length) <= 0 ? 1u : 0u;
    }
    for (size_t i = 4; i < IDEAS; i++) {
        if (IDEA_PHRASE[i] != IDEA_PHRASE[i - 1]) {
            continue;
        }
        (*total)++;
        *held += ideas[i]->note_count < ideas[i - 1]->note_count ? 1u : 0u;
    }
    return NULL;
}

/* ------------------------------------------------------------- applying */

/* A motive node of the material at the bar `bar` of it, `bars` bars long. */
static PlanResult add_node_motive(GrowBuild *build, size_t bar, size_t bars, const char *name, const PlanMotiveRef *ref,
                                  NodeId *out) {
    PlanSpan span;
    span.length = position_times(build->request->bar, bars);
    PlanResult result =
        plan_result_of_rational(rational_add(build->material_span.start, position_times(build->request->bar, bar), &span.start));
    PlanConstraint constraint;
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, build->material, PLAN_NODE_MOTIVE, span, name, out);
    }
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = *ref;
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, *out, &constraint) : result;
}

PlanResult grow_coda_apply(Plan *plan, FillTarget target, const GrowCodaSpec *spec) {
    CodaRequest request;
    if (!spec || spec->magic != GROW_CODA_MAGIC || !read_coda(plan, target, &request)) {
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
    build.slot_base = grow_first_free_bar(plan, build.material, build.material_span, request.theme.bar,
                                          MOTIVE_BARS + (spec->augmented ? 1 : 0));

    /* The motive nodes, bar by bar from slot_base: m1 (when augmented), m2, m3, the head and the tonic. */
    NodeId nodes[IDEAS];
    for (size_t i = 0; i < IDEAS; i++) {
        nodes[i] = build.source;
    }
    size_t slot = build.slot_base;
    PlanResult result = PLAN_OK;
    PlanMotiveRef ref;
    if (spec->augmented) {
        memset(&ref, 0, sizeof(ref));
        ref.motive = build.source;
        ref.has_step = true;
        ref.step = spec->augmentation;
        result = add_node_motive(&build, slot, 2, "m1", &ref, &nodes[2]);
        slot += 2;
    }
    if (result == PLAN_OK) {
        memset(&ref, 0, sizeof(ref));
        ref.motive = build.source;
        ref.has_step = true;
        ref.step = spec->liquidation;
        result = add_node_motive(&build, slot++, 1, "m2", &ref, &nodes[3]);
    }
    if (result == PLAN_OK) {
        memset(&ref, 0, sizeof(ref));
        ref.motive = build.source;
        ref.has_step = true;
        ref.step = spec->repetition;
        result = add_node_motive(&build, slot++, 1, "m3", &ref, &nodes[4]);
    }
    Rational at;
    if (result == PLAN_OK) {
        result = plan_result_of_rational(
            rational_add(build.material_span.start, position_times(request.theme.bar, slot++), &at));
    }
    if (result == PLAN_OK) {
        result = motive_attach_as(plan, build.material, at, &spec->head, "main theme head", PLAN_STATE_REALIZED, &nodes[5], NULL);
    }
    if (result == PLAN_OK) {
        result = plan_result_of_rational(
            rational_add(build.material_span.start, position_times(request.theme.bar, slot++), &at));
    }
    if (result == PLAN_OK) {
        result = motive_attach_as(plan, build.material, at, &spec->tone, "final tone", PLAN_STATE_REALIZED, &nodes[6], NULL);
    }

    /* The phrases, each with its chords, bass and cadence; the ideas in them with their melodies. */
    NodeId phrases[PHRASES];
    for (size_t p = 0; result == PLAN_OK && p < PHRASES; p++) {
        PlanHarmonyShape shape;
        memset(&shape, 0, sizeof(shape));
        if (p == 0) {
            shape.kind = PLAN_HARMONY_SHAPE_PHRASE_MODEL;
            shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
            shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
        } else {
            shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
        }
        result = plan_add_node(plan, request.theme.section, PLAN_NODE_PHRASE, spec->phrase_span[p], "phrase", &phrases[p]);
        if (result == PLAN_OK) {
            result = plan_node_set_state(plan, phrases[p], PLAN_STATE_REALIZED);
        }
        if (result == PLAN_OK) {
            result = plan_node_add_constraint(plan, phrases[p], &build.cadence);
        }
        if (result == PLAN_OK) {
            result = grow_add_chord_targets(&build, phrases[p], &shape, &build.cadence);
        }
    }
    for (size_t i = 0; result == PLAN_OK && i < IDEAS; i++) {
        NodeId idea;
        memset(&ref, 0, sizeof(ref));
        ref.motive = nodes[i];
        result = plan_add_node(plan, phrases[IDEA_PHRASE[i]], PLAN_NODE_IDEA, spec->idea_span[i], IDEA_NAMES[i], &idea);
        if (result == PLAN_OK) {
            result = plan_node_set_state(plan, idea, PLAN_STATE_REALIZED);
        }
        if (result != PLAN_OK) {
            break;
        }
        const bool last = i + 1 == IDEAS || IDEA_PHRASE[i + 1] != IDEA_PHRASE[i];
        if (i == 5) {
            /* The head is written as it is: a melody target is made again under the cadence's rules and would not keep m0's
               intervals, which are the hint of the main theme. */
            result = grow_add_melody_written(&build, idea, &ref, &spec->head);
        } else {
            result = grow_add_melody_target(&build, idea, &ref, last ? &build.cadence : NULL);
        }
    }
    return result;
}
