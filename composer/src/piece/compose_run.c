#include <ursatz_composer/compose.h>

#include <stdlib.h>
#include <string.h>

#include <phrase_level_form.h>

#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_prolong.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/texture_check.h>
#include <ursatz_composer/voicing.h>

#include "grow/grow_internal.h"
#include "plan/plan_internal.h"

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
/* How many runs, from the plan as given, before a stuck run is given up on, and how far apart their seeds are. */
#define COMPOSE_RUN_ROUNDS 12
#define COMPOSE_ROUND_STRIDE 100000UL

const char *compose_stage_name(ComposeStage stage) {
    static const char *const NAMES[COMPOSE_STAGE_COUNT] = {"invent", "structure", "motives", "harmony",
                                                           "bass",   "melody",    "texture"};
    return stage >= 0 && stage < COMPOSE_STAGE_COUNT ? NAMES[stage] : "unknown";
}

/* What a stage's seed adds to the configuration's: grow's stages keep grow's seeds, invention comes after them. */
static unsigned long stage_seed(ComposeStage stage) {
    return stage == COMPOSE_STAGE_INVENT ? (unsigned long)(COMPOSE_STAGE_COUNT - 1)
                                         : (unsigned long)(stage - COMPOSE_STAGE_STRUCTURE);
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

/* The open theme sections, in node order, into a new array; false when out of memory. */
static bool find_themes(const PiecePlan *plan, NodeId **out, size_t *count) {
    size_t found = 0;
    *out = NULL;
    *count = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        found += is_theme_section(plan, id) && plan_node_state(plan, id) == PLAN_STATE_CONSTRAINED;
    }
    if (found == 0) {
        return true;
    }
    *out = calloc(found, sizeof(NodeId));
    if (!*out) {
        return false;
    }
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (is_theme_section(plan, id) && plan_node_state(plan, id) == PLAN_STATE_CONSTRAINED) {
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
    for (size_t t = 0; t < theme_count; t++) {
        NodeId section = themes[t];
        PlanSpan span = plan_node_span(plan, section);
        bool done = false;
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
        result = harmony_prolong_register(registry);
        return result == FILL_OK ? harmony_phrase_register(registry) : result;
    case COMPOSE_STAGE_BASS:
        return harmony_bass_register(registry, NULL);
    case COMPOSE_STAGE_MELODY:
        return melody_fill_register(registry, melody);
    case COMPOSE_STAGE_TEXTURE:
        result = accompaniment_register(registry, accompaniment);
        return result == FILL_OK ? voicing_register(registry, voicing) : result;
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
    outcome->score.numerator = numerator;
    outcome->score.denominator = denominator ? denominator : 1;
}

/* ------------------------------------------------------------- running */

/* Puts the melody of every section the stage left unwritten back to open, so it can be run again. */
static bool reopen_melody(PiecePlan *plan, const NodeId *themes, size_t theme_count) {
    for (size_t t = 0; t < theme_count; t++) {
        if (melody_done(plan, themes[t])) {
            continue;
        }
        size_t count;
        LaneSpanId *ids = find_spans(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, themes[t]), &count);
        for (size_t i = 0; i < count; i++) {
            if (plan_lane_span_set_state(plan, ids[i], PLAN_STATE_CONSTRAINED) != PLAN_OK) {
                free(ids);
                return false;
            }
        }
        free(ids);
    }
    return true;
}

static void outcome_reset(ComposeOutcome *outcome) {
    compose_outcome_destroy(outcome);
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
    /* A piece of sentences keeps the climax it is asked for: their fragments always have room for one. */
    bool sentences = true;
    for (size_t t = 0; t < theme_count; t++) {
        sentences = sentences && has_archetype(plan, themes[t], PLAN_ARCHETYPE_SENTENCE);
    }
    /* Fragments of one note are allowed: grow-theme shortens a fragment to fit its idea. */
    MotiveDevelopContext motives;
    motive_develop_context_default(&motives);
    motives.min_length = 1;
    AccompanimentParams accompaniment = accompaniment_params_default();
    VoicingParams voicing = voicing_params_default();
    PieceResult result = PIECE_OK;

    for (ComposeStage stage = COMPOSE_STAGE_INVENT; result == PIECE_OK && stage < COMPOSE_STAGE_COUNT; stage++) {
        if (stage == COMPOSE_STAGE_INVENT && !invention_open(plan)) {
            continue;
        }
        /* The melody is written idea by idea and a later idea can find the ones before it leave it no way to the
           climax, so a melody that gets stuck is written again, from the next seed, a few times. */
        size_t attempts = stage == COMPOSE_STAGE_MELODY ? COMPOSE_MELODY_ATTEMPTS : 1;
        for (size_t attempt = 0; attempt < attempts; attempt++) {
            FillOperatorRegistry *registry = fill_registry_create();
            FillConfig fill;
            FillOutcome fill_outcome;
            if (!registry) {
                result = PIECE_ERR_ALLOC;
                break;
            }
            memset(&fill, 0, sizeof(fill));
            fill.seed = config->seed + offset + stage_seed(stage) + COMPOSE_ATTEMPT_STRIDE * (unsigned long)attempt;
            fill.max_iterations = config->max_iterations;
            fill.checker = checker_of(stage);
            fill.checker_context =
                stage == COMPOSE_STAGE_MOTIVES || stage == COMPOSE_STAGE_INVENT ? (void *)&motives : NULL;
            fill_record_destroy(outcome->records[stage]);
            outcome->records[stage] = NULL;
            MelodyFillParams melody = config->melody ? *config->melody : melody_fill_params_default();
            if (attempt > 0 && !sentences) {
                melody.climax_position = CLIMAX_RETRIES[attempt];
            }
            FillResult registered =
                register_stage(registry, stage, config, &motives, &melody, &accompaniment, &voicing);
            FillResult ran = registered == FILL_OK
                                 ? fill_run(plan, registry, &fill, &fill_outcome, &outcome->records[stage])
                                 : registered;
            fill_registry_destroy(registry);
            if (ran != FILL_OK) {
                result = ran == FILL_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_STUCK;
                break;
            }
            fill_outcome_destroy(&fill_outcome);
            if (stage_done(plan, stage, themes, theme_count)) {
                break;
            }
            if (attempt + 1 == attempts || !reopen_melody(plan, themes, theme_count)) {
                outcome->stuck = true;
                outcome->stuck_stage = stage;
                result = PIECE_ERR_STUCK;
                break;
            }
        }
    }
    if (result == PIECE_OK) {
        hear(plan, themes, theme_count, outcome);
    }
    free(themes);
    return result;
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
        if (result != PIECE_ERR_STUCK) {
            break;
        }
    }
    free(snapshot);
    return result;
}
