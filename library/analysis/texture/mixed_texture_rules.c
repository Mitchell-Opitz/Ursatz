#include "analysis/texture/mixed_texture_rules.h"

#include <stdio.h>
#include <string.h>

#include "model/core/provenance_reference.h"
#include "model/core/value.h"
#include "analysis/texture/framework.h"

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

