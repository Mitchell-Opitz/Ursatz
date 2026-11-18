#ifndef URSATZ_THEORY_TONALITY_BRIGHTNESS_H
#define URSATZ_THEORY_TONALITY_BRIGHTNESS_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_material.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * Brightness (target architecture 4.4, OMT I ch. 14 example 1): the seven diatonic modes line up from the darkest to the brightest
 * by how many of their degrees are raised against the one above, locrian 1, phrygian 2, aeolian (natural minor) 3, dorian 4,
 * mixolydian 5, major 6, lydian 7. Only the diatonic family has a rank: harmonic and melodic minor are not diatonic, and a request
 * for their brightness is refused.
 */

/*
 * The brightness rank (1 to 7) of the material into *rank. False (*rank untouched) for a NULL material or one with no brightness,
 * and then `message` (when not NULL, with size above 0) says "<name> has no brightness: only the diatonic modes rank".
 */
bool brightness_rank_of(const PitchMaterial *material, int *rank, char *message, size_t size);

/*
 * The diatonic material ranked `steps` above the material's rank (negative for darker), taken from the snapshot, with the same
 * center. NULL when the material has no brightness (as brightness_rank_of), when no material of the snapshot has that rank (past
 * locrian or lydian), or when more than one has, and then `message` says why. NULL for a NULL argument.
 */
const PitchMaterial *brightness_shift(const KnowledgeSnapshot *snapshot, const PitchMaterial *material, int steps, char *message,
                                      size_t size);

#endif
