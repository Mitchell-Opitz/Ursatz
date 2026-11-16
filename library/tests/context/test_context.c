#include <assert.h>
#include <stdio.h>

#include "context/context.h"
#include "identity/entity_id.h"

/*
 * Structural test: a Context is composable/nested -- a child Context can be built over a parent Context that stays
 * the caller's, and destroying the child leaves the parent usable (TDD's Context composability requirement). A
 * Context needs an id and a non-empty label.
 */

static EntityID *make_id(const char *id_value) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    return id;
}

static Context *make_context(const char *id_value, const char *label, const Context *parent) {
    Context *context;
    assert(context_create(make_id(id_value), label, parent, &context) == CONTEXT_OK);
    return context;
}

int main(void) {
    Context *root = make_context("context-root", "exposition", NULL);
    Context *child = make_context("context-child", "measures 4-8", root);
    Context *grandchild = make_context("context-grandchild", "measure 6", child);

    /* The parent is referenced, not owned: children go first and the parent is destroyed by its caller. */
    context_destroy(grandchild);
    context_destroy(child);
    Context *sibling = make_context("context-sibling", "measures 9-12", root);
    context_destroy(sibling);
    context_destroy(root);

    /* A missing id or an empty label is rejected, and the id stays with the caller. */
    Context *rejected = NULL;
    assert(context_create(NULL, "exposition", NULL, &rejected) == CONTEXT_ERR_NULL_ID);
    EntityID *id = make_id("context-rejected");
    assert(context_create(id, NULL, NULL, &rejected) == CONTEXT_ERR_NULL_LABEL);
    assert(context_create(id, "", NULL, &rejected) == CONTEXT_ERR_NULL_LABEL);
    assert(rejected == NULL);
    entity_id_destroy(id);

    context_destroy(NULL);

    printf("test_context: ok\n");
    return 0;
}
