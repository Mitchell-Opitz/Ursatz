#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_IDENTIFICATION_H

/*
 * Deprecated. Not the key identification owner (key_regions_estimate). Kept while Ursatz-GUI calls it;
 * removed in P27 (docs/analytic-job-owners.md).
 */

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "interpretation/key.h"
#include "key_vocabulary.h"

/*
 * Deterministic, single-answer key identification for
 * common-practice-minimal-v1.
 *
 * Input shape: the same caller-supplies-the-dimension pattern
 * triad_identification.h uses. PitchIdentity carries no numeric
 * semantics yet, so this module does not derive intervals from a
 * Sonority/Context itself -- the caller supplies each note's interval
 * in semitones (0-11) from a candidate tonic, already measured by
 * whoever has the tuning-system knowledge to do so. common_practice_
 * build_key (key_vocabulary.h) only labels a tonic/mode the caller
 * already knows; this module is the piece that actually decides mode
 * from raw pitch content.
 *
 * Matching is by unordered pitch-class-SET containment, against exactly
 * the two collections common-practice-minimal-v1 knows: major and
 * natural minor (see README.md -- harmonic/melodic minor variants are
 * deliberately excluded, same restraint as the rest of this framework).
 * A collection matches one of the two when all 7 of that collection's
 * pitch classes are present -- extra pitch classes beyond those 7 (e.g.
 * a chromatic passing tone) do not block the match, the same tolerance
 * chord identification already gives non-diatonic notes within a single
 * sonority. The two collections never share their full 7-element set,
 * so no input can validly match both.
 */

/*
 * Returns true and sets *out to the matching mode when
 * tonic_relative_pitch_classes (each 0-11, duplicates ignored, order
 * irrelevant) contains all 7 pitch classes of the major or natural-minor
 * collection relative to the candidate tonic; extra pitch classes beyond
 * those 7 do not prevent a match. Returns false (out untouched) when no
 * match exists -- e.g. fewer than 7 of the required pitch classes
 * present, a value outside 0-11, or a collection this framework does
 * not define as major or natural minor.
 */
bool common_practice_identify_key_mode(const int *tonic_relative_pitch_classes, size_t pitch_class_count,
                                        CommonPracticeKeyMode *out);

#endif
