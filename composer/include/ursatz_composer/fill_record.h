#ifndef URSATZ_COMPOSER_FILL_RECORD_H
#define URSATZ_COMPOSER_FILL_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "generation/generation_record.h"

#include <ursatz_composer/fill_loop.h>

/*
 * The record of a fill run. It wraps Ursatz's GenerationRecord, which holds the
 * run-level facts (seed, version), and adds the step log Ursatz's type has no
 * place for: for every attempt, the target, the candidates considered with their
 * scores and rejections, which one was chosen, and the random state around it.
 *
 * With the same starting plan, operators and seed, the same record comes out.
 * The step log is the provenance for every choice in the plan.
 *
 * It is written as JSON (fill_record_to_json) to be kept next to the plan; the
 * plan's own format is unchanged.
 */

#define FILL_NAME_MAX 64

typedef struct {
    size_t operator_index;
    char operator_name[FILL_NAME_MAX];
    char operator_version[FILL_NAME_MAX];
    FillOperatorKind kind;
    char label[FILL_LABEL_MAX];
    unsigned long score_numerator;
    unsigned long score_denominator;
    bool rejected;
    char reason[FILL_REASON_MAX];
} FillConsidered;

typedef enum {
    FILL_STEP_APPLIED,
    FILL_STEP_NO_OPERATOR,
    FILL_STEP_ALL_REJECTED
} FillStepOutcome;

typedef struct {
    FillTarget target;
    FillStepOutcome outcome;
    FillConsidered *considered; /* owned; every candidate, rejected or not, in the order proposed */
    size_t considered_count;
    long chosen;                /* index into considered, or -1 when nothing was chosen */
    uint64_t rng_state_before;  /* at the start of the attempt */
    uint64_t rng_state_after;   /* at its end */
} FillStep;

struct FillRecord {
    GenerationRecord *generation; /* owned; the seed and the Composer version */
    unsigned long seed;
    FillStatus status;
    size_t iterations;
    FillStep *steps; /* owned; one per attempt */
    size_t step_count;
};

void fill_record_destroy(FillRecord *record);

/*
 * The record as JSON (schema_version 1): seed and states as decimal strings,
 * then the steps. Returns a heap string the caller frees, or NULL on failure.
 */
char *fill_record_to_json(const FillRecord *record);

#endif
