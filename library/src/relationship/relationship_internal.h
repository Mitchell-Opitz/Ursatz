#ifndef MUSIC_RELATIONSHIP_RELATIONSHIP_INTERNAL_H
#define MUSIC_RELATIONSHIP_RELATIONSHIP_INTERNAL_H

#include "relationship/relationship.h"

/* The fields of a Relationship: relationship.c writes them; tests read them through tests/support/relationship_test_access.h. */
struct Relationship {
    EntityID *id;
    EntityID *source_id;
    char *predicate_name;
    EntityID *target_id;
    Value *scope;
    EntityID *context_id;
    Value *qualifiers;
    EntityID *evidence_id;
    Value *uncertainty;
    ProvenanceReference *provenance;
};

#endif
