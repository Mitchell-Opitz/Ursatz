#include "theory/tonality/common_practice_materials.h"

#include <pthread.h>
#include <stdio.h>

#include "theory/knowledge/knowledge_modes.h"

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

/* StandardTonalityMode order: major, natural minor, harmonic minor, melodic minor. */
#define BUILTIN_DEGREES 7

static PitchMaterial g_builtin[MATERIAL_ID_COUNT];
static CommonPracticeMaterials g_builtin_materials;
static pthread_once_t g_builtin_once = PTHREAD_ONCE_INIT;

static void build_builtin(void) {
    static const char *const NAMES[MATERIAL_ID_COUNT] = {"major", "natural minor", "harmonic minor", "melodic minor"};
    static const char *const KEY_WORDS[MATERIAL_ID_COUNT] = {"major", "minor", "harmonic minor", "melodic minor"};
    static const char *const FAMILIES[MATERIAL_ID_COUNT] = {"diatonic", "diatonic", "minor", "minor"};
    static const int BRIGHTNESS[MATERIAL_ID_COUNT] = {6, 3, -1, -1};
    for (size_t i = 0; i < MATERIAL_ID_COUNT; i++) {
        PitchMaterial *m = &g_builtin[i];
        m->id = MATERIAL_IDS[i];
        m->name = NAMES[i];
        m->key_word = KEY_WORDS[i];
        m->family = FAMILIES[i];
        m->has_brightness = BRIGHTNESS[i] >= 0;
        m->brightness_rank = BRIGHTNESS[i] >= 0 ? BRIGHTNESS[i] : 0;
        m->step_count = BUILTIN_DEGREES;
        for (size_t d = 0; d < BUILTIN_DEGREES; d++) {
            m->steps[d] = knowledge_tonality_mode_steps[i][d];
        }
    }
    g_builtin[1].variant_of = &g_builtin[0];
    g_builtin[2].variant_of = &g_builtin[1];
    g_builtin[3].variant_of = &g_builtin[1];
    for (size_t d = 0; d < BUILTIN_DEGREES; d++) {
        g_builtin[3].descending_steps[d] = knowledge_tonality_mode_steps[1][d];
    }
    g_builtin[3].descending_step_count = BUILTIN_DEGREES;
    g_builtin_materials.major = &g_builtin[0];
    g_builtin_materials.natural_minor = &g_builtin[1];
    g_builtin_materials.harmonic_minor = &g_builtin[2];
    g_builtin_materials.melodic_minor = &g_builtin[3];
}

const CommonPracticeMaterials *common_practice_materials_builtin(void) {
    pthread_once(&g_builtin_once, build_builtin);
    return &g_builtin_materials;
}
