#ifndef URSATZ_THEORY_TONALITY_COMMON_PRACTICE_MATERIALS_H
#define URSATZ_THEORY_TONALITY_COMMON_PRACTICE_MATERIALS_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_material.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/tonality/key_vocabulary.h"

/*
 * The four pitch materials of common-practice tonality, resolved once from a knowledge snapshot (omt.material.major,
 * .natural-minor, .harmonic-minor, .melodic-minor). The pointers live as long as the snapshot. The tables written for major and minor
 * keys (CommonPracticeKeyMode, HarmonyMode, the triad and seventh quality rows, the applied vocabularies, the key finder's 24-key
 * index) stay as they are; a key's material reaches them only through common_practice_key_mode_of (R-593).
 */
typedef struct {
    const PitchMaterial *major;
    const PitchMaterial *natural_minor;
    const PitchMaterial *harmonic_minor;
    const PitchMaterial *melodic_minor;
} CommonPracticeMaterials;

/*
 * Reads the four materials from `snapshot` into *out. False (out untouched) for a NULL argument or a missing material, and then
 * `message` (when not NULL, with size above 0) names the missing id: "common practice materials: omt.material.<name> is not in the
 * knowledge snapshot".
 */
bool common_practice_materials_resolve(const KnowledgeSnapshot *snapshot, CommonPracticeMaterials *out, char *message, size_t size);

/* The material of a key mode: the major, or the natural minor. NULL for a NULL table or another mode. */
const PitchMaterial *common_practice_material(const CommonPracticeMaterials *materials, CommonPracticeKeyMode mode);

/*
 * The key mode a material is read as, by its third: major for a major third (4 semitones), minor for a minor third (3). False
 * (*out untouched) for a NULL material, one with no third, or a third of another size.
 */
bool common_practice_key_mode_of(const PitchMaterial *material, CommonPracticeKeyMode *out);

#endif
