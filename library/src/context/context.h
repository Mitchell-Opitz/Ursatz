#ifndef MUSIC_CONTEXT_CONTEXT_H
#define MUSIC_CONTEXT_CONTEXT_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Context (Category / base type): the conditions under which an
 * assertion is evaluated (music-relationship's Relationship and
 * music-interpretation's Interpretation both carry a context
 * reference back to this type).
 *
 * A Context is composable/nested -- it may have a parent Context, so
 * that a narrower context (e.g. "measures 4-8") can be evaluated
 * within a broader one (e.g. "the exposition"). This base type keeps
 * that composability plus a minimal identifying label; it does not
 * define what makes one context type "narrower" or "broader" than
 * another -- that is left to whatever builds on top of it.
 *
 * parent is a reference only, not owned: context_destroy never
 * destroys a Context's parent, since the same parent Context is
 * expected to be shared by (referenced from) more than one child.
 * Caller is responsible for the parent's lifetime.
 */

typedef struct Context Context;

typedef enum {
    CONTEXT_OK = 0,
    CONTEXT_ERR_NULL_ID,
    CONTEXT_ERR_NULL_LABEL,
    CONTEXT_ERR_ALLOC
} ContextResult;

/*
 * Takes ownership of id on success (destroyed by context_destroy).
 * label is copied. parent, if non-NULL, is referenced only -- not
 * owned, and must outlive this Context.
 */
ContextResult context_create(EntityID *id, const char *label, const Context *parent, Context **out);
void context_destroy(Context *context);

const EntityID *context_id(const Context *context);
const char *context_label(const Context *context);
const Context *context_parent(const Context *context);

bool context_identity_equals(const Context *a, const Context *b);

#endif