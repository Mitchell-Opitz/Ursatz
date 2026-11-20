#include <ursatz_composer/form_spec.h>
#include <common/hash.h>

#include <stdio.h>
#include <string.h>

/* form/form_builtin_data.c, generated at build time from the files in forms/ by cmake/EmbedForms.cmake. */
extern const char *const COMPOSER_FORM_DOCUMENTS[];
extern const char *const COMPOSER_FORM_NAMES[];
extern const char *const COMPOSER_FORM_PLAN_NAMES[];
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

const char *form_spec_builtin_plan_name(size_t index) {
    return index < COMPOSER_FORM_DOCUMENT_COUNT ? COMPOSER_FORM_PLAN_NAMES[index] : NULL;
}

/* FNV-1a 64 of the document's bytes as embedded, 16 lowercase hexadecimal digits; the text is one buffer per call. */
const char *form_spec_builtin_hash(size_t index) {
    static char hashes[FORM_SPEC_BUILTIN_MAX][FORM_SPEC_HASH_LEN + 1];
    if (index >= COMPOSER_FORM_DOCUMENT_COUNT || index >= FORM_SPEC_BUILTIN_MAX) {
        return NULL;
    }
    if (hashes[index][0] == '\0') {
        common_fnv1a64_hex(common_fnv1a64(COMPOSER_FORM_DOCUMENTS[index], strlen(COMPOSER_FORM_DOCUMENTS[index]), COMMON_FNV1A64_OFFSET),
                           hashes[index]);
    }
    return hashes[index];
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
