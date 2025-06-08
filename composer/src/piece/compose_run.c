#include <ursatz_composer/compose.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <phrase_level_form.h>

#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/texture_check.h>
#include <ursatz_composer/voicing.h>

#include "grow/grow_internal.h"
#include "plan/plan_internal.h"
#include "style/style_internal.h"

/* How many times the melody stage is run before it is given up on, and how far apart the seeds of the runs are. */
#define COMPOSE_MELODY_ATTEMPTS 6
#define COMPOSE_ATTEMPT_STRIDE 1000UL
/*
 * Where in its phrase the melody's climax is asked for, by attempt (thousandths). The first attempt keeps what the
 * configuration says; a piece with a theme other than a sentence, whose climax can fall on an idea that cannot carry
 * it (a restated idea, an idea that ends in the cadence's tone and leaves no note above the rest), is written again
 * with the climax elsewhere.
 */
static const unsigned CLIMAX_RETRIES[COMPOSE_MELODY_ATTEMPTS] = {0, 250, 833, 400, 950, 560};
/*
 * How many times the returns are made before a reharmonized return, whose chords the harmony operators write without
 * the melody it keeps, is given up on, and how far apart the seeds of the attempts are.
 */
#define COMPOSE_RETURN_ATTEMPTS 12
#define COMPOSE_RETURN_STRIDE 10000UL
/* How many runs, from the plan as given, before a stuck run is given up on, and how far apart their seeds are. */
#define COMPOSE_RUN_ROUNDS 12
#define COMPOSE_ROUND_STRIDE 100000UL

const char *compose_stage_name(ComposeStage stage) {
    static const char *const NAMES[COMPOSE_STAGE_COUNT] = {"invent", "structure", "motives", "harmony",
                                                           "bass",   "melody",    "texture", "returns"};
    return stage >= 0 && stage < COMPOSE_STAGE_COUNT ? NAMES[stage] : "unknown";
}

/*
 * What a stage's seed adds to the configuration's: grow's stages keep grow's seeds (0 to 5), invention comes after them
 * (6), then the returns (7) and the stages that write what the returns reopened, run again (8 to 11).
 */
static unsigned long stage_seed(ComposeStage stage, bool again) {
    if (again) {
        return (unsigned long)(COMPOSE_STAGE_TEXTURE - COMPOSE_STAGE_STRUCTURE + 3 + (stage - COMPOSE_STAGE_HARMONY));
    }
    if (stage == COMPOSE_STAGE_INVENT) {
        return (unsigned long)(COMPOSE_STAGE_TEXTURE - COMPOSE_STAGE_STRUCTURE + 1);
    }
    if (stage == COMPOSE_STAGE_RETURNS) {
        return (unsigned long)(COMPOSE_STAGE_TEXTURE - COMPOSE_STAGE_STRUCTURE + 2);
    }
    return (unsigned long)(stage - COMPOSE_STAGE_STRUCTURE);
}

void compose_rejections_free(ComposeRejection *rejections, size_t count) {
    for (size_t i = 0; rejections && i < count; i++) {
        free(rejections[i].items);
    }
    free(rejections);
}

void compose_outcome_destroy(ComposeOutcome *outcome) {
    if (!outcome) {
        return;
    }
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        fill_record_destroy(outcome->records[stage]);
    }
    for (size_t i = 0; outcome->sections && i < outcome->section_count; i++) {
        form_heard_destroy(&outcome->sections[i].heard);
    }
    free(outcome->sections);
    return_heard_destroy(&outcome->returns);
    for (size_t i = 0; outcome->analyzer_checks && i < outcome->analyzer_check_count; i++) {
        analyzer_check_destroy(&outcome->analyzer_checks[i]);
    }
    free(outcome->analyzer_checks);
    if (outcome->style_measured) {
        style_comparison_destroy(&outcome->style_distance);
    }
    compose_rejections_free(outcome->rejections, outcome->rejection_count);
    memset(outcome, 0, sizeof(*outcome));
}

/* ------------------------------------------------------------- the plan */

static bool is_theme_section(const PiecePlan *plan, NodeId id) {
    bool archetype = false;
    bool pairing = false;
    if (plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
        return false;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        PlanConstraintKind kind = plan_node_constraint_at(plan, id, i)->kind;
        archetype = archetype || kind == PLAN_CONSTRAINT_ARCHETYPE;
        pairing = pairing || kind == PLAN_CONSTRAINT_THEME_PAIRING;
    }
    return archetype && pairing;
}

/* Whether a lane span in the section (a lane span reopened by plan_regenerate) is still open. */
static bool has_open_span(const PiecePlan *plan, NodeId section) {
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED &&
            plan_section_of(plan, plan_lane_span_owner(plan, id)) == section) {
            return true;
        }
    }
    return false;
}

/*
 * An open theme section: one not yet grown (Constrained), or one grown whose lane spans are partly open again. Given
 * sections are never open.
 */
static bool is_open_theme(const PiecePlan *plan, NodeId id) {
    PlanState state = plan_node_state(plan, id);
    return is_theme_section(plan, id) &&
           (state == PLAN_STATE_CONSTRAINED || (state == PLAN_STATE_REALIZED && has_open_span(plan, id)));
}

/* The open theme sections, in node order, into a new array; false when out of memory. */
static bool find_themes(const PiecePlan *plan, NodeId **out, size_t *count) {
    size_t found = 0;
    *out = NULL;
    *count = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        found += is_open_theme(plan, id);
    }
    if (found == 0) {
        return true;
    }
    *out = calloc(found, sizeof(NodeId));
    if (!*out) {
        return false;
    }
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (is_open_theme(plan, id)) {
            (*out)[(*count)++] = id;
        }
    }
    return true;
}

/* The spans of a lane and voice over span, into a new array; NULL with *count 0 when there are none. */
static LaneSpanId *find_spans(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, size_t *count) {
    *count = plan_lane_find(plan, lane, voice, span, NULL, 0);
    LaneSpanId *ids = *count ? calloc(*count, sizeof(LaneSpanId)) : NULL;
    if (ids) {
        plan_lane_find(plan, lane, voice, span, ids, *count);
    } else {
        *count = 0;
    }
    return ids;
}

/* Whether every lane span of the lane and voice over the span is written; with required, there must be one. */
static bool lane_written(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, bool required) {
    size_t count;
    LaneSpanId *ids = find_spans(plan, lane, voice, span, &count);
    bool written = !required || count > 0;
    for (size_t i = 0; written && i < count; i++) {
        written = plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED;
    }
    free(ids);
    return written;
}

/* Whether the section asks for a voicing that writes the soprano, which is then the melody. */
static bool soprano_voiced(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_VOICING && constraint->params.voicing.write_soprano) {
            return true;
        }
    }
    return false;
}

static bool has_archetype(const PiecePlan *plan, NodeId section, PlanArchetype archetype) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE && constraint->params.archetype == archetype) {
            return true;
        }
    }
    return false;
}

/* Whether a melody span that motive-invent is to write is still open. */
static bool invention_open(const PiecePlan *plan) {
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_lane_span_lane(plan, id) != PLAN_LANE_MELODY ||
            plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED) {
            continue;
        }
        for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
            if (plan_lane_span_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_INVENT_MOTIVE) {
                return true;
            }
        }
    }
    return false;
}

/* The melody of one section is written (a voiced soprano is the texture stage's). */
static bool melody_done(const PiecePlan *plan, NodeId section) {
    return soprano_voiced(plan, section) ||
           lane_written(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, section), true);
}

/* What a stage was to do, read from the plan: its own targets are all filled, in every section. */
static bool stage_done(const PiecePlan *plan, ComposeStage stage, const NodeId *themes, size_t theme_count) {
    if (stage == COMPOSE_STAGE_INVENT) {
        return !invention_open(plan);
    }
    if (stage == COMPOSE_STAGE_MOTIVES) {
        for (NodeId id = 1; id <= plan_node_count(plan); id++) {
            if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && plan_node_state(plan, id) == PLAN_STATE_CONSTRAINED) {
                return false;
            }
        }
        return true;
    }
    if (stage == COMPOSE_STAGE_RETURNS) {
        for (size_t t = 0; t < theme_count; t++) {
            if (form_return_handles(plan, themes[t])) {
                return false;
            }
        }
        return true;
    }
    for (size_t t = 0; t < theme_count; t++) {
        NodeId section = themes[t];
        PlanSpan span = plan_node_span(plan, section);
        bool done = false;
        /* A section that returns an earlier one has nothing yet: the returns stage gives it its material. */
        if (form_return_handles(plan, section)) {
            continue;
        }
        switch (stage) {
        case COMPOSE_STAGE_STRUCTURE:
            done = plan_node_state(plan, section) != PLAN_STATE_CONSTRAINED;
            break;
        case COMPOSE_STAGE_HARMONY:
            done = lane_written(plan, PLAN_LANE_HARMONY, 0, span, true);
            break;
        case COMPOSE_STAGE_BASS:
            done = lane_written(plan, PLAN_LANE_BASS, 0, span, true);
            break;
        case COMPOSE_STAGE_MELODY:
            done = melody_done(plan, section);
            break;
        case COMPOSE_STAGE_TEXTURE:
            /* No inner voice span over the section is still open (there may be none: nothing was asked for). */
            done = (!soprano_voiced(plan, section) || lane_written(plan, PLAN_LANE_MELODY, 0, span, true));
            for (int voice = 1; done && voice <= 3; voice++) {
                done = lane_written(plan, PLAN_LANE_INNER_VOICE, voice, span, false);
            }
            break;
        default:
            break;
        }
        if (!done) {
            return false;
        }
    }
    return true;
}

static FillResult register_stage(FillOperatorRegistry *registry, ComposeStage stage, const ComposeConfig *config,
                                 const MotiveDevelopContext *motives, const MelodyFillParams *melody,
                                 const AccompanimentParams *accompaniment, const VoicingParams *voicing) {
    FillResult result = FILL_OK;
    switch (stage) {
    case COMPOSE_STAGE_INVENT:
        return motive_invent_register(registry, motives);
    case COMPOSE_STAGE_STRUCTURE:
        return grow_register(registry, &config->context);
    case COMPOSE_STAGE_MOTIVES:
        return motive_develop_register(registry, motives);
    case COMPOSE_STAGE_HARMONY:
        /* The harmony operators write chords that stand on a Given bass, carry a Given melody and join Given chords. */
        result = harmony_prolong_register_given(registry);
        return result == FILL_OK ? harmony_phrase_register_given(registry) : result;
    case COMPOSE_STAGE_BASS:
        return harmony_bass_register(registry, NULL);
    case COMPOSE_STAGE_MELODY:
        return melody_fill_register(registry, melody);
    case COMPOSE_STAGE_TEXTURE:
        result = accompaniment_register(registry, accompaniment);
        return result == FILL_OK ? voicing_register(registry, voicing) : result;
    case COMPOSE_STAGE_RETURNS:
        return form_return_register(registry, melody);
    case COMPOSE_STAGE_COUNT:
        break;
    }
    return FILL_ERR_BAD_OPERATOR;
}

static FillCheckerFn checker_of(ComposeStage stage) {
    switch (stage) {
    case COMPOSE_STAGE_INVENT:
        return motive_invent_checker;
    case COMPOSE_STAGE_STRUCTURE:
        return grow_checker;
    case COMPOSE_STAGE_MOTIVES:
        return motive_develop_checker;
    case COMPOSE_STAGE_HARMONY:
        return harmony_prolong_checker;
    case COMPOSE_STAGE_BASS:
        return harmony_phrase_checker;
    case COMPOSE_STAGE_MELODY:
        return melody_fill_checker;
    case COMPOSE_STAGE_TEXTURE:
        return texture_checker;
    case COMPOSE_STAGE_RETURNS:
        return form_return_checker;
    case COMPOSE_STAGE_COUNT:
        break;
    }
    return NULL;
}

/* ------------------------------------------------------------- hearing */

static int form_of(PlanArchetype archetype) {
    switch (archetype) {
    case PLAN_ARCHETYPE_SENTENCE:
        return PHRASE_LEVEL_FORM_SENTENCE;
    case PLAN_ARCHETYPE_PERIOD:
        return PHRASE_LEVEL_FORM_PERIOD;
    case PLAN_ARCHETYPE_HYBRID:
        return PHRASE_LEVEL_FORM_HYBRID;
    }
    return -1;
}

/* The score grow-theme's checker gave the structure it applied to the section. */
static bool structure_score(const FillRecord *record, NodeId section, GrowScore *out) {
    for (size_t s = 0; record && s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome == FILL_STEP_APPLIED && step->chosen >= 0 && step->target.kind == FILL_TARGET_NODE &&
            step->target.id == section) {
            out->numerator = step->considered[step->chosen].score_numerator;
            out->denominator =
                step->considered[step->chosen].score_denominator ? step->considered[step->chosen].score_denominator : 1;
            return true;
        }
    }
    return false;
}

/* Hears one finished section. */
static void hear_section(const PiecePlan *plan, NodeId section, const FillRecord *structure, ComposeHeard *out) {
    PlanSpan span = plan_node_span(plan, section);
    out->section = section;
    out->heard_form = -1;
    out->requested_form = -1;
    out->requested_beginning = -1;
    out->requested_ending = -1;
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE) {
            out->requested_form = form_of(constraint->params.archetype);
        } else if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            out->requested_beginning = grow_phrase_beginning(constraint->params.theme_pairing.beginning);
            out->requested_ending = grow_phrase_ending(constraint->params.theme_pairing.ending);
        }
    }
    out->heard_result = form_hear(plan, span, &out->heard);
    if (out->heard_result == FORM_HEAR_OK && out->heard.count > 0) {
        out->heard_form = out->heard.themes[0].form;
    }
    /* A hybrid is heard as requested only with the beginning and the ending asked for. */
    out->heard_as_requested = out->heard_form >= 0 && out->heard_form == out->requested_form &&
                              (out->requested_form != PHRASE_LEVEL_FORM_HYBRID ||
                               (out->heard.themes[0].beginning == out->requested_beginning &&
                                out->heard.themes[0].ending == out->requested_ending));
    if (!structure_score(structure, section, &out->structure)) {
        out->structure.numerator = 0;
        out->structure.denominator = 1;
    }
}

/* The section a theme section returns, if it has a RETURN constraint. */
static NodeId returned_section(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_RETURN) {
            return constraint->params.section_return.section;
        }
    }
    return PLAN_NODE_NONE;
}

/* Hears the returns the plan asks for. A return that cannot be heard counts as not heard. */
static void hear_returns(const PiecePlan *plan, const NodeId *themes, size_t theme_count, ComposeOutcome *outcome) {
    bool heard_ok = false;
    for (size_t t = 0; t < theme_count; t++) {
        outcome->returns_planned += returned_section(plan, themes[t]) != PLAN_NODE_NONE;
    }
    if (outcome->returns_planned > 0) {
        heard_ok = return_hear(plan, &outcome->returns) == RETURN_HEAR_OK;
    }
    for (size_t t = 0; heard_ok && t < theme_count; t++) {
        NodeId source = returned_section(plan, themes[t]);
        outcome->returns_heard += source != PLAN_NODE_NONE && return_heard_find(&outcome->returns, source, themes[t]);
    }
    outcome->returns_heard_as_planned = outcome->returns_heard == outcome->returns_planned;
}

static void hear(const PiecePlan *plan, const NodeId *themes, size_t theme_count, ComposeOutcome *outcome) {
    unsigned long numerator = 0;
    unsigned long denominator = 0;
    for (size_t t = 0; t < theme_count; t++) {
        ComposeHeard *heard = &outcome->sections[t];
        hear_section(plan, themes[t], outcome->records[COMPOSE_STAGE_STRUCTURE], heard);
        outcome->heard_as_requested += heard->heard_as_requested;
        numerator += heard->structure.numerator + (heard->heard_as_requested ? 1 : 0);
        denominator += heard->structure.denominator + 1;
    }
    hear_returns(plan, themes, theme_count, outcome);
    numerator += outcome->returns_heard;
    denominator += outcome->returns_planned;
    outcome->score.numerator = numerator;
    outcome->score.denominator = denominator ? denominator : 1;
}

/* ------------------------------------------------------------- running */

/*
 * Puts the melody of every section the stage left unwritten back to open, so it can be run again: the spans that were
 * open when the stage began (open_at_start), not the ones written before it, which a regenerated plan holds (R-072).
 */
static bool reopen_melody(PiecePlan *plan, const NodeId *themes, size_t theme_count, const LaneSpanId *open_at_start,
                          size_t open_count) {
    for (size_t t = 0; t < theme_count; t++) {
        if (melody_done(plan, themes[t])) {
            continue;
        }
        size_t count;
        LaneSpanId *ids = find_spans(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, themes[t]), &count);
        for (size_t i = 0; i < count; i++) {
            bool was_open = false;
            for (size_t k = 0; k < open_count && !was_open; k++) {
                was_open = open_at_start[k] == ids[i];
            }
            if (was_open && plan_lane_span_set_state(plan, ids[i], PLAN_STATE_CONSTRAINED) != PLAN_OK) {
                free(ids);
                return false;
            }
        }
        free(ids);
    }
    return true;
}

/* The melody spans that are open now, into a new array (NULL when there are none). */
static LaneSpanId *open_melody_spans(const PiecePlan *plan, size_t *count) {
    size_t found = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        found += plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == PLAN_LANE_MELODY &&
                 plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED;
    }
    LaneSpanId *ids = found ? calloc(found, sizeof(LaneSpanId)) : NULL;
    *count = 0;
    for (LaneSpanId id = 1; ids && id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == PLAN_LANE_MELODY &&
            plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED) {
            ids[(*count)++] = id;
        }
    }
    return ids;
}

static void outcome_reset(ComposeOutcome *outcome) {
    compose_outcome_destroy(outcome);
}

/* What the stages of one run share. */
typedef struct {
    PiecePlan *plan;
    const ComposeConfig *config;
    unsigned long offset;
    const NodeId *themes;
    size_t theme_count;
    bool sentences;
    size_t attempt; /* the attempt at the returns, which moves the seeds of the stages they run */
    MotiveDevelopContext motives;
    AccompanimentParams accompaniment;
    VoicingParams voicing;
    ComposeOutcome *outcome;
} Run;

/*
 * One stage. again marks a stage run a second time, after the returns: it writes what a return reopened, with seeds
 * of its own, and keeps no record.
 */
static PieceResult run_stage(Run *run, ComposeStage stage, bool again) {
    PiecePlan *plan = run->plan;
    const ComposeConfig *config = run->config;
    ComposeOutcome *outcome = run->outcome;
    /* The melody is written idea by idea and a later idea can find the ones before it leave it no way to the
       climax, so a melody that gets stuck is written again, from the next seed, a few times. */
    size_t attempts = stage == COMPOSE_STAGE_MELODY ? COMPOSE_MELODY_ATTEMPTS : 1;
    size_t open_count = 0;
    LaneSpanId *open_at_start = stage == COMPOSE_STAGE_MELODY ? open_melody_spans(plan, &open_count) : NULL;
    for (size_t attempt = 0; attempt < attempts; attempt++) {
        FillOperatorRegistry *registry = fill_registry_create();
        FillConfig fill;
        FillOutcome fill_outcome;
        if (!registry) {
            free(open_at_start);
            return PIECE_ERR_ALLOC;
        }
        memset(&fill, 0, sizeof(fill));
        fill.seed = config->seed + run->offset + stage_seed(stage, again) + COMPOSE_ATTEMPT_STRIDE * (unsigned long)attempt +
                    COMPOSE_RETURN_STRIDE * (unsigned long)run->attempt;
        fill.max_iterations = config->max_iterations;
        fill.checker = checker_of(stage);
        fill.checker_context =
            stage == COMPOSE_STAGE_MOTIVES || stage == COMPOSE_STAGE_INVENT ? (void *)&run->motives : NULL;
        /* With a style, the harmony and melody checkers also prefer what is closer to it, and the structure checker the
           form (where the plan leaves it open). */
        StyleCheckContext style_context;
        if (config->style &&
            (stage == COMPOSE_STAGE_STRUCTURE || stage == COMPOSE_STAGE_HARMONY || stage == COMPOSE_STAGE_MELODY)) {
            style_context.inner = fill.checker;
            style_context.style = config->style;
            style_context.weight = config->style_weight;
            if (style_context.weight.denominator == 0) {
                style_context.weight.numerator = 1;
                style_context.weight.denominator = 1;
            }
            fill.checker = style_stage_checker;
            fill.checker_context = &style_context;
        }
        FillRecord **record = again ? NULL : &outcome->records[stage];
        if (record) {
            fill_record_destroy(*record);
            *record = NULL;
        }
        MelodyFillParams melody = config->melody ? *config->melody : melody_fill_params_default();
        if (attempt > 0 && !run->sentences) {
            melody.climax_position = CLIMAX_RETRIES[attempt];
        }
        FillResult registered = register_stage(registry, stage, config, &run->motives, &melody, &run->accompaniment,
                                               &run->voicing);
        FillResult ran = registered == FILL_OK ? fill_run(plan, registry, &fill, &fill_outcome, record) : registered;
        fill_registry_destroy(registry);
        if (ran != FILL_OK) {
            free(open_at_start);
            return ran == FILL_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_STUCK;
        }
        fill_outcome_destroy(&fill_outcome);
        if (stage_done(plan, stage, run->themes, run->theme_count)) {
            free(open_at_start);
            return PIECE_OK;
        }
        if (attempt + 1 == attempts ||
            !reopen_melody(plan, run->themes, run->theme_count, open_at_start, open_count)) {
            outcome->stuck = true;
            outcome->stuck_stage = stage;
            free(open_at_start);
            return PIECE_ERR_STUCK;
        }
    }
    free(open_at_start);
    return PIECE_OK;
}

/* Whether a theme section is still to be given the material of the one it returns. */
static bool returns_pending(const PiecePlan *plan, const NodeId *themes, size_t theme_count) {
    for (size_t t = 0; t < theme_count; t++) {
        if (form_return_handles(plan, themes[t])) {
            return true;
        }
    }
    return false;
}

/* The theme sections to be returned with a reharmonization. */
static size_t reharmonized(const PiecePlan *plan, const NodeId *themes, size_t theme_count, NodeId *out) {
    size_t count = 0;
    for (size_t t = 0; t < theme_count; t++) {
        PlanReturnVariation variation;
        if (form_return_source(plan, themes[t], NULL, &variation) && variation == PLAN_RETURN_REHARMONIZE) {
            out[count++] = themes[t];
        }
    }
    return count;
}

/*
 * The returns, then the stages that write what they reopened. The chords of a reharmonization are the harmony
 * operators', which know nothing of the melody the return keeps: when a strong-beat note of it is not a tone of the
 * chord now under it, or (in the first half of the attempts) no chord differs from the returned one, the returns are
 * made again from the plan as it was, with other seeds, a few times.
 */
static PieceResult run_returns(Run *run) {
    PiecePlan *plan = run->plan;
    NodeId *pending = calloc(run->theme_count + 1, sizeof(NodeId));
    if (!pending) {
        return PIECE_ERR_ALLOC;
    }
    size_t pending_count = reharmonized(plan, run->themes, run->theme_count, pending);
    char *snapshot = pending_count ? plan_to_json(plan, NULL) : NULL;
    size_t attempts = pending_count ? COMPOSE_RETURN_ATTEMPTS : 1;
    PieceResult result = pending_count && !snapshot ? PIECE_ERR_ALLOC : PIECE_OK;
    for (size_t attempt = 0; result == PIECE_OK && attempt < attempts; attempt++) {
        if (attempt > 0) {
            PiecePlan *fresh = NULL;
            if (plan_from_json(snapshot, &fresh, NULL) != PLAN_OK) {
                result = PIECE_ERR_ALLOC;
                break;
            }
            plan_replace_contents(plan, fresh);
        }
        run->attempt = attempt;
        run->outcome->stuck = false;
        result = run_stage(run, COMPOSE_STAGE_RETURNS, false);
        for (ComposeStage stage = COMPOSE_STAGE_HARMONY; result == PIECE_OK && stage <= COMPOSE_STAGE_TEXTURE; stage++) {
            result = run_stage(run, stage, true);
        }
        /* The melody fits the chords, and (for the first half of the attempts) the chords are not all the returned ones. */
        bool fits = true;
        for (size_t i = 0; result == PIECE_OK && i < pending_count; i++) {
            size_t changed = 0;
            size_t total = 0;
            fits = fits && form_return_melody_fits(plan, pending[i]);
            if (attempt < attempts / 2) {
                fits = fits && form_return_chords_changed(plan, pending[i], &changed, &total) && changed > 0;
            }
        }
        if (result == PIECE_OK && !fits) {
            run->outcome->stuck = true;
            run->outcome->stuck_stage = COMPOSE_STAGE_RETURNS;
            result = PIECE_ERR_STUCK;
        }
        if (result == PIECE_OK || result == PIECE_ERR_ALLOC) {
            break;
        }
        if (attempt + 1 < attempts) {
            result = PIECE_OK; /* another attempt */
            continue;
        }
    }
    free(snapshot);
    free(pending);
    return result;
}

/* One run of the stages. offset is added to every seed. */
static PieceResult run_once(PiecePlan *plan, const ComposeConfig *config, unsigned long offset,
                            ComposeOutcome *outcome) {
    NodeId *themes;
    size_t theme_count;
    outcome_reset(outcome);
    if (!find_themes(plan, &themes, &theme_count)) {
        return PIECE_ERR_ALLOC;
    }
    if (theme_count == 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    outcome->sections = calloc(theme_count, sizeof(ComposeHeard));
    if (!outcome->sections) {
        free(themes);
        return PIECE_ERR_ALLOC;
    }
    outcome->section_count = theme_count;
    for (size_t t = 0; t < theme_count; t++) {
        outcome->sections[t].section = themes[t];
        outcome->sections[t].heard_result = FORM_HEAR_ERR_NOT_WRITTEN;
        outcome->sections[t].heard_form = -1;
        outcome->sections[t].requested_form = -1;
        outcome->sections[t].requested_beginning = -1;
        outcome->sections[t].requested_ending = -1;
        outcome->sections[t].structure.denominator = 1;
    }
    Run run;
    memset(&run, 0, sizeof(run));
    run.plan = plan;
    run.config = config;
    run.offset = offset;
    run.themes = themes;
    run.theme_count = theme_count;
    run.outcome = outcome;
    /* A piece of sentences keeps the climax it is asked for: their fragments always have room for one. */
    run.sentences = true;
    for (size_t t = 0; t < theme_count; t++) {
        run.sentences = run.sentences && has_archetype(plan, themes[t], PLAN_ARCHETYPE_SENTENCE);
    }
    /* Fragments of one note are allowed: grow-theme shortens a fragment to fit its idea. */
    motive_develop_context_default(&run.motives);
    run.motives.min_length = 1;
    run.accompaniment = accompaniment_params_default();
    run.voicing = voicing_params_default();
    PieceResult result = PIECE_OK;

    for (ComposeStage stage = COMPOSE_STAGE_INVENT; result == PIECE_OK && stage <= COMPOSE_STAGE_TEXTURE; stage++) {
        if (stage == COMPOSE_STAGE_INVENT && !invention_open(plan)) {
            continue;
        }
        result = run_stage(&run, stage, false);
    }
    /* The sections that return an earlier one are given its material now it is written, then what they reopened is. */
    if (result == PIECE_OK && returns_pending(plan, themes, theme_count)) {
        result = run_returns(&run);
    }
    if (result == PIECE_OK) {
        hear(plan, themes, theme_count, outcome);
    }
    free(themes);
    return result;
}

/* Whether the plan changes key: its key lane has more than one span. */
static bool has_key_changes(const PiecePlan *plan) {
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    return plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, NULL, 0) > 1;
}

/* The checks of a plan with several keys; PIECE_ERR_STUCK, marked in the outcome, when it fails them. */
static PieceResult check_keys(const PiecePlan *plan, ComposeOutcome *outcome) {
    bool read = piece_check_keys(plan, &outcome->key_check) && piece_check_cadences(plan, &outcome->cadence_check);
    if (read && outcome->key_check.ok && outcome->cadence_check.ok) {
        return PIECE_OK;
    }
    if (!read) {
        outcome->key_check.ok = false;
        snprintf(outcome->key_check.reason, sizeof(outcome->key_check.reason), "the keys could not be read");
    }
    outcome->stuck = true;
    outcome->stuck_stage = COMPOSE_STAGE_TEXTURE;
    outcome->key_rejected = true;
    return PIECE_ERR_STUCK;
}

/*
 * The Analyzer's check of every theme section (R-080). PIECE_ERR_STUCK, marked in the outcome, when some section's
 * analysis contradicts the plan; the score takes in what was planned and what was heard. A section the check cannot read
 * is left out of it.
 */
static PieceResult check_analyzer(const PiecePlan *plan, ComposeOutcome *outcome) {
    outcome->analyzer_checks = calloc(outcome->section_count ? outcome->section_count : 1, sizeof(AnalyzerCheck));
    if (!outcome->analyzer_checks) {
        return PIECE_ERR_ALLOC;
    }
    outcome->analyzer_check_count = outcome->section_count;
    bool contradicts = false;
    for (size_t i = 0; i < outcome->section_count; i++) {
        AnalyzerCheck *check = &outcome->analyzer_checks[i];
        AnalyzerCheckResult result = analyzer_check_section(plan, outcome->sections[i].section, check);
        if (result == ANALYZER_CHECK_ERR_ALLOC) {
            return PIECE_ERR_ALLOC;
        }
        if (result != ANALYZER_CHECK_OK) {
            continue;
        }
        outcome->analyzer_planned += check->planned;
        outcome->analyzer_agreed += check->agreed;
        contradicts = contradicts || check->contradicts;
    }
    outcome->score.numerator += outcome->analyzer_agreed;
    outcome->score.denominator += outcome->analyzer_planned;
    if (!contradicts) {
        return PIECE_OK;
    }
    outcome->stuck = true;
    outcome->stuck_stage = COMPOSE_STAGE_TEXTURE;
    outcome->analyzer_rejected = true;
    return PIECE_ERR_STUCK;
}

/*
 * The Analyzer's measure of how far the finished plan is from the style. A plan the Analyzer cannot read is left
 * unmeasured: the measure is a report, it never rejects a run.
 */
static PieceResult measure_style(const PiecePlan *plan, const ComposeConfig *config, ComposeOutcome *outcome) {
    ComposerMeasureResult measured = composer_style_measure(plan, config->style, &outcome->style_distance);
    if (measured == COMPOSER_MEASURE_ERR_ALLOC) {
        return PIECE_ERR_ALLOC;
    }
    outcome->style_measured = measured == COMPOSER_MEASURE_OK;
    return PIECE_OK;
}

/*
 * Keeps the run that just ended stuck as a rejection, with the reason the run's own marks give; a failed allocation
 * leaves the run out of the list. A fill error that marked no stage (outcome->stuck false) has an empty stage.
 */
static void add_rejection(ComposeRejection **list, size_t *count, const PiecePlan *plan, const ComposeOutcome *outcome,
                          size_t round, unsigned long seed) {
    ComposeRejection *grown = realloc(*list, (*count + 1) * sizeof(ComposeRejection));
    if (!grown) {
        return;
    }
    *list = grown;
    ComposeRejection *rejection = &grown[*count];
    memset(rejection, 0, sizeof(*rejection));
    rejection->round = round;
    rejection->seed = seed;
    snprintf(rejection->stage, sizeof(rejection->stage), "%s", outcome->stuck ? compose_stage_name(outcome->stuck_stage) : "");
    if (outcome->analyzer_rejected) {
        snprintf(rejection->reason, sizeof(rejection->reason), "analyzer");
        for (size_t s = 0; s < outcome->analyzer_check_count; s++) {
            const AnalyzerCheck *check = &outcome->analyzer_checks[s];
            for (size_t i = 0; i < check->item_count; i++) {
                if (check->items[i].agreed) {
                    continue;
                }
                ComposeRejectionItem *items = realloc(rejection->items, (rejection->item_count + 1) * sizeof(*items));
                if (!items) {
                    break;
                }
                rejection->items = items;
                memset(&items[rejection->item_count], 0, sizeof(*items));
                const char *name = s < outcome->section_count ? plan_node_name(plan, outcome->sections[s].section) : NULL;
                snprintf(items[rejection->item_count].section, sizeof(items[0].section), "%s", name ? name : "");
                items[rejection->item_count].item = check->items[i];
                rejection->item_count++;
            }
        }
    } else if (outcome->key_rejected) {
        bool keys = !outcome->key_check.ok;
        snprintf(rejection->reason, sizeof(rejection->reason), "%s", keys ? "key" : "cadence");
        snprintf(rejection->detail, sizeof(rejection->detail), "%s",
                 keys ? outcome->key_check.reason : outcome->cadence_check.reason);
    } else {
        snprintf(rejection->reason, sizeof(rejection->reason), "stuck");
    }
    (*count)++;
}

PieceResult compose_run(PiecePlan *plan, const ComposeConfig *config, ComposeOutcome *outcome) {
    if (!plan || !config || !outcome) {
        return PIECE_ERR_NULL;
    }
    memset(outcome, 0, sizeof(*outcome));
    /*
     * A run can be stuck for a reason no later choice can mend: a melody that no chord progression written before
     * it lets fit. The plan as it was given is kept, and a run that is stuck starts again from it with other seeds,
     * a few times; the first run is always the one with the seeds the configuration names.
     */
    char *snapshot = plan_to_json(plan, NULL);
    PieceResult result = PIECE_ERR_ALLOC;
    ComposeRejection *rejections = NULL;
    size_t rejection_count = 0;
    for (size_t round = 0; snapshot && round < COMPOSE_RUN_ROUNDS; round++) {
        if (round > 0) {
            PiecePlan *fresh = NULL;
            if (plan_from_json(snapshot, &fresh, NULL) != PLAN_OK) {
                result = PIECE_ERR_ALLOC;
                break;
            }
            plan_replace_contents(plan, fresh);
        }
        result = run_once(plan, config, COMPOSE_ROUND_STRIDE * (unsigned long)round, outcome);
        if (result == PIECE_OK && has_key_changes(plan)) {
            result = check_keys(plan, outcome);
        }
        if (result == PIECE_OK && config->analyzer_check) {
            result = check_analyzer(plan, outcome);
        }
        if (result == PIECE_OK && config->style) {
            result = measure_style(plan, config, outcome);
        }
        if (result != PIECE_ERR_STUCK) {
            break;
        }
        add_rejection(&rejections, &rejection_count, plan, outcome, round,
                      config->seed + COMPOSE_ROUND_STRIDE * (unsigned long)round);
    }
    free(snapshot);
    /* Kept apart until now: each run starts by clearing the outcome. */
    outcome->rejections = rejections;
    outcome->rejection_count = rejection_count;
    return result;
}
