#include "texture_classification_rules.h"

#include <stddef.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_TEXTURE_CLASSIFICATION_RULE_COUNT] = {
    "texture-classification-features", "texture-classification-matching", "texture-classification-precedence"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_texture_framework, "standard-texture",
    NAMES, STANDARD_TEXTURE_CLASSIFICATION_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_texture_classification, StandardTextureClassificationRule, STANDARD_TEXTURE_CLASSIFICATION_RULE_COUNT, g_table)

bool standard_texture_features_valid(const StandardTextureFeatures *features) {
    if (features == NULL || features->voice_count < 1) {
        return false;
    }
    if (features->voice_count == 1 &&
        (features->shared_melody || features->predominant_melody || features->independent_lines)) {
        return false;
    }
    if (features->unison && (features->shared_melody || features->predominant_melody)) {
        return false;
    }
    if (features->independent_lines &&
        (features->unison || features->rhythmic_unison || features->shared_melody)) {
        return false;
    }
    return true;
}

StandardTextureSet standard_texture_matching(const StandardTextureFeatures *features) {
    if (!standard_texture_features_valid(features)) {
        return 0;
    }
    bool several = features->voice_count >= 2;

    StandardTextureSet set = 0;
    if (features->voice_count == 1 || features->unison) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_MONOPHONY);
    }
    if (several && features->shared_melody) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_HETEROPHONY);
    }
    if (several && features->independent_lines) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_POLYPHONY);
    }
    if (several && features->rhythmic_unison && !features->unison) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_HOMORHYTHM);
    }
    if (several && features->predominant_melody) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT);
    }
    if ((set & (STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_HOMORHYTHM) |
                STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT))) != 0) {
        set |= STANDARD_TEXTURE_BIT(STANDARD_TEXTURE_HOMOPHONY);
    }
    return set;
}

bool standard_texture_set_contains(StandardTextureSet set, StandardTexture texture) {
    if (texture < 0 || texture >= STANDARD_TEXTURE_COUNT) {
        return false;
    }
    return (set & STANDARD_TEXTURE_BIT(texture)) != 0;
}

bool standard_texture_classify(const StandardTextureFeatures *features, StandardTexture *out) {
    if (out == NULL) {
        return false;
    }
    StandardTextureSet set = standard_texture_matching(features);
    static const StandardTexture ORDER[] = {STANDARD_TEXTURE_MONOPHONY, STANDARD_TEXTURE_HETEROPHONY,
                                            STANDARD_TEXTURE_POLYPHONY, STANDARD_TEXTURE_HOMORHYTHM,
                                            STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT};
    for (size_t i = 0; i < sizeof(ORDER) / sizeof(ORDER[0]); i++) {
        if (standard_texture_set_contains(set, ORDER[i])) {
            *out = ORDER[i];
            return true;
        }
    }
    return false;
}
