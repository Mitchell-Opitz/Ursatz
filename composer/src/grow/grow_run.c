#include <ursatz_composer/grow.h>

#include <string.h>

#include <ursatz_composer/compose.h>

#include "grow/grow_internal.h"

void grow_outcome_destroy(GrowOutcome *outcome) {
    if (!outcome) {
        return;
    }
    for (int stage = 0; stage < GROW_STAGE_COUNT; stage++) {
        fill_record_destroy(outcome->records[stage]);
    }
    form_heard_destroy(&outcome->heard);
    if (outcome->style_measured) {
        style_comparison_destroy(&outcome->style_distance);
    }
    memset(outcome, 0, sizeof(*outcome));
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

static GrowResult from_piece(PieceResult result) {
    switch (result) {
    case PIECE_OK:
        return GROW_OK;
    case PIECE_ERR_NULL:
        return GROW_ERR_NULL;
    case PIECE_ERR_ALLOC:
        return GROW_ERR_ALLOC;
    case PIECE_ERR_STUCK:
        return GROW_ERR_STUCK;
    case PIECE_ERR_MOTIVE_TOO_LONG:
        return GROW_ERR_MOTIVE_TOO_LONG;
    case PIECE_ERR_BAD_REQUEST:
    case PIECE_ERR_BAD_SIZE:
        break;
    }
    return GROW_ERR_BAD_REQUEST;
}

/*
 * Growing is a compose run over the plan's one theme section (compose.h): its stages from structure on are grow's, with
 * grow's seeds, and the plan has m0 Given, so there is nothing to invent. The outcome takes the records of those
 * stages and what the theme was heard as.
 */
GrowResult grow_run(PiecePlan *plan, const GrowConfig *config, GrowOutcome *outcome) {
    if (!plan || !config || !outcome) {
        return GROW_ERR_NULL;
    }
    memset(outcome, 0, sizeof(*outcome));
    outcome->heard_result = FORM_HEAR_ERR_NOT_WRITTEN;
    outcome->heard_form = -1;
    outcome->requested_form = -1;
    if (grow_find_theme(plan, true) == PLAN_NODE_NONE) {
        return GROW_ERR_BAD_REQUEST;
    }
    ComposeConfig compose;
    ComposeOutcome composed;
    memset(&compose, 0, sizeof(compose));
    compose.seed = config->seed;
    compose.context = config->context;
    compose.melody = config->melody;
    compose.max_iterations = config->max_iterations;
    compose.style = config->style;
    compose.style_weight = config->style_weight;
    GrowResult result = from_piece(compose_run(plan, &compose, &composed));

    for (int stage = 0; stage < GROW_STAGE_COUNT; stage++) {
        ComposeStage from = (ComposeStage)(COMPOSE_STAGE_STRUCTURE + stage);
        outcome->records[stage] = composed.records[from];
        composed.records[from] = NULL;
    }
    outcome->stuck = composed.stuck;
    outcome->stuck_stage = composed.stuck ? (GrowStage)(composed.stuck_stage - COMPOSE_STAGE_STRUCTURE) : 0;
    if (result == GROW_OK && composed.section_count > 0) {
        ComposeHeard *heard = &composed.sections[0];
        outcome->heard_result = heard->heard_result;
        outcome->heard = heard->heard;
        memset(&heard->heard, 0, sizeof(heard->heard));
        outcome->heard_form = heard->heard_form;
        outcome->requested_form = heard->requested_form;
        outcome->requested_beginning = heard->requested_beginning;
        outcome->requested_ending = heard->requested_ending;
        outcome->heard_as_requested = heard->heard_as_requested;
        outcome->score = composed.score;
        outcome->style_measured = composed.style_measured;
        outcome->style_distance = composed.style_distance;
        memset(&composed.style_distance, 0, sizeof(composed.style_distance));
        composed.style_measured = false;
    }
    compose_outcome_destroy(&composed);
    return result;
}
