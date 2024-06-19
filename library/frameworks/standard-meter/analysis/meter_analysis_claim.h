#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_CLAIM_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_CLAIM_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/interpretation.h"
#include "relationship/relationship.h"
#include "structure/structure.h"

/*
 * The durable claim pattern for standard-meter's rhythm and meter analysis: an Interpretation targeting a note id, a Structure
 * shell over several note ids (an Interpretation targeting the shell's id), or a Relationship between two entity ids (an
 * Interpretation targeting the Relationship's id). Nothing the caller passes is retained.
 */

/* A Structure with the given note ids as event refs (duplicates removed) under the id `shell_id`. NULL on a NULL or empty
   argument or on allocation failure. */
Structure *standard_meter_analysis_group(const char *shell_id, const char *const *note_ids, size_t count);

/* An Interpretation: FrameworkReference standard-meter, a claim Value of `claim_type` holding a copy of `content`, scalar
   confidence 1 - `uncertainty` in millionths, status MACHINE_GENERATED, author "standard-meter-v1", the given method; it targets
   `target_id`. NULL on a bad argument or failure. */
Interpretation *standard_meter_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty, const char *method);

/* A shell "standard-meter-<kind>-notes-<key>" over the note ids and the Interpretation "standard-meter-<kind>-<key>" targeting
   it. On true both are owned by the caller; on false neither is. */
bool standard_meter_analysis_emit(const char *kind, const char *key, const char *const *note_ids, size_t note_count,
                                 const char *claim_type, const void *claim, size_t claim_size, double uncertainty,
                                 Structure **shell, Interpretation **interpretation);

/* A Relationship "standard-meter-<kind>-<key>" from the entity `source_id` to `target_id` with the predicate name. NULL on a bad
   argument or failure. */
Relationship *standard_meter_analysis_relationship(const char *kind, const char *key, const char *source_id,
                                                  const char *target_id, const char *predicate);

#endif
