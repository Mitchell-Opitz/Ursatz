#ifndef MUSIC_PERSISTENCE_PERSISTENCE_RELATIONSHIP_H
#define MUSIC_PERSISTENCE_PERSISTENCE_RELATIONSHIP_H

#include <stddef.h>

#include "persistence/persistence.h"
#include "relationship/relationship.h"

/*
 * RelationshipRepository: stores and loads Relationships (relationship/relationship.h) in the `relationships` table.
 * A stored Relationship is a descriptive record: it asserts nothing, and storing one promotes nothing to a Rule.
 *
 * References by EntityID only. A Relationship's source, target, context and evidence are EntityID text values. They
 * are NOT foreign keys and are not checked against any other table: a Relationship may point at any entity, in
 * any repository, stored before or after it, or never.
 *
 * Every field round trips: id, source, predicate name, target, scope, context id, qualifiers, evidence id, the
 * uncertainty Value and the provenance reference (present with its id, or absent). The three Values are stored as
 * type, content and size columns (the Interpretation claim pattern); an absent optional Value or id comes back
 * absent. The uncertainty here is the Relationship's own Value, not the Uncertainty type of an Interpretation, so it
 * does not use the `uncertainties` tables.
 *
 * Results. Storing an id that already exists is PERSISTENCE_RELATIONSHIP_ERR_DUPLICATE_ID: nothing is overwritten or
 * duplicated. (The older repositories report a duplicate key, and a missing id on load, as the generic ERR_DB; these
 * name them.) Loading an id that is not stored is PERSISTENCE_RELATIONSHIP_ERR_NOT_FOUND and *out is untouched.
 */

typedef enum {
    PERSISTENCE_RELATIONSHIP_OK = 0,
    PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG,
    PERSISTENCE_RELATIONSHIP_ERR_DB,
    PERSISTENCE_RELATIONSHIP_ERR_ALLOC,
    PERSISTENCE_RELATIONSHIP_ERR_NOT_FOUND,
    PERSISTENCE_RELATIONSHIP_ERR_DUPLICATE_ID
} PersistenceRelationshipResult;

PersistenceRelationshipResult persistence_store_relationship(PersistenceHandle *handle,
                                                             const Relationship *relationship);

PersistenceRelationshipResult persistence_load_relationship(PersistenceHandle *handle, const char *relationship_id,
                                                            Relationship **out_relationship);

#endif
