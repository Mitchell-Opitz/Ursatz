/* Pretty printing of the analysis document (2-space indent; cJSON does all escaping). */

#include "export/analysis_document_internal.h"

static bool buffer_indent(CommonText *buffer, int depth) {
    for (int i = 0; i < depth; i++) {
        if (!common_text_append(buffer, "  ")) {
            return false;
        }
    }
    return true;
}

static bool print_scalar(CommonText *buffer, const cJSON *item) {
    char *text = cJSON_PrintUnformatted(item);
    if (!text) {
        return false;
    }
    bool ok = common_text_append(buffer, text);
    cJSON_free(text);
    return ok;
}

static bool print_key(CommonText *buffer, const char *key) {
    cJSON *key_item = cJSON_CreateString(key);
    if (!key_item) {
        return false;
    }
    bool ok = print_scalar(buffer, key_item) && common_text_append(buffer, ": ");
    cJSON_Delete(key_item);
    return ok;
}

static bool is_container(const cJSON *item) {
    return cJSON_IsObject(item) || cJSON_IsArray(item);
}

/* Objects print one member per line. Arrays of scalars (note_ids) print
   on one line; arrays holding objects print one element per line. */
bool doc_print_value(CommonText *buffer, const cJSON *item, int depth) {
    if (!is_container(item)) {
        return print_scalar(buffer, item);
    }
    bool is_object = cJSON_IsObject(item);
    const char *close = is_object ? "}" : "]";
    if (!item->child) {
        return common_text_append(buffer, is_object ? "{" : "[") && common_text_append(buffer, close);
    }

    bool multiline = is_object;
    for (const cJSON *child = item->child; child && !multiline; child = child->next) {
        multiline = is_container(child);
    }
    if (!multiline) {
        if (!common_text_append(buffer, "[")) {
            return false;
        }
        for (const cJSON *child = item->child; child; child = child->next) {
            if ((child != item->child && !common_text_append(buffer, ", ")) || !print_scalar(buffer, child)) {
                return false;
            }
        }
        return common_text_append(buffer, "]");
    }

    if (!common_text_append(buffer, is_object ? "{\n" : "[\n")) {
        return false;
    }
    for (const cJSON *child = item->child; child; child = child->next) {
        if (!buffer_indent(buffer, depth + 1) || (is_object && !print_key(buffer, child->string)) ||
            !doc_print_value(buffer, child, depth + 1) || !common_text_append(buffer, child->next ? ",\n" : "\n")) {
            return false;
        }
    }
    return buffer_indent(buffer, depth) && common_text_append(buffer, close);
}
