#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "triad_rules.h"

/*
 * Triad identification rules for standard-tonality-v1. A triad is
 * identified by its root, its quality, and its inversion; this file is
 * root and quality (inversion is a later chapter's, apart from the bass
 * note a chord symbol names, chord_symbol_rules.h):
 *
 *   ROOT_AND_QUALITY   identify the root, imagine the major key
 *                      signature of the root, and identify the quality:
 *                      the third and fifth are as in that key (major),
 *                      the third lowered (minor), both lowered
 *                      (diminished), or the fifth raised (augmented)
 *   OCTAVE_EQUIVALENCE doubling a note, spacing the notes widely, or
 *                      both, does not change the triad: pitches an
 *                      octave apart are the same pitch class
 *   CLOSED_POSITION    to identify a triad with doublings and open
 *                      spacing, write it in closed spacing without
 *                      doublings: root, third, fifth stacked
 *
 * The input is any number of PitchSpellings in any order: exactly three
 * distinct note names (letter and accidental) once octaves are set
 * aside, whose letters are a root, the letter a third above, and the
 * letter a fifth above. If the root has an imaginary key signature
 * (needing a double accidental in the key), the quality is still found,
 * by the interval rules; standard_tonality_identify_triad_by_key_signature
 * follows the steps literally and reports no answer for such a root.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_ROOT_AND_QUALITY = 0,
    STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_OCTAVE_EQUIVALENCE,
    STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_CLOSED_POSITION,
    STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT
} StandardTonalityTriadIdentificationRule;

const Rule *standard_tonality_triad_identification_rule(StandardTonalityTriadIdentificationRule which);
const Constraint *standard_tonality_triad_identification_constraint(StandardTonalityTriadIdentificationRule which);

/*
 * ROOT_AND_QUALITY / OCTAVE_EQUIVALENCE: the root and quality of the
 * triad the notes make. On true *root_letter and *root_accidental name
 * the root and *quality its quality. False (outputs untouched) for NULL,
 * fewer than three notes, a NULL note, anything but exactly three
 * distinct note names, three that do not stack in thirds from one of
 * them, or a third and fifth that are not one of the four qualities.
 */
bool standard_tonality_identify_triad(const PitchSpelling *const *notes, size_t count, PitchLetter *root_letter,
                                      int *root_accidental, StandardTonalityTriadQuality *quality);

/*
 * ROOT_AND_QUALITY: the same answer by the written steps: the root's
 * major scale is built, and the third and fifth are read against its
 * third and fifth. False also when the root has no major scale.
 */
bool standard_tonality_identify_triad_by_key_signature(const PitchSpelling *const *notes, size_t count,
                                                       PitchLetter *root_letter, int *root_accidental,
                                                       StandardTonalityTriadQuality *quality);

/*
 * CLOSED_POSITION: the triad of the notes in closed spacing without
 * doublings, root first, into out[0..2] (the caller provides 3 pointers
 * and destroys each with pitch_spelling_destroy). The root is placed in
 * the lowest octave it occurs in among the notes. False (out untouched)
 * for the same reasons as standard_tonality_identify_triad, or
 * allocation failure.
 */
bool standard_tonality_triad_closed_position(const PitchSpelling *const *notes, size_t count, PitchSpelling *out[3]);

/* The lowest note of the notes (by 12-TET pitch; the first of equals), as
   letter and accidental. False for NULL, count 0, or a NULL note. */
bool standard_tonality_lowest_note(const PitchSpelling *const *notes, size_t count, PitchLetter *letter,
                                   int *accidental);

#endif
