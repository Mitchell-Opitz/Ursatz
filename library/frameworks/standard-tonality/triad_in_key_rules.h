#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IN_KEY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IN_KEY_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "triad_rules.h"

/*
 * Triad qualities within a key for standard-tonality-v1. Any note of a
 * scale can be the root of a triad, and the quality follows the scale
 * degree, the same in every key of the kind:
 *
 *   MAJOR_KEY       major on do, fa, sol (1 4 5); minor on re, mi, la
 *                   (2 3 6); diminished on ti (7)
 *   MINOR_KEY       natural minor: minor on do, fa, sol (1 4 5); major on
 *                   me, le, te (3 6 7); diminished on re (2)
 *   RAISED_LEADING_TONE
 *                   with the leading tone raised (harmonic minor) the
 *                   triad on sol is major (a major dominant) and the
 *                   triad on ti is diminished; the triad on re stays
 *                   diminished
 *   KEY_INDEPENDENT the qualities do not change with the key: the
 *                   triad on do is major in every major key
 *
 * Harmonic minor's triad on me, which also holds the raised leading tone,
 * is augmented. The source's example names the triads above and leaves
 * that one out; it follows from the spelled scale and is included here
 * so the table is complete.
 *
 * Degrees are 1..7. The table is checked against triads stacked in
 * thirds from the spelled scales.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_KEY_MAJOR = 0,
    STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL,
    STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC,
    STANDARD_TONALITY_TRIAD_KEY_COUNT
} StandardTonalityTriadKey;

/* The quality of the triad on scale degree 1..7 of the kind of key. False
   (output untouched) for an invalid key kind or degree, or NULL output. */
bool standard_tonality_triad_quality_in_key(StandardTonalityTriadKey key, int degree,
                                            StandardTonalityTriadQuality *quality);

#endif
