#ifndef URSATZ_COMPOSER_ANALYZER_CHECK_H
#define URSATZ_COMPOSER_ANALYZER_CHECK_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * The Analyzer check (R-080): a realized section is heard by the Analyzer's composer API (analyzer_hear) and what the
 * Analyzer hears is compared with what the plan asked for.
 *
 * The plan is rendered once with every lane (plan_render); the notes that start in the section's span are heard in the
 * plan's meter map and the key the plan has at the section's start, and the cadences, chords and key regions are read from
 * the result's report (analyzer_result_report). Bars are counted from the section's start. Then:
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

/*
 * The whole piece (R-172, R-173): every section of a piece structure plan (a section with a piece part; not the seam or
 * the material), heard as one span (from the plan's start to the end of its last section that is not material) in the plan's first
 * meter and key. A plan version that has sections not on whole bars, a pickup or any length of beats is read as it is.
 *   - Cadence: the first CADENCE constraint of each section is its planned cadence. It is heard as planned when an
 *     Analyzer cadence of that type ends within one beat of the section's end, a cadence ending where the chord of its
 *     arrival gives way to the next (the end of the piece for the last). A planned PAC or IAC is heard by any authentic
 *     cadence, as in a section.
 *   - Key: the first key region heard is the key the plan has at its start.
 *     A planned HC leads on, so it is also heard when a cadence arrives within a beat of the section's end (the dominant
 *     resolving into what follows), and at the end of the piece, where nothing follows to resolve into.
 *     A cadence also counts when its arrival begins in the section's last bar (a tonic that carries on into the next
 *     section ends later than a beat after it).
 *   - Violations: a hard voice-leading fault that involves a note the plan wrote (not in a Given span) is listed as an
 *     item, as in a section, but does not make the piece contradict the plan: the harmony, bass and melody checkers do not
 *     yet keep parallels out of the short intro and turnaround, and a whole piece would otherwise pass almost no seed.
 * The items' bar is counted from the start of the piece. ANALYZER_CHECK_ERR_NOT_WRITTEN for a lane left open before the
 * piece's end. analyzer_check_section stays as it was for the other commands.
 */
AnalyzerCheckResult analyzer_check_piece(const PiecePlan *plan, AnalyzerCheck *out);

/* Frees everything and zeroes the struct. Safe on a zeroed struct. */
void analyzer_check_destroy(AnalyzerCheck *check);

#endif
