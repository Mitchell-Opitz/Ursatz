#ifndef URSATZ_COMPOSER_ANALYZER_CHECK_H
#define URSATZ_COMPOSER_ANALYZER_CHECK_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * The Analyzer check (R-080): a realized section is rendered, analyzed in memory by the Analyzer's composer API
 * (analyzer_analyze_fragment) and what the Analyzer hears is compared with what the plan asked for.
 *
 * The section's span is rendered with every lane (plan_render_range), its events measured from the section's start,
 * and analyzed as a fragment in the section's meter and in the key the plan has at the section's start. Then:
 *   - Cadence: every planned cadence, the CADENCE constraints of the section (the first is the one it ends in, a second
 *     is the weak one an antecedent ends in, halfway), must be heard arriving in its bar. A planned PAC or IAC is heard
 *     as planned when the Analyzer hears any authentic cadence there; any other type must be heard as that type.
 *   - Key: the first key region heard is the key the plan has at the section's start (tonic and mode).
 *   - Violations: a hard voice-leading fault (parallel fifths or octaves) that involves a note the plan wrote, one not
 *     in a Given span, contradicts the plan. A fault between Given notes is the user's own.
 * The cadences and the key are the "planned" items; "agreed" counts those heard as planned. contradicts is true when any
 * cadence or the key is not heard as planned, or a hard violation is found.
 */

typedef enum {
    ANALYZER_CHECK_CADENCE,
    ANALYZER_CHECK_KEY,
    ANALYZER_CHECK_VIOLATION
} AnalyzerCheckKind;

typedef struct {
    AnalyzerCheckKind kind;
    size_t bar;       /* 0-based from the section's start; for a key 0 */
    char planned[32]; /* "perfect-authentic", "C major", ... ; "none" for a violation */
    char heard[32];   /* what the Analyzer heard there, "none" when nothing; a violation's rule id */
    bool agreed;
} AnalyzerCheckItem;

typedef struct {
    bool contradicts;
    size_t planned;          /* the cadences and the key the plan asks for */
    size_t agreed;           /* of them, those heard as planned */
    AnalyzerCheckItem *items; /* owned; planned items in order, then violations */
    size_t item_count;
} AnalyzerCheck;

typedef enum {
    ANALYZER_CHECK_OK = 0,
    ANALYZER_CHECK_ERR_NULL,
    ANALYZER_CHECK_ERR_ALLOC,
    ANALYZER_CHECK_ERR_NOT_WRITTEN, /* a lane of the section is still Constrained, or it holds no notes */
    ANALYZER_CHECK_ERR_PLAN,        /* no meter or key over the section, or a length that is not whole bars */
    ANALYZER_CHECK_ERR_ANALYSIS     /* the Analyzer could not analyze the fragment */
} AnalyzerCheckResult;

const char *analyzer_check_result_name(AnalyzerCheckResult result);

/* Checks the section into *out, which the caller destroys. On failure *out is empty and safe to destroy. */
AnalyzerCheckResult analyzer_check_section(const PiecePlan *plan, NodeId section, AnalyzerCheck *out);

/* Frees everything and zeroes the struct. Safe on a zeroed struct. */
void analyzer_check_destroy(AnalyzerCheck *check);

#endif
