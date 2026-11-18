#include "theory/style/style_model_file.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "model/identity/entity_id.h"
#include "theory/style/style_labels.h"
#include "theory/style/style_model_build.h"

#define FNV1A_OFFSET_BASIS 14695981039346656037ULL
#define FNV1A_PRIME 1099511628211ULL

/* Folds length bytes into a 64-bit FNV-1a hash. */
static uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < length; i++) {
        hash = (hash ^ bytes[i]) * FNV1A_PRIME;
    }
    return hash;
}

/* The whole file in a new NUL-terminated buffer; NULL when it cannot be read. */
static char *read_all(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    size_t size = 0;
    if (fseek(file, 0, SEEK_END) == 0) {
        long length = ftell(file);
        if (length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
            text = malloc((size_t)length + 1);
            size = text ? fread(text, 1, (size_t)length, file) : 0;
            if (text) {
                text[size] = '\0';
            }
        }
    }
    fclose(file);
    return text;
}

static const char *string_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring[0] ? item->valuestring : NULL;
}

static bool count_member(const cJSON *object, const char *name, unsigned long *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble != (double)(unsigned long)item->valuedouble) {
        return false;
    }
    *out = (unsigned long)item->valuedouble;
    return true;
}

StyleModelFileResult style_model_file_read(const char *path, StyleModel **out, size_t *pieces) {
    return style_model_file_read_without(path, NULL, 0, out, pieces);
}

StyleModelFileResult style_model_file_read_without(const char *path, const char *const *categories, size_t dropped,
                                                   StyleModel **out, size_t *pieces) {
    if (!path || !out || (dropped > 0 && !categories)) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < dropped; i++) {
        StyleDimension unused;
        if (!style_category_dimension(categories[i], &unused)) {
            return STYLE_MODEL_FILE_ERR_ARGUMENT;
        }
    }
    *out = NULL;
    char *text = read_all(path);
    if (!text) {
        return STYLE_MODEL_FILE_ERR_IO;
    }
    cJSON *root = cJSON_Parse(text);
    uint64_t hash = fnv1a_bytes(FNV1A_OFFSET_BASIS, text, strlen(text));
    free(text);

    StyleModelFileResult result = STYLE_MODEL_FILE_ERR_FORMAT;
    StyleStatistic *statistics = NULL;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "statistics");
    const char *schema = string_member(root, "schema");
    const char *name = string_member(root, "style_name");
    const char *corpus = string_member(root, "training_corpus");
    const char *version = string_member(root, "model_version");
    unsigned long schema_version = 0, piece_count = 0;
    if (!schema || strcmp(schema, STYLE_MODEL_FILE_SCHEMA) != 0 || !count_member(root, "schema_version", &schema_version) ||
        schema_version != STYLE_MODEL_FILE_SCHEMA_VERSION || !name || !corpus || !version || !count_member(root, "pieces", &piece_count) ||
        !cJSON_IsArray(list)) {
        goto done;
    }
    size_t count = (size_t)cJSON_GetArraySize(list);
    statistics = count ? calloc(count, sizeof *statistics) : NULL;
    if (count && !statistics) {
        result = STYLE_MODEL_FILE_ERR_ALLOC;
        goto done;
    }
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        const char *dimension_name = string_member(entry, "dimension");
        const char *category = string_member(entry, "category");
        const char *label = string_member(entry, "label");
        StyleDimension named, actual;
        unsigned long entry_count, entry_total;
        if (!dimension_name || !category || !label || !style_dimension_from_name(dimension_name, &named) ||
            !style_category_dimension(category, &actual) || named != actual || !count_member(entry, "count", &entry_count) ||
            !count_member(entry, "total", &entry_total)) {
            goto done;
        }
        statistics[index++] = (StyleStatistic){category, label, entry_count, entry_total};
    }
    /* Statistics are sorted by category, so each category's counts are adjacent; the total is their sum. */
    for (size_t start = 0, i = 0; i <= count; i++) {
        if (i == count || strcmp(statistics[i].category, statistics[start].category) != 0) {
            unsigned long sum = 0;
            for (size_t j = start; j < i; j++) {
                sum += statistics[j].count;
            }
            for (size_t j = start; j < i; j++) {
                if (statistics[j].total != sum) {
                    goto done;
                }
            }
            start = i;
        }
    }
    /* The named categories are dropped here, after the totals are checked, so a damaged file is refused either way. */
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        bool drop = false;
        for (size_t d = 0; d < dropped && !drop; d++) {
            drop = strcmp(statistics[i].category, categories[d]) == 0;
        }
        if (!drop) {
            statistics[kept++] = statistics[i];
        }
    }
    count = kept;
    char id_text[40];
    snprintf(id_text, sizeof id_text, "style-model-%016llx", (unsigned long long)hash);
    EntityID *id = NULL;
    if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
        result = STYLE_MODEL_FILE_ERR_ALLOC;
        goto done;
    }
    StyleModelBuildResult built = style_model_from_statistics(id, name, corpus, version, statistics, count, out);
    if (built != STYLE_MODEL_BUILD_OK) {
        entity_id_destroy(id);
        result = built == STYLE_MODEL_BUILD_ERR_ALLOC ? STYLE_MODEL_FILE_ERR_ALLOC : STYLE_MODEL_FILE_ERR_FORMAT;
        goto done;
    }
    if (pieces) {
        *pieces = (size_t)piece_count;
    }
    result = STYLE_MODEL_FILE_OK;
done:
    free(statistics);
    cJSON_Delete(root);
    return result;
}

StyleModelFileResult style_model_statistics(const StyleModel *model, StyleStatistic **out, size_t *count) {
    if (!model || !out || !count) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    StyleFeaturesResult result = style_features_decode(style_model_style_features(model), out, count);
    if (result == STYLE_FEATURES_OK) {
        return STYLE_MODEL_FILE_OK;
    }
    return result == STYLE_FEATURES_ERR_ALLOC ? STYLE_MODEL_FILE_ERR_ALLOC : STYLE_MODEL_FILE_ERR_FORMAT;
}
