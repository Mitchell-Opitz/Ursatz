#include "mixed_texture_rules.h"

#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "interpretation/uncertainty.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TEXTURE_MIXED_RULE_COUNT] = {"mixed-texture-segments", "mixed-texture-changes",
                                                                      "mixed-texture-label"};

static StandardTextureRulePair g_pairs[STANDARD_TEXTURE_MIXED_RULE_COUNT];
static bool g_built[STANDARD_TEXTURE_MIXED_RULE_COUNT];

static bool valid_rule(StandardTextureMixedRule which) {
    return which >= 0 && which < STANDARD_TEXTURE_MIXED_RULE_COUNT;
}

static void ensure_built(StandardTextureMixedRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_texture_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_texture_mixed_rule(StandardTextureMixedRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_texture_mixed_constraint(StandardTextureMixedRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_texture_segments_valid(const StandardTextureSegment *segments, size_t count) {
    if (segments == NULL || count == 0) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (segments[i].passage_id == NULL || standard_texture_name(segments[i].texture) == NULL) {
            return false;
        }
    }
    return true;
}

/* The texture, or its main category when by_category. */
static StandardTexture key_of(StandardTexture texture, bool by_category) {
    StandardTexture category = texture;
    if (by_category) {
        standard_texture_category(texture, &category);
    }
    return category;
}

size_t standard_texture_change_points(const StandardTextureSegment *segments, size_t count, bool by_category,
                                      size_t *indices, size_t capacity) {
    if (!standard_texture_segments_valid(segments, count)) {
        return 0;
    }
    size_t found = 0;
    for (size_t i = 1; i < count; i++) {
        if (key_of(segments[i].texture, by_category) != key_of(segments[i - 1].texture, by_category)) {
            if (indices != NULL && found < capacity) {
                indices[found] = i;
            }
            found++;
        }
    }
    return found;
}

size_t standard_texture_distinct_count(const StandardTextureSegment *segments, size_t count, bool by_category) {
    if (!standard_texture_segments_valid(segments, count)) {
        return 0;
    }
    bool seen[STANDARD_TEXTURE_COUNT] = {false};
    size_t distinct = 0;
    for (size_t i = 0; i < count; i++) {
        StandardTexture key = key_of(segments[i].texture, by_category);
        if (!seen[key]) {
            seen[key] = true;
            distinct++;
        }
    }
    return distinct;
}

bool standard_texture_is_mixed(const StandardTextureSegment *segments, size_t count) {
    return standard_texture_change_points(segments, count, true, NULL, 0) > 0;
}

TextureResult standard_texture_build_texture(StandardTexture texture, EntityID *passage_id, Texture **out) {
    const char *label = standard_texture_name(texture);
    if (label == NULL) {
        return TEXTURE_ERR_NULL_LABEL;
    }

    const Framework *framework = standard_texture_framework();
    if (framework == NULL) {
        return TEXTURE_ERR_ALLOC;
    }

    char interpretation_id_value[256];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "standard-texture-%s@%s", label,
             entity_id_value(passage_id));

    EntityID *interpretation_id;
    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK) {
        return TEXTURE_ERR_ALLOC;
    }

    EntityID *framework_target_id;
    if (entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }
    FrameworkReference *framework_reference;
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        entity_id_destroy(framework_target_id);
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }

    Value *claim;
    if (value_create("standard-texture/texture", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }

    Interpretation *interpretation;
    InterpretationResult interpretation_result =
        interpretation_create(interpretation_id, passage_id, claim, NULL, NULL, framework_reference, uncertainty,
                               "standard-texture-v1", "texture-vocabulary", INTERPRETATION_STATUS_MACHINE_GENERATED,
                               provenance, &interpretation);
    if (interpretation_result != INTERPRETATION_OK) {
        provenance_reference_destroy(provenance);
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return TEXTURE_ERR_ALLOC;
    }

    TextureResult texture_result = texture_create(interpretation, label, out);
    if (texture_result != TEXTURE_OK) {
        interpretation_destroy(interpretation);
        return texture_result;
    }
    return TEXTURE_OK;
}
