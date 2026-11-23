#include "theory/tonality/common_practice_materials.h"

#include <stdio.h>
#include <string.h>

#define MATERIAL_ID_COUNT 4

static const char *const MATERIAL_IDS[MATERIAL_ID_COUNT] = {"omt.material.major", "omt.material.natural-minor",
                                                            "omt.material.harmonic-minor", "omt.material.melodic-minor"};

static const char *const TRIAD_IDS[4] = {"omt.chord-type.major-triad", "omt.chord-type.minor-triad", "omt.chord-type.diminished-triad",
                                         "omt.chord-type.augmented-triad"};
static const char *const SEVENTH_IDS[5] = {"omt.chord-type.major-seventh", "omt.chord-type.dominant-seventh", "omt.chord-type.minor-seventh",
                                           "omt.chord-type.half-diminished-seventh", "omt.chord-type.diminished-seventh"};

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
    const ChordType *triads[4];
    const ChordType *sevenths[5];
    for (size_t i = 0; i < 9; i++) {
        const char *id = i < 4 ? TRIAD_IDS[i] : SEVENTH_IDS[i - 4];
        const ChordType *type = knowledge_snapshot_chord_type(snapshot, id);
        if (type == NULL) {
            if (message != NULL && size > 0) {
                snprintf(message, size, "common practice materials: %s is not in the knowledge snapshot", id);
            }
            return false;
        }
        if (i < 4) {
            triads[i] = type;
        } else {
            sevenths[i - 4] = type;
        }
    }
    out->major = found[0];
    out->natural_minor = found[1];
    out->harmonic_minor = found[2];
    out->melodic_minor = found[3];
    memcpy(out->triad_types, triads, sizeof(triads));
    memcpy(out->seventh_types, sevenths, sizeof(sevenths));
    return true;
}

static bool type_semitones(const ChordType *type, size_t count, int *out) {
    if (type == NULL || type->member_count != count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        out[i] = type->members[i].semitones;
    }
    return true;
}

bool common_practice_triad_semitones(const CommonPracticeMaterials *materials, StandardTonalityTriadQuality quality, int out[3]) {
    if (materials == NULL) {
        return standard_tonality_triad_quality_semitones(quality, out);
    }
    return (int)quality >= 0 && (int)quality < 4 && type_semitones(materials->triad_types[quality], 3, out);
}

bool common_practice_seventh_semitones(const CommonPracticeMaterials *materials, StandardTonalitySeventhQuality quality, int out[4]) {
    if (materials == NULL) {
        return standard_tonality_seventh_quality_semitones(quality, out);
    }
    return (int)quality >= 0 && (int)quality < 5 && type_semitones(materials->seventh_types[quality], 4, out);
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
