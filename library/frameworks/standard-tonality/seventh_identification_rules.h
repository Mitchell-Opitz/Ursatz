#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "seventh_rules.h"

/*
 * Seventh chord identification rules for standard-tonality-v1. A seventh
 * chord is identified by its root, the quality of its triad and seventh,
 * and its inversion; this file is root and quality (inversion is a later
 * chapter's, apart from the bass note a chord symbol names,
 * chord_symbol_rules.h), so the examples are in root position:
 *
 *   ROOT_AND_QUALITY   identify the root, imagine the major key
 *                      signature of the root, identify the quality of
 *                      the triad, then the quality of the seventh: a
 *                      major seventh made a half step smaller is minor
 *   OCTAVE_EQUIVALENCE doubling a note, spacing the notes widely (even
 *                      across clefs), or both, does not change the
 *                      chord: pitches an octave apart are the same pitch
 *                      class
 *   CLOSED_POSITION    to identify a seventh chord with doublings and
 *                      open spacing, write it in closed spacing without
 *                      doublings: root, third, fifth, seventh stacked
 *
 * The input is any number of PitchSpellings in any order: exactly four
 * distinct note names (letter and accidental) once octaves are set
 * aside, whose letters are a root and the letters a third, a fifth, and a
 * seventh above it. If the root has an imaginary key signature (needing
 * a double accidental in the key), the quality is still found, by the
 * interval rules; standard_tonality_identify_seventh_by_key_signature
 * follows the steps literally and reports no answer for such a root
 * (respell it enharmonically first).
 */

typedef enum {
    STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_ROOT_AND_QUALITY = 0,
    STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_OCTAVE_EQUIVALENCE,
    STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_CLOSED_POSITION,
    STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT
} StandardTonalitySeventhIdentificationRule;

const Rule *standard_tonality_seventh_identification_rule(StandardTonalitySeventhIdentificationRule which);
const Constraint *standard_tonality_seventh_identification_constraint(StandardTonalitySeventhIdentificationRule which);

/*
 * ROOT_AND_QUALITY / OCTAVE_EQUIVALENCE: the root and quality of the
 * seventh chord the notes make. On true *root_letter and *root_accidental
 * name the root and *quality its quality. False (outputs untouched) for
 * NULL, fewer than four notes, a NULL note, anything but exactly four
 * distinct note names, four that do not stack in thirds from one of
 * them, or a triad and seventh that are not one of the five qualities.
 */
bool standard_tonality_identify_seventh(const PitchSpelling *const *notes, size_t count, PitchLetter *root_letter,
                                        int *root_accidental, StandardTonalitySeventhQuality *quality);

/*
 * ROOT_AND_QUALITY: the same answer by the written steps: the root's
 * major scale is built, and the third, fifth, and seventh are read
 * against its third, fifth, and seventh. False also when the root has no
 * major scale.
 */
bool standard_tonality_identify_seventh_by_key_signature(const PitchSpelling *const *notes, size_t count,
                                                         PitchLetter *root_letter, int *root_accidental,
                                                         StandardTonalitySeventhQuality *quality);

/*
 * CLOSED_POSITION: the seventh chord of the notes in closed spacing
 * without doublings, root first, into out[0..3] (the caller provides 4
 * pointers and destroys each with pitch_spelling_destroy). The root is
 * placed in the lowest octave it occurs in among the notes. False (out
 * untouched) for the same reasons as standard_tonality_identify_seventh,
 * or allocation failure.
 */
bool standard_tonality_seventh_closed_position(const PitchSpelling *const *notes, size_t count,
                                               PitchSpelling *out[4]);

#endif
