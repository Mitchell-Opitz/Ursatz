#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "context/context.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/intent.h"
#include "generation/intent_compiler.h"
#include "identity/entity_id.h"

/*
 * Interface-contract test: a concrete IntentCompiler is a name plus an
 * IntentCompilerFn, invoked as compile(request_text, context, config)
 * -> out (TDD Sec154). This mock compiler stands in for a real
 * NLP-backed one (out of scope for this pass) purely to exercise the
 * interface's shape: request_text/context/config are borrowed (never
 * destroyed by the compiler), a NULL request_text is rejected without
 * touching *out, and a successful call hands back an Intent the caller
 * owns.
 *
 * Compilation-regression test: a fixed request always produces the same
 * Intent fields -- pinning the mock compiler's behavior so a future
 * refactor of the interface plumbing cannot silently change what a
 * caller receives.
 */

static IntentCompilerResult mock_compile(const char *request_text, const Context *context, const Value *config,
                                          Intent **out) {
    (void)context;
    (void)config;

    if (request_text == NULL || request_text[0] == '\0') {
        return INTENT_COMPILER_ERR_NULL_REQUEST_TEXT;
    }
    if (out == NULL) {
        return INTENT_COMPILER_ERR_NULL_OUT;
    }

    EntityID *id;
    if (entity_id_create("intent-mock-1", &id) != ENTITY_ID_OK) {
        return INTENT_COMPILER_ERR_ALLOC;
    }
    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(id);
        return INTENT_COMPILER_ERR_ALLOC;
    }

    Intent *intent;
    if (intent_create(id, "counter-melody", "8-measure phrase", NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL,
                       provenance, &intent) != INTENT_OK) {
        return INTENT_COMPILER_ERR_COMPILATION_FAILED;
    }

    *out = intent;
    return INTENT_COMPILER_OK;
}

int main(void) {
    /* IntentCompiler interface contract: a concrete compiler is name + IntentCompilerFn. */
    IntentCompiler mock = {"mock-compiler", mock_compile};
    assert(strcmp(mock.name, "mock-compiler") == 0);

    /* NULL/empty request_text is rejected without touching *out. */
    Intent *untouched = (Intent *)0x1;
    assert(mock.compile(NULL, NULL, NULL, &untouched) == INTENT_COMPILER_ERR_NULL_REQUEST_TEXT);
    assert(untouched == (Intent *)0x1);
    assert(mock.compile("", NULL, NULL, &untouched) == INTENT_COMPILER_ERR_NULL_REQUEST_TEXT);
    assert(untouched == (Intent *)0x1);

    /* Compilation-regression: a fixed request always yields the same Intent fields. */
    Intent *intent;
    assert(mock.compile("write me a counter-melody for an 8-measure phrase", NULL, NULL, &intent) ==
           INTENT_COMPILER_OK);
    assert(strcmp(intent_purpose(intent), "counter-melody") == 0);
    assert(strcmp(intent_target(intent), "8-measure phrase") == 0);

    intent_destroy(intent);

    printf("test_intent_compiler: ok\n");
    return 0;
}