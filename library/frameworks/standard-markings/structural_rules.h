#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_STRUCTURAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_STRUCTURAL_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Structural marker rules for standard-markings-v1. Structural features
 * divide a work or movement into smaller sections. Kinds are opaque
 * strings in events/structural_marker.h; this file gives the standard ones
 * their meaning. Recognized kinds (case-insensitive):
 *
 *   fermata          hold the note longer than its regular duration; often
 *                    at the start or end of a section
 *   caesura          a break or musical cutoff
 *   breath-mark      a breath for wind players and singers; for others, a
 *                    pause about as long as a breath
 *   repeat-start     |:  begin a repeated section
 *   repeat-end       :|  end a repeated section (repeat back to the start)
 *   ending           a numbered ending (parameter = 1, 2, 3, ...); the nth
 *                    ending is played the nth time through
 *   one-measure-repeat / two-measure-repeat
 *                    repeat the previous one / two measures
 *   segno, coda, to-coda, fine      the two signs and the end marks
 *   dc-al-coda, dc-al-fine, ds-al-coda, ds-al-fine
 *                    da capo (from the beginning) / dal segno (from the
 *                    sign), then play to the coda or to the fine
 *
 * How a sequence of these steers playback is repeat_navigation.h.
 */

typedef enum {
    STANDARD_MARKINGS_STRUCTURAL_RULE_FERMATA_HOLD = 0,
    STANDARD_MARKINGS_STRUCTURAL_RULE_CAESURA_BREAK,
    STANDARD_MARKINGS_STRUCTURAL_RULE_BREATH_MARK,
    STANDARD_MARKINGS_STRUCTURAL_RULE_REPEAT_SIGNS,
    STANDARD_MARKINGS_STRUCTURAL_RULE_NUMBERED_ENDINGS,
    STANDARD_MARKINGS_STRUCTURAL_RULE_MEASURE_REPEATS,
    STANDARD_MARKINGS_STRUCTURAL_RULE_JUMP_MARKERS,
    STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT
} StandardMarkingsStructuralRule;

typedef enum {
    STRUCTURAL_KIND_UNKNOWN,
    STRUCTURAL_KIND_FERMATA,
    STRUCTURAL_KIND_CAESURA,
    STRUCTURAL_KIND_BREATH_MARK,
    STRUCTURAL_KIND_REPEAT_START,
    STRUCTURAL_KIND_REPEAT_END,
    STRUCTURAL_KIND_ENDING,
    STRUCTURAL_KIND_ONE_MEASURE_REPEAT,
    STRUCTURAL_KIND_TWO_MEASURE_REPEAT,
    STRUCTURAL_KIND_SEGNO,
    STRUCTURAL_KIND_CODA,
    STRUCTURAL_KIND_TO_CODA,
    STRUCTURAL_KIND_FINE,
    STRUCTURAL_KIND_DC_AL_CODA,
    STRUCTURAL_KIND_DC_AL_FINE,
    STRUCTURAL_KIND_DS_AL_CODA,
    STRUCTURAL_KIND_DS_AL_FINE
} StructuralKind;

typedef enum {
    STRUCTURAL_JUMP_NONE,
    STRUCTURAL_JUMP_TO_START, /* da capo: back to the beginning */
    STRUCTURAL_JUMP_TO_SEGNO  /* dal segno: back to the sign */
} StructuralJump;

typedef enum {
    STRUCTURAL_STOP_NONE,
    STRUCTURAL_STOP_AT_FINE, /* play to the Fine */
    STRUCTURAL_STOP_AT_CODA  /* play to the "to coda" sign, then to the coda */
} StructuralStop;

const Rule *standard_markings_structural_rule(StandardMarkingsStructuralRule which);
const Constraint *standard_markings_structural_constraint(StandardMarkingsStructuralRule which);

/* The kind named by a marker's kind string; STRUCTURAL_KIND_UNKNOWN for
   anything unrecognized or NULL. */
StructuralKind standard_markings_structural_kind(const char *kind);

/* FERMATA_HOLD: the note is held longer than its regular duration. */
bool standard_markings_extends_duration(StructuralKind kind);

/* CAESURA_BREAK / BREATH_MARK: a break in the music (caesura, breath mark). */
bool standard_markings_interrupts_flow(StructuralKind kind);

/*
 * NUMBERED_ENDINGS: an ending needs a number of at least 1; every other
 * kind ignores its parameter. True when (kind, parameter) is well formed.
 */
bool standard_markings_marker_well_formed(StructuralKind kind, unsigned int parameter);

/* MEASURE_REPEATS: measures a repeat sign covers (1 or 2); 0 for other kinds. */
unsigned int standard_markings_measure_repeat_span(StructuralKind kind);

/* JUMP_MARKERS: where a D.C./D.S. marker sends the player, and what ends
   the repeat. None for every other kind. */
StructuralJump standard_markings_jump_target(StructuralKind kind);
StructuralStop standard_markings_jump_stop(StructuralKind kind);

#endif
