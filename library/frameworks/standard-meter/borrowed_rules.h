#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_BORROWED_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_BORROWED_RULES_H

#include <stdbool.h>

#include "meter_class_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/time_signature.h"
#include "time/tuplet.h"

/*
 * Borrowed division rules for standard-meter-v1. A meter is defined by a
 * consistent beat division -- two in simple meter, three in compound.
 * A composer sometimes uses the other one; such rhythms are tuplets:
 *
 *   BORROWED       a triplet (3 in the time of 2) in a simple meter
 *                  divides a beat three ways, "borrowed" from compound
 *                  meter; a duplet (2 in the time of 3) in a compound
 *                  meter divides a beat two ways, borrowed from simple.
 *                  The same ratio in its own meter -- a triplet in
 *                  compound -- is just that meter's division, not
 *                  borrowed. Only triplets and duplets are named here.
 *   LEVEL          a tuplet occurs at a metric level, found from the
 *                  length it spans against the beat: the whole beat
 *                  (the most common triplet), a subdivision (a span
 *                  that divides the beat evenly: a sixteenth-note
 *                  triplet spans one eighth), or several beats (a triplet
 *                  of quarter notes spans a half note, two beats).
 *   TYPICAL_LEVEL  triplets occur at any level; duplets typically at the
 *                  beat or a subdivision. This describes where they
 *                  usually occur, it does not forbid anything.
 *
 * How a borrowed division is counted is counting_rules.h (the borrowed
 * meter's own syllables).
 */

typedef enum {
    STANDARD_METER_BORROWED_RULE_BORROWED = 0,
    STANDARD_METER_BORROWED_RULE_LEVEL,
    STANDARD_METER_BORROWED_RULE_TYPICAL_LEVEL,
    STANDARD_METER_BORROWED_RULE_COUNT
} StandardMeterBorrowedRule;

typedef enum {
    TUPLET_LEVEL_BEAT,        /* spans exactly one beat */
    TUPLET_LEVEL_SUBDIVISION, /* spans less than a beat, dividing it evenly */
    TUPLET_LEVEL_MULTI_BEAT   /* spans a whole number of beats, more than one */
} TupletLevel;

const Rule *standard_meter_borrowed_rule(StandardMeterBorrowedRule which);
const Constraint *standard_meter_borrowed_constraint(StandardMeterBorrowedRule which);

/* BORROWED: true for a triplet in a simple meter or a duplet in a
   compound meter; false for anything else, including an unclassified
   meter type. */
bool standard_meter_tuplet_borrowed(const Tuplet *tuplet, MeterType type);

/*
 * LEVEL: the metric level of a tuplet whose whole group spans `span`
 * (an exact Duration; standard-rhythm computes it from the written
 * value), against the beat of the signature's meter class. False,
 * leaving *out untouched, when the signature is unclassified or the span
 * is zero or neither divides the beat evenly nor is a whole number of
 * beats.
 */
bool standard_meter_tuplet_level(const TimeSignature *time_signature, const Duration *span, TupletLevel *out);

/* TYPICAL_LEVEL: true for a triplet at any level and a duplet at the beat
   or a subdivision; false for a duplet at the multi-beat level and for
   any other tuplet (no convention is stated for them). */
bool standard_meter_tuplet_level_typical(const Tuplet *tuplet, TupletLevel level);

#endif
