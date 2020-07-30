#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_DYNAMICS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_DYNAMICS_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Dynamics rules for standard-markings-v1. Dynamics indicate loudness,
 * written as Italian words usually abbreviated:
 *
 *   LOUDNESS_ORDER   from quietest to loudest: ppp pp p mp mf f ff fff
 *   ISSIMO_STACKING  the suffix -issimo means "very" and stacks:
 *                    pianissimo (pp), pianississimo (ppp), and beyond --
 *                    pppppp and ffffff are legal, if rare
 *   MEZZO_MODERATE   mezzo means "moderately": mezzo piano (mp) is
 *                    moderately quiet, mezzo forte (mf) moderately loud
 *   CHANGE           crescendo (cresc.) is getting louder; decrescendo
 *                    (decresc.) and diminuendo (dim.) are getting quieter;
 *                    hairpins are equivalent to the written-out words
 *
 * Markings are opaque strings in events/dynamics.h; this file interprets
 * them. Both abbreviations ("pp") and full names ("pianissimo") are
 * accepted, case-insensitively, and a trailing '.' is ignored for the
 * change words ("cresc."). Anything unrecognized is reported as such,
 * never guessed at.
 *
 * Loudness is an ordinal rank, not a decibel level: only the order is
 * meaningful. Rank 0 is unused; quiet marks are negative and loud marks
 * positive, so ranks compare directly.
 */

typedef enum {
    STANDARD_MARKINGS_DYNAMICS_RULE_LOUDNESS_ORDER = 0,
    STANDARD_MARKINGS_DYNAMICS_RULE_ISSIMO_STACKING,
    STANDARD_MARKINGS_DYNAMICS_RULE_MEZZO_MODERATE,
    STANDARD_MARKINGS_DYNAMICS_RULE_CHANGE,
    STANDARD_MARKINGS_DYNAMICS_RULE_COUNT
} StandardMarkingsDynamicsRule;

typedef enum {
    DYNAMIC_CHANGE_NONE, /* not a change word or hairpin */
    DYNAMIC_CHANGE_LOUDER,
    DYNAMIC_CHANGE_QUIETER
} DynamicChange;

const Rule *standard_markings_dynamics_rule(StandardMarkingsDynamicsRule which);
const Constraint *standard_markings_dynamics_constraint(StandardMarkingsDynamicsRule which);

/*
 * LOUDNESS_ORDER / ISSIMO_STACKING / MEZZO_MODERATE: the loudness rank of
 * a level marking. p = -2, pp = -3, ppp = -4, ... (each extra p one lower);
 * mp = -1; mf = 1; f = 2, ff = 3, fff = 4, ... (each extra f one higher).
 * Returns true and sets *out_rank for a recognized marking; false, leaving
 * *out_rank untouched, otherwise (NULL included).
 */
bool standard_markings_dynamic_rank(const char *marking, int *out_rank);

/* True when the marking is a recognized level marking. */
bool standard_markings_is_dynamic_level(const char *marking);

/* LOUDNESS_ORDER: true when both are level markings and `quieter` ranks
   strictly below `louder`. */
bool standard_markings_dynamic_is_quieter(const char *quieter, const char *louder);

/*
 * The abbreviation of a level marking in its canonical lowercase form
 * ("pianissimo" -> "pp"), written to buffer. Returns false when the
 * marking is not a level marking or buffer is too small.
 */
bool standard_markings_dynamic_abbreviation(const char *marking, char *buffer, unsigned long buffer_size);

/* CHANGE: direction of a crescendo/decrescendo/diminuendo word or
   abbreviation; hairpins are written "<" (louder) and ">" (quieter). */
DynamicChange standard_markings_dynamic_change(const char *marking);

#endif
