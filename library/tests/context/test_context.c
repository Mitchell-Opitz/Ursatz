#include <assert.h>
#include <stdio.h>

#include "context/context.h"
#include "identity/entity_id.h"

/*
 * Structural test: a Context is composable/nested -- a child Context
 * can carry a reference to a parent Context, and that reference is
 * retrievable unchanged (TDD's Context composability requirement).
 *
 * Context test: two different Contexts (different EntityID identity)
 * remain distinguishable from one another via context_identity_equals,
 * and a Context is equal to itself.
 */

static Context *make_context(const char *id_value, const char *label, const Context *parent) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);

    Context *context;
    assert(context_create(id, label, parent, &context) == CONTEXT_OK);
    return context;
}

int main(void) {
    Context *root = make_context("context-root", "exposition", NULL);
    assert(context_parent(root) == NULL);

    Context *child = make_context("context-child", "measures 4-8", root);
    assert(context_parent(child) == root);
    assert(context_identity_equals(context_parent(child), root));

    Context *other = make_context("context-other", "development", NULL);

    assert(context_identity_equals(root, root));
    assert(!context_identity_equals(root, other));
    assert(!context_identity_equals(child, other));

    context_destroy(child);
    context_destroy(other);
    context_destroy(root);

    printf("test_context: ok\n");
    return 0;
}