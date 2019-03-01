#include "relationship/relationship.h"

#include <stdlib.h>
#include <string.h>

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

RelationshipResult relationship_create(EntityID *id, EntityID *source_id, const char *predicate_name,
                                        EntityID *target_id, Value *scope, EntityID *context_id,
                                        Value *qualifiers, EntityID *evidence_id, Value *uncertainty,
                                        ProvenanceReference *provenance, Relationship **out) {
    if (id == NULL) {
        return RELATIONSHIP_ERR_NULL_ID;
    }
    if (source_id == NULL) {
        return RELATIONSHIP_ERR_NULL_SOURCE_ID;
    }
    if (predicate_name == NULL || predicate_name[0] == '\0') {
        return RELATIONSHIP_ERR_NULL_PREDICATE;
    }
    if (target_id == NULL) {
        return RELATIONSHIP_ERR_NULL_TARGET_ID;
    }
    if (provenance == NULL) {
        return RELATIONSHIP_ERR_NULL_PROVENANCE;
    }

    Relationship *relationship = (Relationship *)malloc(sizeof(Relationship));
    if (relationship == NULL) {
        return RELATIONSHIP_ERR_ALLOC;
    }

    size_t len = strlen(predicate_name);
    relationship->predicate_name = (char *)malloc(len + 1);
    if (relationship->predicate_name == NULL) {
        free(relationship);
        return RELATIONSHIP_ERR_ALLOC;
    }
    memcpy(relationship->predicate_name, predicate_name, len + 1);

    relationship->id = id;
    relationship->source_id = source_id;
    relationship->target_id = target_id;
    relationship->scope = scope;
    relationship->context_id = context_id;
    relationship->qualifiers = qualifiers;
    relationship->evidence_id = evidence_id;
    relationship->uncertainty = uncertainty;
    relationship->provenance = provenance;

    *out = relationship;
    return RELATIONSHIP_OK;
}

void relationship_destroy(Relationship *relationship) {
    if (relationship == NULL) {
        return;
    }
    entity_id_destroy(relationship->id);
    entity_id_destroy(relationship->source_id);
    free(relationship->predicate_name);
    entity_id_destroy(relationship->target_id);
    value_destroy(relationship->scope);
    entity_id_destroy(relationship->context_id);
    value_destroy(relationship->qualifiers);
    entity_id_destroy(relationship->evidence_id);
    value_destroy(relationship->uncertainty);
    provenance_reference_destroy(relationship->provenance);
    free(relationship);
}

const EntityID *relationship_id(const Relationship *relationship) {
    return relationship->id;
}

const EntityID *relationship_source_id(const Relationship *relationship) {
    return relationship->source_id;
}

const char *relationship_predicate_name(const Relationship *relationship) {
    return relationship->predicate_name;
}

const EntityID *relationship_target_id(const Relationship *relationship) {
    return relationship->target_id;
}

const Value *relationship_scope(const Relationship *relationship) {
    return relationship->scope;
}

const EntityID *relationship_context_id(const Relationship *relationship) {
    return relationship->context_id;
}

const Value *relationship_qualifiers(const Relationship *relationship) {
    return relationship->qualifiers;
}

const EntityID *relationship_evidence_id(const Relationship *relationship) {
    return relationship->evidence_id;
}

const Value *relationship_uncertainty(const Relationship *relationship) {
    return relationship->uncertainty;
}

const ProvenanceReference *relationship_provenance(const Relationship *relationship) {
    return relationship->provenance;
}