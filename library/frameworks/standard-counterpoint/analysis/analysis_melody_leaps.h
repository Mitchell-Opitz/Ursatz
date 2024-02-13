#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_LEAPS_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_LEAPS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_melody.h"

/*
 * Leap recovery and consecutive leaps (the melody adapter, step 2). Both are the existing cantus firmus rules
 * (cantus_firmus_rules.h LEAP_RECOVERY and CONSECUTIVE_LEAPS), applied to one melodic line; nothing is redefined here.
 *
 * LEAP RECOVERY. A leap is an interval of a third or larger. The rule asks recovery only of a leap of a FOURTH or larger: it
 * must be followed by a STEP in the OPPOSITE direction. Each leap gets one status:
 *
 *   RECOVERED     a fourth or larger, followed by a step in the opposite direction
 *   UNRECOVERED   a fourth or larger, followed by a step in the same direction, by a repeated pitch, or by a leap
 *   NOT_REQUIRED  a third: the rule asks nothing of it
 *   NOT_JUDGED    a fourth or larger with no following interval (the last note, or a rest follows): the rule judges only
 *                 leaps that have a next interval, so neither status is claimed
 *
 * The size is the interval's generic size (from spellings or scale degrees); with none, the semitones decide it and the
 * claim is APPROXIMATE (analysis_melody.h). Leap recovery is judged along the whole line, not per span.
 *
 * CONSECUTIVE LEAPS. For two leaps in a row (the second starts where the first ends) the rule forbids two in the SAME
 * direction, except two thirds that together outline a perfect fifth (a triad), and forbids three leaps in a row. One claim
 * per offending leap pair:
 *
 *   same_direction   the two leaps go the same way and are not two thirds outlining a perfect fifth (judged from the
 *                    spellings when both ends have one, else from the semitones: two thirds spanning 7 semitones)
 *   three_in_a_row   the pair is followed by a third leap
 *
 * Each claim is a Structure shell (the note ids) and an Interpretation targeting it; uncertainty is the greatest of the
 * intervals' involved.
 */

#define STANDARD_COUNTERPOINT_LEAP_RECOVERY_CLAIM_TYPE "standard-counterpoint/leap-recovery"
#define STANDARD_COUNTERPOINT_CONSECUTIVE_LEAPS_CLAIM_TYPE "standard-counterpoint/consecutive-leaps"

typedef enum {
    STANDARD_COUNTERPOINT_LEAP_RECOVERED = 0,
    STANDARD_COUNTERPOINT_LEAP_UNRECOVERED,
    STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED,
    STANDARD_COUNTERPOINT_LEAP_NOT_JUDGED
} StandardCounterpointLeapStatus;

/* "recovered" "unrecovered" "not-required" "not-judged"; NULL for an invalid status. */
const char *standard_counterpoint_leap_status_name(StandardCounterpointLeapStatus status);

typedef struct {
    int from;
    int to;
    long semitones;
    int direction; /* ContourDirection of the leap */
    int generic_size;
    int approximate;
    int status; /* StandardCounterpointLeapStatus */
    int next_index; /* the entry after the leap's second note whose interval was read; -1 when there is none */
} StandardCounterpointLeapRecoveryClaim;

typedef struct {
    StandardCounterpointLeapRecoveryClaim claim;
    double uncertainty;
    Structure *shell;               /* owned: the leap's two note ids, and the next note's */
    Interpretation *interpretation; /* owned */
} StandardCounterpointLeapRecovery;

typedef struct {
    StandardCounterpointLeapRecovery *items; /* owned; in line order; NULL when none */
    size_t count;
} StandardCounterpointLeapRecoveries;

StandardCounterpointMelodyStatus standard_counterpoint_melody_leap_recoveries(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    StandardCounterpointLeapRecoveries *out);
void standard_counterpoint_leap_recoveries_destroy(StandardCounterpointLeapRecoveries *recoveries);
bool standard_counterpoint_leap_recovery_claim_read(const Interpretation *interpretation,
                                                    StandardCounterpointLeapRecoveryClaim *out);

typedef struct {
    int first_from; /* the first leap is first_from to middle */
    int middle;
    int last_to;    /* the second leap is middle to last_to */
    int same_direction;
    int three_in_a_row;
} StandardCounterpointConsecutiveLeapsClaim;

typedef struct {
    StandardCounterpointConsecutiveLeapsClaim claim;
    double uncertainty;
    Structure *shell;
    Interpretation *interpretation;
} StandardCounterpointConsecutiveLeaps;

typedef struct {
    StandardCounterpointConsecutiveLeaps *items; /* owned; NULL when none */
    size_t count;
} StandardCounterpointConsecutiveLeapList;

StandardCounterpointMelodyStatus standard_counterpoint_melody_consecutive_leaps(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    StandardCounterpointConsecutiveLeapList *out);
void standard_counterpoint_consecutive_leap_list_destroy(StandardCounterpointConsecutiveLeapList *list);
bool standard_counterpoint_consecutive_leaps_claim_read(const Interpretation *interpretation,
                                                        StandardCounterpointConsecutiveLeapsClaim *out);

#endif
