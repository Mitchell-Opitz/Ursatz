#ifndef MUSIC_RELATIONSHIP_PREDICATE_DEFINITION_H
#define MUSIC_RELATIONSHIP_PREDICATE_DEFINITION_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * PredicateDefinition (Entity): metadata for a relationship predicate
 * (TDD §76).
 *
 * Fields: name, inverse, symmetric, transitive, allowed_source_types,
 * allowed_target_types, contextual.
 *
 * The initial predicate vocabulary (TDD §77: contains, precedes,
 * derived_from, realizes, resembles, interpreted_as, etc.) is
 * EXPLICITLY EXTENSIBLE, not a fixed enum -- the System Registry marks
 * this "No (explicitly extensible, not fixed)". Accordingly this type
 * is open/registrable: name is a caller-supplied string, not a value
 * drawn from a closed enum, and there is no fixed table of predicates
 * anywhere in this module.
 *
 * Following the rest of this codebase's convention for concrete
 * entities built on top of a base category (e.g. Motif relative to
 * Structure), PredicateDefinition flattens to its own bare EntityID id
 * rather than composing core/entity.h's Entity wrapper.
 *
 * allowed_source_types/allowed_target_types are open string lists
 * (type-name tags), not a closed type enum -- an empty list
 * (count == 0, NULL) means "unrestricted", not "unknown".
 *
 * A Relationship (relationship/relationship.h) references a predicate
 * by name (relationship_predicate_name) -- correlation to a
 * PredicateDefinition is by that name string, not a compile-time or
 * object reference, since a Relationship may exist before or without
 * the corresponding PredicateDefinition being registered.
 */

typedef struct PredicateDefinition PredicateDefinition;

typedef enum {
    PREDICATE_DEFINITION_OK = 0,
    PREDICATE_DEFINITION_ERR_NULL_ID,
    PREDICATE_DEFINITION_ERR_NULL_NAME,
    PREDICATE_DEFINITION_ERR_NULL_TYPE_ENTRY,
    PREDICATE_DEFINITION_ERR_ALLOC
} PredicateDefinitionResult;

/*
 * Takes ownership of id on success (destroyed by
 * predicate_definition_destroy). name is required (copied,
 * non-empty). inverse may be NULL (no known inverse predicate);
 * if non-NULL, copied.
 *
 * allowed_source_types/allowed_target_types may each be NULL with a
 * count of 0 (unrestricted); if a count is > 0 the corresponding array
 * must be non-NULL with no NULL entries -- each entry is copied.
 */
PredicateDefinitionResult predicate_definition_create(EntityID *id, const char *name, const char *inverse,
                                                       bool symmetric, bool transitive,
                                                       const char **allowed_source_types,
                                                       size_t allowed_source_type_count,
                                                       const char **allowed_target_types,
                                                       size_t allowed_target_type_count, bool contextual,
                                                       PredicateDefinition **out);
void predicate_definition_destroy(PredicateDefinition *definition);

const EntityID *predicate_definition_id(const PredicateDefinition *definition);
const char *predicate_definition_name(const PredicateDefinition *definition);
/* NULL when no inverse is known. */
const char *predicate_definition_inverse(const PredicateDefinition *definition);
bool predicate_definition_symmetric(const PredicateDefinition *definition);
bool predicate_definition_transitive(const PredicateDefinition *definition);

size_t predicate_definition_allowed_source_type_count(const PredicateDefinition *definition);
const char *predicate_definition_allowed_source_type_at(const PredicateDefinition *definition, size_t index);

size_t predicate_definition_allowed_target_type_count(const PredicateDefinition *definition);
const char *predicate_definition_allowed_target_type_at(const PredicateDefinition *definition, size_t index);

bool predicate_definition_contextual(const PredicateDefinition *definition);

#endif