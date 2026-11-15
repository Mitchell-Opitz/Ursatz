#ifndef URSATZ_COMPOSER_EXPLAIN_H
#define URSATZ_COMPOSER_EXPLAIN_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/rejection.h>

/*
 * The explanation of a run (R-080): why each candidate was chosen or rejected, put together from what a run already
 * keeps and changing none of it.
 *
 *   - the fill records, per stage: for every target the loop worked on, the candidate it chose, up to three runners-up
 *     with how far each scored behind, and every candidate a checker rejected with the checker's own reason;
 *   - the style term of each score (FillConsidered.style_*), so a score can be read as rules plus style;
 *   - per theme section, the analyzer check items (analyzer_check.h) and the form the section was heard as;
 *   - the whole runs that were thrown away (ComposeOutcome.rejections) and why.
 *
 * Reasons are the checkers' own strings; nothing is put into words beyond them.
 *
 * An Explanation owns everything in it (copies), so it outlives the outcome it was built from.
 */

#define EXPLAIN_RUNNERS_UP 3
#define EXPLAIN_NAME_MAX 64

typedef struct {
    char operator_name[FILL_NAME_MAX];
    char label[FILL_LABEL_MAX];
    unsigned long score_numerator;   /* the whole score, style term included */
    unsigned long score_denominator;
    bool styled;                     /* a style was set; then the term below is real */
    unsigned long style_numerator;   /* the style term in the score, 0/1 with no style */
    unsigned long style_denominator;
    bool rejected;
    char reason[FILL_REASON_MAX];
    /* A runner-up: how far its score is behind the chosen candidate's (0/1 for a tie); 0/1 for the others. */
    unsigned long behind_numerator;
    unsigned long behind_denominator;
} ExplainCandidate;

typedef struct {
    char stage[16];            /* compose_stage_name, or grow_stage_name for a grow */
    size_t step;               /* index into the stage's record: a target can be tried again after another succeeded */
    FillTargetKind kind;
    size_t id;                 /* a NodeId or a LaneSpanId, by kind */
    NodeId section;            /* the section the target is in, PLAN_NODE_NONE for none */
    char name[EXPLAIN_NAME_MAX];     /* the node's name, or its owner's for a lane span */
    char letter[PLAN_SECTION_LETTER_MAX]; /* the section's form letter, "" for none */
    char lane[16];             /* "harmony", "bass", "melody", ... for a lane span, "" for a node */
    int voice;                 /* an inner voice's number, otherwise 0 */
    size_t first_bar;          /* 1-based from the start of the piece, 0 when the plan has no meter; the last bar the target ends in */
    size_t last_bar;
    char pickup[16];           /* the pickup the target's melody span starts with, "n/d" whole notes; "" for none */
    FillStepOutcome outcome;
    size_t considered;         /* how many candidates the step weighed */
    bool has_chosen;
    ExplainCandidate chosen;   /* the candidate applied, when has_chosen */
    ExplainCandidate runners_up[EXPLAIN_RUNNERS_UP]; /* the best of those not chosen and not rejected, by score, then as proposed */
    size_t runner_up_count;
    ExplainCandidate *rejected; /* owned; every rejected candidate, in the order proposed */
    size_t rejected_count;
} ExplainTarget;

typedef struct {
    NodeId section;
    char name[EXPLAIN_NAME_MAX];
    char letter[PLAN_SECTION_LETTER_MAX];
    bool checked;              /* the Analyzer check read the section */
    AnalyzerCheck check;       /* owned; empty when not checked */
    bool heard;                /* the form was heard */
    char heard_form[32];       /* the phrase-level form heard first, "none" when no theme was heard */
    char requested_form[32];   /* "none" when the section asked for none */
    bool heard_as_requested;
    char development[16];      /* a theme's development setting ("melody", "balanced", "theme"), "" for none (the caller sets it) */
} ExplainSection;

typedef struct {
    ExplainTarget *targets;    /* owned; stage by stage, in the order the loop worked */
    size_t target_count;
    ExplainSection *sections;  /* owned; in plan order */
    size_t section_count;
    ComposeRejection *rejections; /* owned copies */
    size_t rejection_count;
} Explanation;

typedef enum {
    EXPLAIN_OK = 0,
    EXPLAIN_ERR_NULL,
    EXPLAIN_ERR_ALLOC
} ExplainResult;

/*
 * Builds the explanation of a compose run on plan (the plan compose_run filled; the outcome is not changed). Stages
 * that did not run have no records and add nothing. EXPLAIN_ERR_NULL for a NULL argument.
 */
ExplainResult explain_build(const PiecePlan *plan, const ComposeOutcome *outcome, Explanation *out);

/*
 * The same for a grow: the stages of its records, the one theme section `theme`, what it was heard as, its analyzer
 * check (NULL when it was not made) and the rejections to show (the grow's own, GrowOutcome.rejections, are added to
 * the ones given; extra may be NULL when extra_count is 0).
 */
ExplainResult explain_build_grow(const PiecePlan *plan, NodeId theme, const GrowOutcome *outcome,
                                 const AnalyzerCheck *check, const ComposeRejection *extra, size_t extra_count,
                                 Explanation *out);

/* A rejection for a run an analyzer check threw away, from that check of `section`: reason "analyzer", its items that disagree. */
ExplainResult explain_analyzer_rejection(const PiecePlan *plan, NodeId section, const AnalyzerCheck *check, size_t round,
                                         unsigned long seed, ComposeRejection *out);

/* Frees everything and zeroes the struct. Safe on a zeroed struct. */
void explain_destroy(Explanation *explanation);

/*
 * The text for reading: one block per section (its name and letter, what it was heard as, the check items), each with
 * the targets in it by stage, then the targets in no section, then the rejected runs. A heap string the caller frees,
 * NULL on failure.
 */
char *explain_to_text(const Explanation *explanation);

/* The JSON, schema "explanation", version 1. A heap string the caller frees, NULL on failure. */
char *explain_to_json(const Explanation *explanation);

#endif
