#ifndef URSATZ_THEORY_TONALITY_MODAL_CHORDS_H
#define URSATZ_THEORY_TONALITY_MODAL_CHORDS_H

#include <stdbool.h>

#include "model/pitch/pitch_material.h"
#include "theory/knowledge/vocabulary_entry.h"
#include "theory/tonality/common_practice_materials.h"
#include "theory/tonality/triad_rules.h"

/*
 * The chords of a modal key (R-725): a seven-step material that is neither the major nor the natural minor and names a cadence (the
 * five church modes of omt.json), stacked in thirds from its own steps. Nothing here knows a leading tone or a dominant seventh:
 * the chord on a degree is the material's third and fifth above it.
 */

/* The role of a chord in a modal key (the order of HarmonicFunction; the Composer maps one onto the other). */
typedef enum { MODAL_FUNCTION_TONIC = 0, MODAL_FUNCTION_PREDOMINANT, MODAL_FUNCTION_DOMINANT } ModalFunction;

/*
 * The triad on `degree` (1 to 7) of a 7-step material: the steps degree, degree+2 and degree+4 (counted mod 7) above the center.
 * *root_semitones is the root's semitones above the center, intervals[3] the semitones above the root (0, third, fifth), *quality its
 * quality (each output may be NULL). False (outputs untouched) for a NULL material, one without 7 steps, a degree out of range or
 * a stack of another quality than major, minor, diminished and augmented.
 */
bool pitch_material_triad_on(const PitchMaterial *material, int degree, int *root_semitones, int intervals[3],
                             StandardTonalityTriadQuality *quality);

/* True for a material that is not the major or the natural minor of `materials`, has 7 steps and names a cadence. False for NULL. */
bool pitch_material_is_modal_key(const CommonPracticeMaterials *materials, const PitchMaterial *material);

/* True when `degree` is one of the material's avoided chord degrees (chords_to_avoid). False for NULL. */
bool pitch_material_avoids(const PitchMaterial *material, int degree);

/*
 * The function of the chord on `degree` in a modal key: tonic for the first degree, dominant for one of the cadence kind's approach
 * degrees, predominant for any other major or minor triad that the material does not avoid. False (*out untouched) for the rest
 * (an avoided chord, a diminished or augmented one, an unusable argument).
 */
bool modal_function_of(const PitchMaterial *material, const VocabularyCadence *cadence, int degree, ModalFunction *out);

#endif
