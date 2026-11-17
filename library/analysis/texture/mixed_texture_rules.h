#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_MIXED_TEXTURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_MIXED_TEXTURE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "model/identity/entity_id.h"
#include "model/interpretation/texture.h"
#include "analysis/texture/texture_rules.h"

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

#endif
