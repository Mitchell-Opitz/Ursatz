#include <ursatz_composer/compose.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_json.h>

void compose_candidates_destroy(ComposeCandidates *candidates) {
    if (!candidates) {
        return;
    }
    for (size_t i = 0; candidates->items && i < candidates->count; i++) {
        plan_destroy(candidates->items[i].plan);
        compose_outcome_destroy(&candidates->items[i].outcome);
    }
    free(candidates->items);
    memset(candidates, 0, sizeof(*candidates));
}

/* Whether a ranks before b: more agreement with the Analyzer, then a better score, then the lower seed. */
static bool ranks_before(const ComposeCandidate *a, const ComposeCandidate *b) {
    /* agreed / planned, 1 when nothing was planned, compared without division. */
    unsigned long long a_planned = a->planned ? a->planned : 1;
    unsigned long long a_agreed = a->planned ? a->agreed : 1;
    unsigned long long b_planned = b->planned ? b->planned : 1;
    unsigned long long b_agreed = b->planned ? b->agreed : 1;
    unsigned long long by_agreement_a = a_agreed * b_planned;
    unsigned long long by_agreement_b = b_agreed * a_planned;
    if (by_agreement_a != by_agreement_b) {
        return by_agreement_a > by_agreement_b;
    }
    unsigned long long by_score_a = (unsigned long long)a->score.numerator * b->score.denominator;
    unsigned long long by_score_b = (unsigned long long)b->score.numerator * a->score.denominator;
    if (by_score_a != by_score_b) {
        return by_score_a > by_score_b;
    }
    return a->seed < b->seed;
}

PieceResult compose_candidates(const PiecePlan *plan, const ComposeConfig *config, size_t count,
                               ComposeCandidates *candidates) {
    if (!candidates) {
        return PIECE_ERR_NULL;
    }
    memset(candidates, 0, sizeof(*candidates));
    if (!plan || !config || count == 0) {
        return PIECE_ERR_NULL;
    }
    char *text = plan_to_json(plan, NULL);
    candidates->items = calloc(count, sizeof(ComposeCandidate));
    if (!text || !candidates->items) {
        free(text);
        compose_candidates_destroy(candidates);
        return PIECE_ERR_ALLOC;
    }
    PieceResult status = PIECE_OK;
    for (size_t i = 0; i < count && status == PIECE_OK; i++) {
        ComposeCandidate *candidate = &candidates->items[candidates->count];
        ComposeConfig run = *config;
        run.seed = config->seed + i;
        candidate->seed = run.seed;
        if (plan_from_json(text, &candidate->plan, NULL) != PLAN_OK) {
            status = PIECE_ERR_ALLOC;
            break;
        }
        PieceResult ran = compose_run(candidate->plan, &run, &candidate->outcome);
        bool cancelled = candidate->outcome.cancelled;
        candidates->tried++;
        if (ran == PIECE_OK) {
            candidate->agreed = candidate->outcome.analyzer_agreed;
            candidate->planned = candidate->outcome.analyzer_planned;
            candidate->score = candidate->outcome.score;
            candidates->count++;
        } else {
            plan_destroy(candidate->plan);
            compose_outcome_destroy(&candidate->outcome);
            memset(candidate, 0, sizeof(*candidate));
            if (ran != PIECE_ERR_STUCK) {
                status = ran;
            } else if (cancelled) {
                status = PIECE_ERR_STUCK; /* no further candidate is tried */
            }
        }
    }
    free(text);
    if (status != PIECE_OK) {
        compose_candidates_destroy(candidates);
        return status;
    }
    /* An insertion sort keeps the order of equals stable; the seed already settles ties. */
    for (size_t i = 1; i < candidates->count; i++) {
        ComposeCandidate moving = candidates->items[i];
        size_t position = i;
        while (position > 0 && ranks_before(&moving, &candidates->items[position - 1])) {
            candidates->items[position] = candidates->items[position - 1];
            position--;
        }
        candidates->items[position] = moving;
    }
    if (candidates->count == 0) {
        free(candidates->items);
        candidates->items = NULL;
    }
    return PIECE_OK;
}
