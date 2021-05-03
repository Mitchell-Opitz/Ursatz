#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_MIXED_TEXTURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_MIXED_TEXTURE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "interpretation/texture.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "texture_rules.h"

/*
 * Mixed texture rules for standard-texture-v1. Most musical works do not
 * keep to one texture: one may open with a solo voice, then change to a
 * melody with accompaniment, and there are many other possibilities. A
 * work's textures over time are an ordered list of segments, each a
 * passage and the texture it has:
 *
 *   SEGMENTS  a segment is a passage (an EntityID, whatever the caller
 *             takes a passage to be) and one of the textures
 *             (texture_rules.h). A list of segments is in order, the
 *             first passage first, and holds at least one segment
 *   CHANGES   the texture changes where a segment's texture differs from
 *             the one before it. A change from homorhythm to melody and
 *             accompaniment is a change of texture but not of category
 *             (both are homophony); the rules count changes either way.
 *             A work is mixed when its texture changes by category
 *   LABEL     a segment's texture, as a Texture
 *             (src/interpretation/texture.h), is labeled with the
 *             texture's name
 *
 * No container type is added for a work's texture history: the list is
 * what a caller keeps, the same way key and chord results are lists of
 * the interpretations themselves.
 */

typedef struct {
    const EntityID *passage_id;
    StandardTexture texture;
} StandardTextureSegment;

typedef enum {
    STANDARD_TEXTURE_MIXED_RULE_SEGMENTS = 0,
    STANDARD_TEXTURE_MIXED_RULE_CHANGES,
    STANDARD_TEXTURE_MIXED_RULE_LABEL,
    STANDARD_TEXTURE_MIXED_RULE_COUNT
} StandardTextureMixedRule;

const Rule *standard_texture_mixed_rule(StandardTextureMixedRule which);
const Constraint *standard_texture_mixed_constraint(StandardTextureMixedRule which);

/* SEGMENTS: true when the list holds at least one segment and every
   segment has a passage and a valid texture. False for NULL. */
bool standard_texture_segments_valid(const StandardTextureSegment *segments, size_t count);

/*
 * CHANGES: the places the texture changes: indices i (1 or more) where
 * segment i has a different texture than segment i - 1, or, with
 * by_category, a different category (texture_rules.h). Returns how many
 * there are and writes the first `capacity` of them, in order, to
 * indices (which may be NULL). 0 for an invalid list.
 */
size_t standard_texture_change_points(const StandardTextureSegment *segments, size_t count, bool by_category,
                                      size_t *indices, size_t capacity);

/* CHANGES: how many different textures (or categories) the segments use.
   0 for an invalid list. */
size_t standard_texture_distinct_count(const StandardTextureSegment *segments, size_t count, bool by_category);

/* CHANGES: true when the work's texture changes by category. False for
   an invalid list and for a single texture throughout. */
bool standard_texture_is_mixed(const StandardTextureSegment *segments, size_t count);

/*
 * LABEL: builds a real, populated Texture labeled with the texture's name,
 * claiming that the passage named by passage_id has that texture under
 * standard-texture-v1. Takes ownership of passage_id on success
 * (destroyed with the Texture); on failure the caller still owns it. An
 * invalid texture is TEXTURE_ERR_NULL_LABEL; other TextureResult errors
 * as in interpretation/texture.h.
 */
TextureResult standard_texture_build_texture(StandardTexture texture, EntityID *passage_id, Texture **out);

#endif
