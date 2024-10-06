#ifndef URSATZ_COMPOSER_COMPOSE_H
#define URSATZ_COMPOSER_COMPOSE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/piece_plan_keys.h>

/*
 * The compose run (R-068): every theme section of a plan filled in stages, each stage one fill_run over the whole plan
 * with only its operators registered and its own checker.
 *
 *   invent     motive-invent writes m0, when the plan asks for it (an open melody span with INVENT_MOTIVE); the stage
 *              does not run otherwise
 *   structure  grow-theme gives every theme section its nodes, its motive nodes in the material and its open lanes
 *   motives    the motive development operators write those motives (a section's own motive before the ones made
 *              from it)
 *   harmony    harmony-prolongation and harmony-phrase-model
 *   bass       harmony-bass
 *   melody     melody-fill
 *   texture    accompaniment and voicing-satb, where a section asks for one
 *   returns    form-return gives a section that returns an earlier one (a RETURN constraint, form_return.h) the material
 *              of the section it returns, once that is written; the stages before leave such a section alone. Then
 *              harmony, bass, melody and texture run again, to write what a variation reopened (REHARMONIZE). The stage
 *              does not run when no section returns an earlier one.
 *
 * The stages from structure to texture are seeded seed + 0 to seed + 5, as grow's are, and invention seed + 6, so a
 * plan with one theme section and a Given motive is filled exactly as grow_run fills it. The returns are seeded
 * seed + 7, and the harmony, bass, melody and texture run again seed + 8 to seed + 11 (their records are not kept).
 *
 * Retries are grow's (R-051): the melody stage is run up to six times, each from the next seed, reopening the melody
 * of the sections it left unwritten, the climax moved in a theme that is not a sentence; a run stuck after that starts
 * again from a JSON snapshot of the plan as given, up to twelve rounds.
 *
 * After a complete run every theme section is heard with form_hear over its own span. A plan with more than one key
 * span (R-069) is then checked: its keys are closely related to the home key and every key change a section ends in
 * is a modulation (piece_check_keys), and its cadences follow the plan's (piece_check_cadences). A run that fails is
 * rejected like a stuck one: it starts again from the plan as given, with other seeds, up to the same twelve rounds.
 */

typedef enum {
    COMPOSE_STAGE_INVENT,
    COMPOSE_STAGE_STRUCTURE,
    COMPOSE_STAGE_MOTIVES,
    COMPOSE_STAGE_HARMONY,
    COMPOSE_STAGE_BASS,
    COMPOSE_STAGE_MELODY,
    COMPOSE_STAGE_TEXTURE,
    COMPOSE_STAGE_RETURNS,
    COMPOSE_STAGE_COUNT
} ComposeStage;

/* "invent", "structure", "motives", "harmony", "bass", "melody", "texture", "returns". */
const char *compose_stage_name(ComposeStage stage);

/* All-zero is the default configuration (seed 0). The members are GrowConfig's. */
typedef struct {
    unsigned long seed;
    GrowContext context;
    const MelodyFillParams *melody; /* NULL for the defaults; must outlive compose_run */
    size_t max_iterations;          /* per stage; 0 means FILL_DEFAULT_MAX_ITERATIONS */
} ComposeConfig;

/* What one theme section was heard as. The members are GrowOutcome's. */
typedef struct {
    NodeId section;
    FormHearResult heard_result; /* FORM_HEAR_ERR_NOT_WRITTEN when the run did not complete */
    FormHeard heard;             /* owned */
    int heard_form;              /* PhraseLevelForm of the first theme heard, -1 for none */
    int requested_form;
    int requested_beginning;
    int requested_ending;
    bool heard_as_requested;
    GrowScore structure;         /* the score grow-theme's checker gave the section's structure */
} ComposeHeard;

typedef struct {
    FillRecord *records[COMPOSE_STAGE_COUNT]; /* owned; NULL for a stage that did not run */
    bool stuck;
    ComposeStage stuck_stage;
    ComposeHeard *sections;                   /* owned; one per theme section, in plan order */
    size_t section_count;
    size_t heard_as_requested;                /* how many sections were */
    bool key_rejected;                        /* the last run was complete but failed the key or cadence check (stuck) */
    PieceKeyCheck key_check;                  /* the checks of the last complete run, for a plan with several keys */
    PieceCadenceCheck cadence_check;
    /*
     * Over the sections, the structure's score and 1 for each section heard as requested: the sum of the numerators
     * over the sum of the denominators (each section's denominator one more than its structure's), as GrowOutcome's
     * score is for one.
     */
    GrowScore score;
} ComposeOutcome;

void compose_outcome_destroy(ComposeOutcome *outcome);

/*
 * Runs the stages on plan (one made by piece_build_plan or grow_build_plan). PIECE_OK with the plan complete;
 * PIECE_ERR_STUCK with the stage that failed in outcome->stuck_stage (the plan is then partly filled);
 * PIECE_ERR_BAD_REQUEST for a plan with no open theme section.
 */
PieceResult compose_run(PiecePlan *plan, const ComposeConfig *config, ComposeOutcome *outcome);

#endif
