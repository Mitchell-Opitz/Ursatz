#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_CLAIM_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_CLAIM_H

#include <stddef.h>

#include "interpretation/interpretation.h"
#include "structure/structure.h"

/*
 * The durable claim pattern for claims about notes: a Structure shell whose event refs are the involved note ids, and an
 * Interpretation targeting the shell's id. Both are built here; nothing the caller passes is retained.
 */

/* A Structure with the given note ids as event refs (duplicates removed), under the id `shell_id`. NULL on a NULL or
   empty argument or allocation failure. */
Structure *standard_counterpoint_analysis_note_group(const char *shell_id, const char *const *note_ids, size_t count);

/* An Interpretation: FrameworkReference standard-counterpoint, a claim Value of `claim_type` holding a copy of
   `content`, scalar confidence 1 - `uncertainty` in millionths, status MACHINE_GENERATED, author
   "standard-counterpoint-v1", the given method; it targets `target_id`. NULL on a bad argument or failure. */
Interpretation *standard_counterpoint_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                              const char *claim_type, const void *content,
                                                              size_t size, double uncertainty, const char *method);

#endif
