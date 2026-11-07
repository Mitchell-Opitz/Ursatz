#ifndef URSATZ_COMPOSER_MOTIVE_FIDELITY_H
#define URSATZ_COMPOSER_MOTIVE_FIDELITY_H

#include <stdbool.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

/*
 * How faithful an idea's realized notes are to the motive they were made from (R-354).
 *   score   motive_recognizability of the realized motive against the source, the total at the default weights
 *   found   the recognizer found a chain of at most two steps from the source to the realized motive
 *   agrees  the chain heard agrees with what the ref intended (after dropping octave transpositions)
 *   heard   the chain in words: "exact", "not recognized", or each step's phrase joined by ", then "
 */
typedef struct {
    MotiveScore score;
    bool found;
    bool agrees;
    char heard[96];
} MotiveFidelity;

/*
 * Measures `realized` against `source` as the ref intended it. from_segment says the source is already the segment the
 * ref names (a fragment or a residue), so a fragment step is not heard again. A transposition by whole octaves is not
 * heard (the realized idea sits in the voice's register). False for NULL arguments or when the score or the recognizer
 * fails; *out is untouched then.
 */
bool motive_fidelity(const Motive *source, const Motive *realized, const PlanMotiveRef *ref, bool from_segment,
                     MotiveFidelity *out);

#endif
