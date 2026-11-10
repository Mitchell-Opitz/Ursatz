#ifndef URSATZ_COMPOSER_COMPOSE_H
#define URSATZ_COMPOSER_COMPOSE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/return_hear.h>
#include <ursatz_composer/seam_hear.h>
#include <ursatz_composer/style.h>

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
 *   seam       the loop's end of a piece structure, held by the stages before (a HOLD constraint, as a RETURN section is
 *              left alone), is filled last against the bar it leads into (R-171). The body's first bar is copied into the
 *              seam section (a SEAM constraint) as Given, then structure, motives, harmony, bass, melody and texture run
 *              for the held sections. The stage does not run when the plan has no seam section. Its records are kept
 *              together under the seam stage.
 *
 * The stages from structure to texture are seeded seed + 0 to seed + 5, as grow's are, and invention seed + 6, so a
 * plan with one theme section and a Given motive is filled exactly as grow_run fills it. The returns are seeded
 * seed + 7, and the harmony, bass, melody and texture run again seed + 8 to seed + 11 (their records are not kept).
 * The seam stage runs harmony, bass, melody and texture seed + 12 to seed + 15, and the structure and the motives of the
 * held sections seed + 16 and seed + 17.
 *
 * Retries are grow's (R-051): the melody stage is run up to six times, each from the next seed, reopening the melody
 * of the sections it left unwritten, the climax moved in a theme that is not a sentence; a run stuck after that starts
 * again from a JSON snapshot of the plan as given, up to twelve rounds.
 *
 * After a complete run every theme section is heard with form_hear over its own span, and the returns are heard
 * (return_hear.h): each section with a RETURN is planned to resemble the section it returns, and is heard as planned
 * when the kernel's thematic return detection finds that pair; a pair not found is reported, not rejected (R-048). A plan with more than one key
 * span (R-069) is then checked: its keys are closely related to the home key and every key change a section ends in
 * is a modulation (piece_check_keys), and its cadences follow the plan's (piece_check_cadences). A run that fails is
 * rejected like a stuck one: it starts again from the plan as given, with other seeds, up to the same twelve rounds.
 *
 * With analyzer_check (R-080) every theme section is then analyzed through the Analyzer (analyzer_check.h): a run whose
 * analysis contradicts the plan (a planned cadence not heard at its bar, the key not heard, a hard voice-leading fault
 * in material the plan wrote) is rejected the same way. A section the check cannot read (a length that is not whole
 * bars, a lane left open) is not checked. A piece structure plan (a section with a piece part) is checked whole, once,
 * with analyzer_check_piece: outcome->analyzer_checks holds that one check, and its sections need not be whole bars. The check is off by default, so a run without it is what it always was.
 *
 * With a style (R-080, style.h) the harmony and melody stages prefer the candidates closest to it: their checkers add
 * style_weight times the candidate's closeness to the style (style_labels.h, composer_style_closeness) to the score of
 * every candidate they accept, and reject nothing more (Laws 12 and 13). After a complete run the whole plan is heard
 * (analyzer_hear) and the result compared with the style (analyzer_result_style_distance): the independent
 * measure outcome->style_distance holds. Without a style a run is byte for byte what it was.
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
    COMPOSE_STAGE_SEAM,
    COMPOSE_STAGE_COUNT
} ComposeStage;

/* "invent", "structure", "motives", "harmony", "bass", "melody", "texture", "returns", "seam". */
const char *compose_stage_name(ComposeStage stage);

/* All-zero is the default configuration (seed 0). The members are GrowConfig's. */
typedef struct {
    unsigned long seed;
    GrowContext context;
    const MelodyFillParams *melody; /* NULL for the defaults; must outlive compose_run */
    bool analyzer_check;            /* reject a complete run the Analyzer's analysis contradicts (false: no check) */
    const ComposerStyle *style;     /* NULL for none; must outlive compose_run */
    ComposerRatio style_weight;     /* a denominator of 0 (all-zero) is 1/1; unused without a style */
    const Motive *basic;            /* GrowConfig.basic; NULL for none; must outlive compose_run */
    ComposerRatio tempo;            /* GrowConfig.tempo; a denominator of 0 (all-zero) is none */
    const struct GrowPoolForm *pool; /* GrowConfig.pool; NULL for none; must outlive compose_run */
    size_t pool_count;
    const GrowFidelityTable *fidelity; /* GrowConfig.fidelity; NULL for the defaults; must outlive compose_run */
    /* The theme sections heard and checked after the run (R-261); NULL is every section. A section out of scope is
       reported with ComposeHeard.in_scope false, adds nothing to the heard score and is not checked by the Analyzer. The
       returns, seam and key checks read the whole piece either way. plan_regenerate_all sets it to the sections that
       hold the targets it reopened. Must outlive compose_run. */
    const NodeId *scope;
    size_t scope_count;
    /* NULL for none. Reported once per stage (compose_stage_name, stage index, COMPOSE_STAGE_COUNT) and handed to every fill
       loop (FillConfig.job). A cancel ends the run with PIECE_ERR_STUCK and outcome->cancelled; the plan is then partly
       filled and no further round or candidate is tried. */
    const ComposerJob *job;
    /*
     * The form whose checks the run makes (form_spec.h: its `checks` name which of hearing each theme section as its
     * shape, hearing the returns, checking the keys, checking the cadences and hearing the seam run; a check not named
     * is not made, and no check changes what is written: the key check only rejects a finished run). NULL is the
     * built-in form the plan's root is named for; a plan of no built-in form, such as a theme or a fill plan, runs every
     * check it has the parts for. Must outlive compose_run.
     */
    const FormSpec *form;
} ComposeConfig;

/* What one theme section was heard as. The members are GrowOutcome's. */
typedef struct {
    NodeId section;
    /* false: the section was kept (outside ComposeConfig.scope): not heard, not checked, no heard form. */
    bool in_scope;
    /* A section of a piece structure (piece_structure.h) names its id and its part; has_part is false for any other. */
    bool has_part;
    int part;                    /* a PiecePart */
    char id[PLAN_PIECE_ID_MAX];
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
    bool cancelled;                           /* the job was cancelled (ComposeConfig.job); stuck is false */
    ComposeHeard *sections;                   /* owned; one per theme section, in plan order */
    size_t section_count;
    size_t heard_as_requested;                /* how many sections were */
    ReturnHeard returns;                      /* owned; the returns heard over the whole plan */
    size_t returns_planned;                   /* the theme sections with a RETURN, each planned to be heard as a return */
    size_t returns_heard;                     /* how many of them were */
    bool returns_heard_as_planned;            /* every return planned was heard (true when none was planned) */
    bool seam_planned;                        /* the body's first bar was copied into the seam section and the seam heard (R-171) */
    SeamHeard seam;                           /* the seam rules over the finished plan; empty without seam_planned */
    bool key_rejected;                        /* the last run was complete but failed the key or cadence check (stuck) */
    PieceKeyCheck key_check;                  /* the checks of the last complete run, for a plan with several keys */
    PieceCadenceCheck cadence_check;
    bool analyzer_rejected;                   /* the last run was complete but the Analyzer's analysis contradicted the plan */
    AnalyzerCheck *analyzer_checks;           /* owned; one per theme section, in plan order; NULL without analyzer_check */
    size_t analyzer_check_count;
    size_t analyzer_planned;                  /* over the sections, the cadences and keys planned ... */
    size_t analyzer_agreed;                   /* ... and those the Analyzer heard as planned */
    /*
     * Over the sections, the structure's score and 1 for each section heard as requested: the sum of the numerators
     * over the sum of the denominators (each section's denominator one more than its structure's), as GrowOutcome's
     * score is for one; and 1 more for each return planned, in the numerator too when it was heard; and, with a seam
     * heard, 4 more for its rules and the ones it holds in the numerator. With analyzer_check, each cadence and key planned adds 1 to the denominator and each one heard as planned adds 1 to
     * the numerator.
     */
    GrowScore score;
    /*
     * With a style, after a complete run: the whole plan analyzed and compared with the style. style_measured is false
     * when there is no style, the run is not complete, or the Analyzer could not analyze the plan; then style_distance is
     * empty. The categories the plan has no data for are listed in it with in_a false (analyzer_result_style_distance).
     */
    bool style_measured;
    StyleComparison style_distance; /* owned */
    /* Every run rejected before this one, in order, the last run included when none completed. NULL when none. */
    ComposeRejection *rejections; /* owned */
    size_t rejection_count;
} ComposeOutcome;

void compose_outcome_destroy(ComposeOutcome *outcome);

/*
 * Runs the stages on plan (one made by piece_build_plan or grow_build_plan). PIECE_OK with the plan complete;
 * PIECE_ERR_STUCK with the stage that failed in outcome->stuck_stage (the plan is then partly filled);
 * PIECE_ERR_BAD_REQUEST for a plan with no open theme section.
 */
PieceResult compose_run(PiecePlan *plan, const ComposeConfig *config, ComposeOutcome *outcome);

/*
 * Candidates (R-080): compose_run on fresh copies of a plan with the seeds seed, seed + 1, ... (count of them), the
 * complete runs kept. Rejected and stuck runs are not candidates. They are ranked
 *   1. by agreement with the Analyzer, agreed over planned (1 when nothing was planned, as when analyzer_check is off),
 *   2. by the compose score,
 *   3. by the lower seed,
 * so the best is first and the same inputs always give the same order. Each keeps its own plan, outcome and seed.
 */
typedef struct {
    PiecePlan *plan;       /* owned */
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
PieceResult compose_candidates(const PiecePlan *plan, const ComposeConfig *config, size_t count,
                               ComposeCandidates *candidates);

#endif
