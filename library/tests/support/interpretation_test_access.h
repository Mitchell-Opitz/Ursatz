#ifndef URSATZ_TESTS_SUPPORT_INTERPRETATION_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_INTERPRETATION_TEST_ACCESS_H

#include "model/interpretation/interpretation.h"
#include "model/interpretation/interpretation_internal.h"
#include "model/interpretation/key.h"
#include "model/interpretation/key_internal.h"
#include "model/interpretation/texture.h"
#include "model/interpretation/texture_internal.h"

/*
 * What the tests read of an Interpretation, a Key and a Texture. No application reads these, so they are not in
 * interpretation/interpretation.h, key.h and texture.h.
 */

static inline const EntityID *interpretation_id(const Interpretation *interpretation) {
    return interpretation->id;
}

static inline const EntityID *interpretation_target_id(const Interpretation *interpretation) {
    return interpretation->target_id;
}

static inline const Value *interpretation_claim(const Interpretation *interpretation) {
    return interpretation->claim;
}

static inline const EntityID *interpretation_context_id(const Interpretation *interpretation) {
    return interpretation->context_id;
}

static inline const char *interpretation_author(const Interpretation *interpretation) {
    return interpretation->author;
}

static inline const char *interpretation_method(const Interpretation *interpretation) {
    return interpretation->method;
}

static inline const Interpretation *key_interpretation(const Key *key) {
    return key->interpretation;
}

static inline const char *key_label(const Key *key) {
    return key->label;
}

static inline const Interpretation *texture_interpretation(const Texture *texture) {
    return texture->interpretation;
}

static inline const char *texture_label(const Texture *texture) {
    return texture->label;
}

#endif
