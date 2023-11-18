#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CLAIM_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CLAIM_H

#include <stddef.h>

#include "interpretation/interpretation.h"

/*
 * Builds one standard-harmony Interpretation (the durable claim pattern: a generic Interpretation targeting an
 * existing object by id). FrameworkReference standard-harmony; the claim is a Value of `claim_type` holding a
 * copy of `content`; the uncertainty is the scalar confidence 1 - `uncertainty` in millionths; status
 * MACHINE_GENERATED; author "standard-harmony-v1"; method as given.
 *
 *   interpretation_id  the Interpretation's own id (non-empty)
 *   target_id          the id of the object it is about: a segment's Sonority id, or a Relationship's id
 *
 * Returns NULL for a NULL or empty argument, an uncertainty outside [0, 1] or on allocation failure. Nothing the
 * caller passed is retained or freed.
 */
Interpretation *harmony_analysis_make_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty, const char *method);

#endif
