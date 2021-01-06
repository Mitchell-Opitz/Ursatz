#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_INVERSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_INVERSION_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Intervallic inversion rules for standard-tonality-v1. Inversion
 * "flips" two notes: the lower note moves up an octave (or the upper
 * moves down), so the old upper note is now the bottom:
 *
 *   SIZES            the sizes of an interval and its inversion add up
 *                    to 9: unison and octave, second and seventh, third
 *                    and sixth, fourth and fifth
 *   QUALITIES        perfect inverts to perfect, major to minor and
 *                    minor to major, augmented to diminished and
 *                    diminished to augmented (doubly and triply too)
 *   IDENTIFICATION   inversion identifies an interval whose bottom note
 *                    is awkward (the tonic of a difficult key): invert
 *                    it, identify the inversion from the new bottom
 *                    note, then invert the answer. E-double-flat up to
 *                    A-flat inverts to A-flat up to E-double-flat, a
 *                    diminished fifth, so the original is an augmented
 *                    fourth
 *
 * Inversion is defined for simple intervals (size 1..8). A compound
 * interval inverts through its simple counterpart.
 */

typedef enum {
    STANDARD_TONALITY_INTERVAL_INVERSION_RULE_SIZES = 0,
    STANDARD_TONALITY_INTERVAL_INVERSION_RULE_QUALITIES,
    STANDARD_TONALITY_INTERVAL_INVERSION_RULE_IDENTIFICATION,
    STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT
} StandardTonalityIntervalInversionRule;

const Rule *standard_tonality_interval_inversion_rule(StandardTonalityIntervalInversionRule which);
const Constraint *standard_tonality_interval_inversion_constraint(StandardTonalityIntervalInversionRule which);

/* SIZES: the size of the inversion of a size (9 minus its simple size):
   1 gives 8, 8 gives 1, 2 gives 7, and so on. 0 for a size below 1. */
int standard_tonality_interval_inverted_size(int size);

/* QUALITIES: the quality of the inversion. Perfect stays perfect; major
   and minor swap; augmented, doubly augmented, and triply augmented
   become diminished, doubly diminished, and triply diminished, and the
   reverse. Returns the same quality for an invalid one. */
StandardTonalityIntervalQuality standard_tonality_interval_inverted_quality(StandardTonalityIntervalQuality quality);

/* SIZES / QUALITIES: the inversion of an interval given by size and
   quality. False (outputs untouched) for NULL outputs, a size below 1,
   or a quality that does not fit the size. */
bool standard_tonality_interval_invert(int size, StandardTonalityIntervalQuality quality, int *inverted_size,
                                       StandardTonalityIntervalQuality *inverted_quality);

/*
 * IDENTIFICATION: the size and quality of the interval from `lower` up to
 * `upper`, found through its inversion: the interval from `upper` up to
 * `lower` an octave higher is identified by the major-scale method from
 * `upper`, and the answer inverted. Only simple intervals. False
 * (outputs untouched) for NULL, a compound or reversed interval, or an
 * `upper` with no major scale.
 */
bool standard_tonality_interval_quality_via_inversion(const PitchSpelling *lower, const PitchSpelling *upper,
                                                      int *size, StandardTonalityIntervalQuality *quality);

#endif
