#ifndef URSATZ_COMPOSER_COMPOSE_CANDIDATES_H
#define URSATZ_COMPOSER_COMPOSE_CANDIDATES_H

#include <ursatz_composer/compose.h>

/*
 * Candidates (R-080): compose_run on fresh copies of a plan with the seeds seed, seed + 1, ... (count of them), the
 * complete runs kept. Rejected and stuck runs are not candidates. They are ranked
 *   1. by agreement with the Analyzer, agreed over planned (1 when nothing was planned, as when analyzer_check is off),
 *   2. by the compose score,
 *   3. by the lower seed,
 * so the best is first and the same inputs always give the same order. Each keeps its own plan, outcome and seed.
 */
typedef struct {
    Plan *plan;       /* owned */
    ComposeOutcome outcome; /* owned */
    unsigned long seed;
    /* What the ranking used, kept apart so it can be listed after the plan and outcome were taken. */
    size_t agreed;
    size_t planned;
    GrowScore score;
} ComposeCandidate;

typedef struct {
    ComposeCandidate *items; /* owned; best first; NULL when none completed */
    size_t count;
    size_t tried;            /* the runs made: count given */
} ComposeCandidates;

void compose_candidates_destroy(ComposeCandidates *candidates);

/*
 * Runs count candidates (1 or more) from plan, which is not changed. PIECE_OK with candidates->count possibly 0 when no
 * seed completed; PIECE_ERR_BAD_REQUEST for a plan with no open theme section; PIECE_ERR_NULL and PIECE_ERR_ALLOC.
 */
PieceResult compose_candidates(const Plan *plan, const ComposeConfig *config, size_t count,
                               ComposeCandidates *candidates);

#endif
