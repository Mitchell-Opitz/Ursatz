#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MODE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MODE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Diatonic mode rules for standard-tonality-v1. A mode is a scale-like
 * collection of seven notes with its own pattern of whole (W) and half
 * (H) steps, ascending:
 *
 *   lydian      W-W-W-H-W-W-H   major with raised 4
 *   ionian      W-W-H-W-W-W-H   the major scale
 *   mixolydian  W-W-H-W-W-H-W   major with lowered 7
 *   dorian      W-H-W-W-W-H-W   natural minor with raised 6
 *   aeolian     W-H-W-W-H-W-W   the natural minor scale
 *   phrygian    H-W-W-W-H-W-W   natural minor with lowered 2
 *   locrian     H-W-W-H-W-W-W   natural minor with lowered 2 and 5
 *
 * Rules:
 *
 *   PATTERNS     each mode's step pattern, above
 *   BRIGHTNESS   modes run along a continuum, lydian (brightest, most
 *                like major) to locrian (darkest, most like minor), in
 *                the order above
 *   THIRD        the three bright modes (lydian, ionian, mixolydian)
 *                have mi, a major third above the tonic; the four dark
 *                modes (dorian, aeolian, phrygian, locrian) have me, a
 *                minor third
 *   ALTERATIONS  which degrees differ from the ionian (major) mode, or
 *                from the aeolian (natural minor) mode, and by how
 *                much; the darker the mode the more degrees are lowered
 *                against ionian
 *   PARALLEL     modes are described through the parallel relationship:
 *                a shared tonic, so every mode builds on any note
 *
 * A mode is a list of PitchSpelling, eight long: the seven notes and the
 * tonic an octave up, spelled with each letter once in turn
 * (scale_pattern.h). A mode can begin on any note, with whatever
 * accidentals its pattern needs (mixolydian on D-flat, aeolian on
 * G-flat, lydian on F-sharp).
 */

#define STANDARD_TONALITY_MODE_STEP_COUNT 7
#define STANDARD_TONALITY_MODE_NOTE_COUNT 8

/* In brightness order: brightest first. */
typedef enum {
    STANDARD_TONALITY_MODE_LYDIAN = 0,
    STANDARD_TONALITY_MODE_IONIAN,
    STANDARD_TONALITY_MODE_MIXOLYDIAN,
    STANDARD_TONALITY_MODE_DORIAN,
    STANDARD_TONALITY_MODE_AEOLIAN,
    STANDARD_TONALITY_MODE_PHRYGIAN,
    STANDARD_TONALITY_MODE_LOCRIAN,
    STANDARD_TONALITY_MODE_COUNT
} StandardTonalityMode;

/* The mode alterations are measured against. */
typedef enum {
    STANDARD_TONALITY_MODE_REFERENCE_IONIAN = 0,
    STANDARD_TONALITY_MODE_REFERENCE_AEOLIAN
} StandardTonalityModeReference;

typedef enum {
    STANDARD_TONALITY_MODE_RULE_PATTERNS = 0,
    STANDARD_TONALITY_MODE_RULE_BRIGHTNESS,
    STANDARD_TONALITY_MODE_RULE_THIRD,
    STANDARD_TONALITY_MODE_RULE_ALTERATIONS,
    STANDARD_TONALITY_MODE_RULE_PARALLEL,
    STANDARD_TONALITY_MODE_RULE_COUNT
} StandardTonalityModeRule;

const Rule *standard_tonality_mode_rule(StandardTonalityModeRule which);
const Constraint *standard_tonality_mode_constraint(StandardTonalityModeRule which);

/* "lydian" "ionian" "mixolydian" "dorian" "aeolian" "phrygian" "locrian";
   NULL for an invalid mode. */
const char *standard_tonality_mode_name(StandardTonalityMode mode);

/* PATTERNS: the index-th step of the mode in semitones (1 H, 2 W).
   index must be < STANDARD_TONALITY_MODE_STEP_COUNT, mode valid. */
int standard_tonality_mode_step(StandardTonalityMode mode, size_t index);

/* PATTERNS: true when notes (count must be 8) form that mode: its steps,
   each letter once in turn. False for NULL, a NULL note, the wrong
   count, or an invalid mode. */
bool standard_tonality_is_mode_scale(StandardTonalityMode mode, const PitchSpelling *const *notes, size_t count);

/* PATTERNS / PARALLEL: builds the mode on `tonic` into out[0..7] (the
   caller provides 8 pointers and destroys each with
   pitch_spelling_destroy). False, leaving out untouched, for NULL, an
   invalid mode, allocation failure, or a mode needing more than a double
   accidental. */
bool standard_tonality_build_mode_scale(StandardTonalityMode mode, const PitchSpelling *tonic,
                                        PitchSpelling *out[STANDARD_TONALITY_MODE_NOTE_COUNT]);

/* BRIGHTNESS: 1 (lydian, brightest) to 7 (locrian, darkest); 0 for an
   invalid mode. */
int standard_tonality_mode_brightness_rank(StandardTonalityMode mode);

/* BRIGHTNESS: true when `a` is brighter than `b`. False if either is
   invalid. */
bool standard_tonality_mode_is_brighter(StandardTonalityMode a, StandardTonalityMode b);

/* THIRD: semitones from tonic to the third degree: 4 (mi) or 3 (me); 0
   for an invalid mode. */
int standard_tonality_mode_third_semitones(StandardTonalityMode mode);

/* THIRD: bright modes have mi, dark modes me. False for an invalid
   mode. */
bool standard_tonality_mode_is_bright(StandardTonalityMode mode);
bool standard_tonality_mode_is_dark(StandardTonalityMode mode);

/*
 * ALTERATIONS: the degrees (2..7) where the mode differs from the
 * reference, in ascending order, with +1 (raised) or -1 (lowered)
 * semitone each. Writes up to 6 entries to degrees and offsets and
 * returns how many (0 for the reference mode itself, or for an invalid
 * argument or NULL output).
 */
size_t standard_tonality_mode_alterations(StandardTonalityMode mode, StandardTonalityModeReference reference,
                                          int degrees[6], int offsets[6]);

/* PARALLEL: true when two tonics are the same name (letter and
   accidental), whatever the octave. False for NULL. */
bool standard_tonality_modes_parallel(const PitchSpelling *tonic_a, const PitchSpelling *tonic_b);

#endif
