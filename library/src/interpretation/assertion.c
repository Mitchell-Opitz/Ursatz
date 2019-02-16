#include "interpretation/assertion.h"

#include <stdlib.h>
#include <string.h>

struct Assertion {
    EntityID *id;
    EntityID *subject_id;
    Value *claim;
    char *agent_identifier;
};

AssertionResult assertion_create(EntityID *id, EntityID *subject_id, Value *claim, const char *agent_identifier,
                                  Assertion **out) {
    if (id == NULL) {
        return ASSERTION_ERR_NULL_ID;
    }
    if (subject_id == NULL) {
        return ASSERTION_ERR_NULL_SUBJECT_ID;
    }
    if (claim == NULL) {
        return ASSERTION_ERR_NULL_CLAIM;
    }
    if (agent_identifier == NULL || agent_identifier[0] == '\0') {
        return ASSERTION_ERR_NULL_AGENT_IDENTIFIER;
    }

    Assertion *assertion = (Assertion *)malloc(sizeof(Assertion));
    if (assertion == NULL) {
        return ASSERTION_ERR_ALLOC;
    }

    size_t len = strlen(agent_identifier);
    assertion->agent_identifier = (char *)malloc(len + 1);
    if (assertion->agent_identifier == NULL) {
        free(assertion);
        return ASSERTION_ERR_ALLOC;
    }
    memcpy(assertion->agent_identifier, agent_identifier, len + 1);

    assertion->id = id;
    assertion->subject_id = subject_id;
    assertion->claim = claim;

    *out = assertion;
    return ASSERTION_OK;
}

void assertion_destroy(Assertion *assertion) {
    if (assertion == NULL) {
        return;
    }
    entity_id_destroy(assertion->id);
    entity_id_destroy(assertion->subject_id);
    value_destroy(assertion->claim);
    free(assertion->agent_identifier);
    free(assertion);
}

const EntityID *assertion_id(const Assertion *assertion) {
    return assertion->id;
}

const EntityID *assertion_subject_id(const Assertion *assertion) {
    return assertion->subject_id;
}

const Value *assertion_claim(const Assertion *assertion) {
    return assertion->claim;
}

const char *assertion_agent_identifier(const Assertion *assertion) {
    return assertion->agent_identifier;
}