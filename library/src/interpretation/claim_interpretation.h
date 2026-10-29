#ifndef URSATZ_INTERPRETATION_CLAIM_INTERPRETATION_H
#define URSATZ_INTERPRETATION_CLAIM_INTERPRETATION_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/interpretation.h"
#include "relationship/relationship.h"
#include "structure/structure.h"

/*
 * The durable claim pattern every framework shares: a Structure shell over note ids, an Interpretation of a framework's claim
 * targeting the shell (or a Relationship), and the Relationship itself. `framework_name` is the short name a framework uses in
 * its ids ("standard-form"); the author is "<framework_name>-v1". Nothing the caller passes is retained.
 */

/* An Interpretation: FrameworkReference `framework_id`, a claim Value of `claim_type` holding a copy of `content`, scalar
   confidence 1 - `uncertainty` in millionths, status MACHINE_GENERATED, author "<framework_name>-v1", the given method; it
   targets `target_id`. NULL on a bad argument or failure, with everything freed. */
Interpretation *claim_interpretation_create(const char *framework_id, const char *framework_name,
                                            const char *interpretation_id, const char *target_id, const char *claim_type,
                                            const void *content, size_t size, double uncertainty, const char *method);

/* A Structure of type "<framework_name>/note-group" with the given note ids as event refs (duplicates removed) under the id
   `shell_id`. NULL on a NULL or empty argument or on allocation failure. */
Structure *claim_interpretation_note_group(const char *framework_name, const char *shell_id, const char *const *note_ids,
                                           size_t count);

/* A Relationship "<framework_name>-<kind>-<key>" from the entity `source_id` to `target_id` with the predicate name. NULL on a
   bad argument or failure. */
Relationship *claim_interpretation_relationship(const char *framework_name, const char *kind, const char *key,
                                                const char *source_id, const char *target_id, const char *predicate);

#endif
