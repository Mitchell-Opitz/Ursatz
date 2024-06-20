#include <ursatz_composer/grow.h>

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
#define GROW_MELODY_ATTEMPTS 6
#define GROW_ATTEMPT_STRIDE 1000UL
/*
 * Where in its phrase the melody's climax is asked for, by attempt (thousandths). The first attempt keeps what the
 * configuration says; a theme other than a sentence whose climax falls on an idea that cannot carry it (a restated
 * idea, an idea that ends in the cadence's tone and leaves no note above the rest) is written again with the climax
 * elsewhere.
 */
static const unsigned CLIMAX_RETRIES[GROW_MELODY_ATTEMPTS] = {0, 250, 833, 400, 950, 560};
/* How many runs, from the plan as given, before a stuck run is given up on, and how far apart their seeds are. */
#define GROW_RUN_ROUNDS 12
#define GROW_ROUND_STRIDE 100000UL

void grow_outcome_destroy(GrowOutcome *outcome) {
    if (!outcome) {
        return;
    }
    for (int stage = 0; stage < GROW_STAGE_COUNT; stage++) {
        fill_record_destroy(outcome->records[stage]);
    }
    form_heard_destroy(&outcome->heard);
    memset(outcome, 0, sizeof(*outcome));
}

/* Whether every lane span of the lane over the span is written (and there is one). */
static bool lane_written(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span) {
    LaneSpanId ids[64];
    size_t count = plan_lane_find(plan, lane, 0, span, ids, 64);
    for (size_t i = 0; i < count && i < 64; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
    }
    return count > 0;
}

/* Whether the theme asks for a voicing that writes the soprano, which is then the melody. */
static bool soprano_voiced(const PiecePlan *plan, NodeId theme) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_VOICING && constraint->params.voicing.write_soprano) {
            return true;
        }
    }
    return false;
}

/* What a stage was to do, read from the plan: its own targets are all filled. */
static bool stage_done(const PiecePlan *plan, GrowStage stage, NodeId theme, PlanSpan span) {
    switch (stage) {
    case GROW_STAGE_STRUCTURE:
        return plan_node_state(plan, theme) != PLAN_STATE_CONSTRAINED;
    case GROW_STAGE_MOTIVES:
        for (NodeId id = 1; id <= plan_node_count(plan); id++) {
            if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && plan_node_state(plan, id) == PLAN_STATE_CONSTRAINED) {
                return false;
            }
        }
        return true;
    case GROW_STAGE_HARMONY:
        return lane_written(plan, PLAN_LANE_HARMONY, span);
    case GROW_STAGE_BASS:
        return lane_written(plan, PLAN_LANE_BASS, span);
    case GROW_STAGE_MELODY:
        /* A voiced soprano is written by the texture stage. */
        return soprano_voiced(plan, theme) || lane_written(plan, PLAN_LANE_MELODY, span);
    case GROW_STAGE_TEXTURE: {
        /* No inner voice span over the theme is still open (there may be none: nothing was asked for). */
        LaneSpanId ids[64];
        for (int voice = 1; voice <= 3; voice++) {
            size_t count = plan_lane_find(plan, PLAN_LANE_INNER_VOICE, voice, span, ids, 64);
            for (size_t i = 0; i < count && i < 64; i++) {
                if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
                    return false;
                }
            }
        }
        return !soprano_voiced(plan, theme) || lane_written(plan, PLAN_LANE_MELODY, span);
    }
    case GROW_STAGE_COUNT:
        break;
    }
    return false;
}

static FillResult register_stage(FillOperatorRegistry *registry, GrowStage stage, const GrowConfig *config,
                                 const MotiveDevelopContext *motives, const MelodyFillParams *melody,
                                 const AccompanimentParams *accompaniment,
                                 const VoicingParams *voicing) {
    FillResult result = FILL_OK;
    switch (stage) {
    case GROW_STAGE_STRUCTURE:
        return grow_register(registry, &config->context);
    case GROW_STAGE_MOTIVES:
        return motive_develop_register(registry, motives);
    case GROW_STAGE_HARMONY:
        result = harmony_prolong_register(registry);
        return result == FILL_OK ? harmony_phrase_register(registry) : result;
    case GROW_STAGE_BASS:
        return harmony_bass_register(registry, NULL);
    case GROW_STAGE_MELODY:
        return melody_fill_register(registry, melody);
    case GROW_STAGE_TEXTURE:
        result = accompaniment_register(registry, accompaniment);
        return result == FILL_OK ? voicing_register(registry, voicing) : result;
    case GROW_STAGE_COUNT:
        break;
    }
    return FILL_ERR_BAD_OPERATOR;
}

static FillCheckerFn checker_of(GrowStage stage) {
    switch (stage) {
    case GROW_STAGE_STRUCTURE:
        return grow_checker;
    case GROW_STAGE_MOTIVES:
        return motive_develop_checker;
    case GROW_STAGE_HARMONY:
        return harmony_prolong_checker;
    case GROW_STAGE_BASS:
        return harmony_phrase_checker;
    case GROW_STAGE_MELODY:
        return melody_fill_checker;
    case GROW_STAGE_TEXTURE:
        return texture_checker;
    case GROW_STAGE_COUNT:
        break;
    }
    return NULL;
}

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

bool grow_record_score(const FillRecord *record, GrowScore *out) {
    for (size_t s = 0; record && s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome == FILL_STEP_APPLIED && step->chosen >= 0) {
            out->numerator = step->considered[step->chosen].score_numerator;
            out->denominator = step->considered[step->chosen].score_denominator ? step->considered[step->chosen].score_denominator : 1;
            return true;
        }
    }
    return false;
}

/* Hears the finished theme and adds the heard form to the score. */
static void hear(const PiecePlan *plan, NodeId theme, PlanSpan span, GrowOutcome *outcome) {
    GrowScore structure = {0, 1};
    outcome->heard_form = -1;
    outcome->requested_form = -1;
    outcome->requested_beginning = -1;
    outcome->requested_ending = -1;
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE) {
            outcome->requested_form = form_of(constraint->params.archetype);
        } else if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            outcome->requested_beginning = grow_phrase_beginning(constraint->params.theme_pairing.beginning);
            outcome->requested_ending = grow_phrase_ending(constraint->params.theme_pairing.ending);
        }
    }
    outcome->heard_result = form_hear(plan, span, &outcome->heard);
    if (outcome->heard_result == FORM_HEAR_OK && outcome->heard.count > 0) {
        outcome->heard_form = outcome->heard.themes[0].form;
    }
    /* A hybrid is heard as requested only with the beginning and the ending asked for. */
    outcome->heard_as_requested = outcome->heard_form >= 0 && outcome->heard_form == outcome->requested_form &&
                                  (outcome->requested_form != PHRASE_LEVEL_FORM_HYBRID ||
                                   (outcome->heard.themes[0].beginning == outcome->requested_beginning &&
                                    outcome->heard.themes[0].ending == outcome->requested_ending));
    if (!grow_record_score(outcome->records[GROW_STAGE_STRUCTURE], &structure)) {
        structure.numerator = 0;
        structure.denominator = 1;
    }
    outcome->score.numerator = structure.numerator + (outcome->heard_as_requested ? 1 : 0);
    outcome->score.denominator = structure.denominator + 1;
}

/* Puts the melody back to open, so the stage can be run again: its spans keep their notes, which the next run replaces. */
static bool reopen_melody(PiecePlan *plan, PlanSpan span) {
    LaneSpanId ids[64];
    size_t count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 64);
    for (size_t i = 0; i < count && i < 64; i++) {
        if (plan_lane_span_set_state(plan, ids[i], PLAN_STATE_CONSTRAINED) != PLAN_OK) {
            return false;
        }
    }
    return true;
}

/* One run of the stages. offset is added to every seed. */
static GrowResult run_once(PiecePlan *plan, const GrowConfig *config, unsigned long offset, GrowOutcome *outcome) {
    memset(outcome, 0, sizeof(*outcome));
    outcome->heard_result = FORM_HEAR_ERR_NOT_WRITTEN;
    outcome->heard_form = -1;
    outcome->requested_form = -1;
    NodeId theme = grow_find_theme(plan, true);
    if (theme == PLAN_NODE_NONE) {
        return GROW_ERR_BAD_REQUEST;
    }
    PlanSpan span = plan_node_span(plan, theme);
    /* A sentence keeps the climax it is asked for: its fragments always have room for one. */
    bool sentence = false;
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        sentence = sentence || (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE &&
                                constraint->params.archetype == PLAN_ARCHETYPE_SENTENCE);
    }
    /* Fragments of one note are allowed: grow-theme shortens a fragment to fit its idea. */
    MotiveDevelopContext motives;
    motive_develop_context_default(&motives);
    motives.min_length = 1;
    AccompanimentParams accompaniment = accompaniment_params_default();
    VoicingParams voicing = voicing_params_default();

    for (GrowStage stage = GROW_STAGE_STRUCTURE; stage < GROW_STAGE_COUNT; stage++) {
        /* The melody is written idea by idea and a later idea can find the ones before it leave it no way to the
           climax, so a melody that gets stuck is written again, from the next seed, a few times. */
        size_t attempts = stage == GROW_STAGE_MELODY ? GROW_MELODY_ATTEMPTS : 1;
        for (size_t attempt = 0; attempt < attempts; attempt++) {
            FillOperatorRegistry *registry = fill_registry_create();
            FillConfig fill;
            FillOutcome fill_outcome;
            memset(&fill, 0, sizeof(fill));
            fill.seed = config->seed + offset + (unsigned long)stage + GROW_ATTEMPT_STRIDE * (unsigned long)attempt;
            fill.max_iterations = config->max_iterations;
            fill.checker = checker_of(stage);
            fill.checker_context = stage == GROW_STAGE_MOTIVES ? (void *)&motives : NULL;
            if (!registry) {
                return GROW_ERR_ALLOC;
            }
            fill_record_destroy(outcome->records[stage]);
            outcome->records[stage] = NULL;
            MelodyFillParams melody = config->melody ? *config->melody : melody_fill_params_default();
            if (attempt > 0 && !sentence) {
                melody.climax_position = CLIMAX_RETRIES[attempt];
            }
            FillResult registered = register_stage(registry, stage, config, &motives, &melody, &accompaniment, &voicing);
            FillResult ran = registered == FILL_OK
                                 ? fill_run(plan, registry, &fill, &fill_outcome, &outcome->records[stage])
                                 : registered;
            fill_registry_destroy(registry);
            if (ran != FILL_OK) {
                return ran == FILL_ERR_ALLOC ? GROW_ERR_ALLOC : GROW_ERR_STUCK;
            }
            fill_outcome_destroy(&fill_outcome);
            if (stage_done(plan, stage, theme, span)) {
                break;
            }
            if (attempt + 1 == attempts || !reopen_melody(plan, span)) {
                outcome->stuck = true;
                outcome->stuck_stage = stage;
                return GROW_ERR_STUCK;
            }
        }
    }
    hear(plan, theme, span, outcome);
    return GROW_OK;
}

GrowResult grow_run(PiecePlan *plan, const GrowConfig *config, GrowOutcome *outcome) {
    if (!plan || !config || !outcome) {
        return GROW_ERR_NULL;
    }
    /*
     * A run can be stuck for a reason no later choice can mend: a melody that no chord progression written before
     * it lets fit. The plan as it was given is kept, and a run that is stuck starts again from it with other seeds,
     * a few times; the first run is always the one with the seeds the configuration names.
     */
    char *snapshot = plan_to_json(plan, NULL);
    GrowResult result = GROW_ERR_ALLOC;
    for (size_t round = 0; snapshot && round < GROW_RUN_ROUNDS; round++) {
        if (round > 0) {
            PiecePlan *fresh = NULL;
            grow_outcome_destroy(outcome);
            if (plan_from_json(snapshot, &fresh, NULL) != PLAN_OK) {
                result = GROW_ERR_ALLOC;
                break;
            }
            plan_replace_contents(plan, fresh);
        }
        result = run_once(plan, config, GROW_ROUND_STRIDE * (unsigned long)round, outcome);
        if (result != GROW_ERR_STUCK) {
            break;
        }
    }
    free(snapshot);
    return result;
}
