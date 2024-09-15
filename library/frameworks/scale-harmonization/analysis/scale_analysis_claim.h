#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_ANALYSIS_SCALE_ANALYSIS_CLAIM_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_ANALYSIS_SCALE_ANALYSIS_CLAIM_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/interpretation.h"
#include "relationship/relationship.h"

/*
 * The durable claim pattern for scale-harmonization's analysis: an Interpretation (FrameworkReference scale-harmonization) targeting a
 * passage id, wrapped in a Texture, or targeting the id of a Relationship between two passages. Nothing the caller passes is retained.
 */

/* An Interpretation: FrameworkReference scale-harmonization, a claim Value of `claim_type` holding a copy of `content`, scalar
   confidence 1 - `uncertainty` in millionths, status MACHINE_GENERATED, author "scale-harmonization-v1", the given method; it targets
   `target_id`. NULL on a bad argument or failure. */
Interpretation *scale_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                         const char *claim_type, const void *content, size_t size,
                                                         double uncertainty, const char *method);

/* A Relationship "scale-harmonization-<kind>-<key>" from the entity `source_id` to `target_id` with the predicate name. NULL on a bad
   argument or failure. */
Relationship *scale_analysis_relationship(const char *kind, const char *key, const char *source_id, const char *target_id,
                                                     const char *predicate);

#endif
