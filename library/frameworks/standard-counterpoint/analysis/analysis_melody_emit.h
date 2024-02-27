#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_EMIT_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/interpretation.h"
#include "structure/structure.h"

/*
 * The one place the melody analyses build their durable claims (a note-id shell and an Interpretation targeting it), so the
 * pattern is the same in every melody claim. Internal to the melody adapter.
 */

/* Builds the shell "standard-counterpoint-melody-notes-<key>" over `note_ids` and the Interpretation
   "standard-counterpoint-<kind>-<key>" targeting it (claim `claim_type` holding a copy of `claim`, uncertainty as in
   analysis_claim.h). On true, both are owned by the caller; on false, neither is. */
bool standard_counterpoint_melody_emit(const char *kind, const char *key, const char *const *note_ids,
                                       size_t note_count, const char *claim_type, const void *claim,
                                       size_t claim_size, double uncertainty, Structure **shell,
                                       Interpretation **interpretation);

#endif
