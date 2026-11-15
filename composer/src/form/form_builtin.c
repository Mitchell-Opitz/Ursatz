#include <ursatz_composer/form_spec.h>

#include <string.h>

/* form/form_builtin_data.c, generated at build time from the files in forms/ by cmake/EmbedForms.cmake. */
extern const char *const COMPOSER_FORM_DOCUMENTS[];
extern const char *const COMPOSER_FORM_NAMES[];
extern const size_t COMPOSER_FORM_DOCUMENT_COUNT;

size_t form_spec_builtin_count(void) {
    return COMPOSER_FORM_DOCUMENT_COUNT;
}

const char *form_spec_builtin_document(size_t index) {
    return index < COMPOSER_FORM_DOCUMENT_COUNT ? COMPOSER_FORM_DOCUMENTS[index] : NULL;
}

const char *form_spec_builtin_name(size_t index) {
    return index < COMPOSER_FORM_DOCUMENT_COUNT ? COMPOSER_FORM_NAMES[index] : NULL;
}

FormSpecResult form_spec_builtin(const char *name, FormSpec **out) {
    if (out) {
        *out = NULL;
    }
    if (!name || !out) {
        return FORM_SPEC_ERR_NULL;
    }
    for (size_t i = 0; i < COMPOSER_FORM_DOCUMENT_COUNT; i++) {
        if (strcmp(name, COMPOSER_FORM_NAMES[i]) == 0) {
            return form_spec_parse(COMPOSER_FORM_DOCUMENTS[i], out, NULL);
        }
    }
    return FORM_SPEC_ERR_UNKNOWN;
}

FormSpecResult form_spec_builtin_for_plan(const PiecePlan *plan, FormSpec **out) {
    if (out) {
        *out = NULL;
    }
    if (!plan || !out) {
        return FORM_SPEC_ERR_NULL;
    }
    NodeId root = plan_root(plan);
    const char *root_name = root == PLAN_NODE_NONE ? NULL : plan_node_name(plan, root);
    for (size_t i = 0; root_name && i < COMPOSER_FORM_DOCUMENT_COUNT; i++) {
        FormSpec *form = NULL;
        FormSpecResult parsed = form_spec_parse(COMPOSER_FORM_DOCUMENTS[i], &form, NULL);
        if (parsed != FORM_SPEC_OK) {
            return parsed;
        }
        if (strcmp(root_name, form->plan_name) == 0) {
            *out = form;
            return FORM_SPEC_OK;
        }
        form_spec_destroy(form);
    }
    return FORM_SPEC_ERR_UNKNOWN;
}
