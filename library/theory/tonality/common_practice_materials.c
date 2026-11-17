#include "theory/tonality/common_practice_materials.h"

#include <stdio.h>

#define MATERIAL_ID_COUNT 4

static const char *const MATERIAL_IDS[MATERIAL_ID_COUNT] = {"omt.material.major", "omt.material.natural-minor",
                                                            "omt.material.harmonic-minor", "omt.material.melodic-minor"};

#define THIRD_MINOR 3
#define THIRD_MAJOR 4

bool common_practice_materials_resolve(const KnowledgeSnapshot *snapshot, CommonPracticeMaterials *out, char *message, size_t size) {
    if (message != NULL && size > 0) {
        message[0] = '\0';
    }
    if (snapshot == NULL || out == NULL) {
        return false;
    }
    const PitchMaterial *found[MATERIAL_ID_COUNT];
    for (size_t i = 0; i < MATERIAL_ID_COUNT; i++) {
        found[i] = knowledge_snapshot_material(snapshot, MATERIAL_IDS[i]);
        if (found[i] == NULL) {
            if (message != NULL && size > 0) {
                snprintf(message, size, "common practice materials: %s is not in the knowledge snapshot", MATERIAL_IDS[i]);
            }
            return false;
        }
    }
    out->major = found[0];
    out->natural_minor = found[1];
    out->harmonic_minor = found[2];
    out->melodic_minor = found[3];
    return true;
}

const PitchMaterial *common_practice_material(const CommonPracticeMaterials *materials, CommonPracticeKeyMode mode) {
    if (materials == NULL) {
        return NULL;
    }
    switch (mode) {
        case COMMON_PRACTICE_KEY_MAJOR:
            return materials->major;
        case COMMON_PRACTICE_KEY_MINOR:
            return materials->natural_minor;
        default:
            return NULL;
    }
}

bool common_practice_key_mode_of(const PitchMaterial *material, CommonPracticeKeyMode *out) {
    int third = pitch_material_third(material);
    if (out == NULL || (third != THIRD_MINOR && third != THIRD_MAJOR)) {
        return false;
    }
    *out = third == THIRD_MAJOR ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR;
    return true;
}
