#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_CLAIM_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_CLAIM_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/interpretation.h"
#include "relationship/relationship.h"

/*
 * The durable claim pattern for standard-texture's analysis: an Interpretation (FrameworkReference standard-texture) targeting a
 * passage id, wrapped in a Texture, or targeting the id of a Relationship between two passages. Nothing the caller passes is retained.
 */

/* An Interpretation: FrameworkReference standard-texture, a claim Value of `claim_type` holding a copy of `content`, scalar
   confidence 1 - `uncertainty` in millionths, status MACHINE_GENERATED, author "standard-texture-v1", the given method; it targets
   `target_id`. NULL on a bad argument or failure. */
Interpretation *standard_texture_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                         const char *claim_type, const void *content, size_t size,
                                                         double uncertainty, const char *method);

/* A Relationship "standard-texture-<kind>-<key>" from the entity `source_id` to `target_id` with the predicate name. NULL on a bad
   argument or failure. */
Relationship *standard_texture_analysis_relationship(const char *kind, const char *key, const char *source_id, const char *target_id,
                                                     const char *predicate);

#endif
