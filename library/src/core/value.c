#include "core/value.h"

#include <stdlib.h>
#include <string.h>

struct Value {
    char *type;
    void *content;
    size_t content_size;
};

ValueResult value_create(const char *type, const void *content, size_t content_size, Value **out) {
    if (type == NULL || type[0] == '\0') {
        return VALUE_ERR_NULL_TYPE;
    }
    if (content == NULL) {
        return VALUE_ERR_NULL_CONTENT;
    }

    Value *value = (Value *)malloc(sizeof(Value));
    if (value == NULL) {
        return VALUE_ERR_ALLOC;
    }

    size_t type_len = strlen(type);
    value->type = (char *)malloc(type_len + 1);
    if (value->type == NULL) {
        free(value);
        return VALUE_ERR_ALLOC;
    }
    memcpy(value->type, type, type_len + 1);

    value->content = malloc(content_size);
    if (value->content == NULL && content_size > 0) {
        free(value->type);
        free(value);
        return VALUE_ERR_ALLOC;
    }
    memcpy(value->content, content, content_size);
    value->content_size = content_size;

    *out = value;
    return VALUE_OK;
}

void value_destroy(Value *value) {
    if (value == NULL) {
        return;
    }
    free(value->type);
    free(value->content);
    free(value);
}

const char *value_type(const Value *value) {
    return value->type;
}

const void *value_content(const Value *value, size_t *out_size) {
    if (out_size != NULL) {
        *out_size = value->content_size;
    }
    return value->content;
}

bool value_equals(const Value *a, const Value *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (strcmp(a->type, b->type) != 0) {
        return false;
    }
    if (a->content_size != b->content_size) {
        return false;
    }
    return memcmp(a->content, b->content, a->content_size) == 0;
}