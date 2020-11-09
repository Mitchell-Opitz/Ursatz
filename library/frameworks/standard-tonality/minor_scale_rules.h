#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_SCALE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MINOR_SCALE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Minor scale rules for standard-tonality-v1. A minor scale's third
 * note is a half step lower than the major scale's third on the same
 * tonic. There are three kinds of minor scale, each an ordered
 * collection of whole (W) and half (H) steps, "3H" being three half
 * steps (a whole step plus a half step):
 *
 *   NATURAL   W-H-W-W-H-W-W ascending (the same pattern read downward)
 *   HARMONIC  W-H-W-W-H-3H-H ascending (and descending alike)
 *   MELODIC   W-H-W-W-W-W-H ascending, W-W-H-W-W-H-W descending (the
 *             descending form is natural minor)
 *
 * A minor scale is named for its first note, accidental included, like a
 * major scale (see standard_tonality_same_scale_name, major_scale_rules.h).
 * Minor KEYS and signatures are always just "minor" and rest on the
 * natural form (minor_key_rules.h).
 *
 * Scales are lists of PitchSpelling, eight long. A descending scale
 * runs from the upper tonic down to the lower one. Spelling follows
 * scale_pattern.h: each letter once in turn.
 */

#define STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT 7
#define STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT 8

typedef enum {
    STANDARD_TONALITY_MINOR_NATURAL = 0,
    STANDARD_TONALITY_MINOR_HARMONIC,
    STANDARD_TONALITY_MINOR_MELODIC_ASCENDING,
    STANDARD_TONALITY_MINOR_MELODIC_DESCENDING,
    STANDARD_TONALITY_MINOR_FORM_COUNT
} StandardTonalityMinorForm;

typedef enum {
    STANDARD_TONALITY_MINOR_SCALE_RULE_NATURAL = 0,
    STANDARD_TONALITY_MINOR_SCALE_RULE_HARMONIC,
    STANDARD_TONALITY_MINOR_SCALE_RULE_MELODIC,
    STANDARD_TONALITY_MINOR_SCALE_RULE_THIRD_LOWERED,
    STANDARD_TONALITY_MINOR_SCALE_RULE_COUNT
} StandardTonalityMinorScaleRule;

const Rule *standard_tonality_minor_scale_rule(StandardTonalityMinorScaleRule which);
const Constraint *standard_tonality_minor_scale_constraint(StandardTonalityMinorScaleRule which);

/* The index-th step of the form, in semitones, in the order the scale
   is played: 2 (W), 1 (H), or 3 (3H). index must be <
   STANDARD_TONALITY_MINOR_SCALE_STEP_COUNT; form must be valid. For
   MELODIC_DESCENDING the steps are the drops from the upper tonic. */
int standard_tonality_minor_scale_step(StandardTonalityMinorForm form, size_t index);

/* True when notes (count must be 8) form that minor scale: the form's
   steps, each letter once in turn. A descending form is listed from the
   upper tonic down. False for NULL, a NULL note, the wrong count, or an
   invalid form. */
bool standard_tonality_is_minor_scale(StandardTonalityMinorForm form, const PitchSpelling *const *notes,
                                      size_t count);

/*
 * Builds the minor scale of the given form into out[0..7] (the caller
 * provides 8 pointers and destroys each with pitch_spelling_destroy).
 * For a descending form `tonic` is the lower tonic and out[0] is the
 * upper one. Returns false, leaving out untouched, for NULL, an invalid
 * form, allocation failure, or a scale that would need more than a
 * double accidental.
 */
bool standard_tonality_build_minor_scale(StandardTonalityMinorForm form, const PitchSpelling *tonic,
                                         PitchSpelling *out[STANDARD_TONALITY_MINOR_SCALE_NOTE_COUNT]);

/* THIRD_LOWERED: true when the natural minor scale on `tonic` has its
   third on the same letter as the major scale's third and a half step
   lower. False for NULL or a tonic with no major or minor scale. */
bool standard_tonality_minor_third_lowered(const PitchSpelling *tonic);

#endif
