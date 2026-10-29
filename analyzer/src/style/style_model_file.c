#include <ursatz_analyzer/style_profile.h>
#include <ursatz_analyzer/version.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "identity/entity_id.h"
#include "theory/style_model_build.h"

#include "style_internal.h"

#define FILE_SCHEMA "ursatz-style-model"
#define FILE_SCHEMA_VERSION 1

static uint64_t fnv1a(const char *text) {
    uint64_t hash = 14695981039346656037ULL;
    for (; *text; text++) {
        hash = (hash ^ (unsigned char)*text) * 1099511628211ULL;
    }
    return hash;
}

StyleModelFileResult style_model_file_write(const StyleProfile *profile, const char *name, const char *path) {
    if (!profile || !name || !name[0] || !path) {
        return STYLE_MODEL_FILE_ERR_ARGUMENT;
    }
    StyleStatistic *statistics;
    size_t count;
    if (style_profile_statistics(profile, &statistics, &count) != STYLE_PROFILE_OK) {
        return STYLE_MODEL_FILE_ERR_ALLOC;
    }
    const char *corpus = style_profile_training_corpus(profile);
    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    bool ok = root && list;
    if (!ok) {
        cJSON_Delete(root);
        cJSON_Delete(list);
        free(statistics);
        return STYLE_MODEL_FILE_ERR_ALLOC;
    }
    {
        cJSON_AddItemToObject(root, "schema", cJSON_CreateString(FILE_SCHEMA));
        cJSON_AddItemToObject(root, "schema_version", cJSON_CreateNumber(FILE_SCHEMA_VERSION));
        cJSON_AddItemToObject(root, "style_name", cJSON_CreateString(name));
        cJSON_AddItemToObject(root, "training_corpus", cJSON_CreateString(corpus[0] ? corpus : "unspecified"));
        cJSON_AddItemToObject(root, "model_version", cJSON_CreateString(URSATZ_ANALYZER_VERSION));
        cJSON_AddItemToObject(root, "pieces", cJSON_CreateNumber((double)style_profile_piece_count(profile)));
        cJSON_AddItemToObject(root, "statistics", list);
    }
    for (size_t i = 0; ok && i < count; i++) {
        StyleDimension dimension;
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !style_category_dimension(statistics[i].category, &dimension)) {
            cJSON_Delete(entry);
            ok = false;
            break;
        }
        cJSON_AddItemToObject(entry, "dimension", cJSON_CreateString(style_dimension_name(dimension)));
        cJSON_AddItemToObject(entry, "category", cJSON_CreateString(statistics[i].category));
        cJSON_AddItemToObject(entry, "label", cJSON_CreateString(statistics[i].label));
        cJSON_AddItemToObject(entry, "count", cJSON_CreateNumber((double)statistics[i].count));
        cJSON_AddItemToObject(entry, "total", cJSON_CreateNumber((double)statistics[i].total));
        cJSON_AddItemToArray(list, entry);
    }
    free(statistics);
    char *text = ok ? cJSON_Print(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        return STYLE_MODEL_FILE_ERR_ALLOC;
    }
    FILE *file = fopen(path, "wb");
    bool wrote = file && fputs(text, file) >= 0 && fputc('\n', file) != EOF;
    if (file && fclose(file) != 0) {
        wrote = false;
    }
    free(text);
    return wrote ? STYLE_MODEL_FILE_OK : STYLE_MODEL_FILE_ERR_IO;
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
    char *text = style_read_file(path);
    if (!text) {
        return STYLE_MODEL_FILE_ERR_IO;
    }
    cJSON *root = cJSON_Parse(text);
    uint64_t hash = fnv1a(text);
    free(text);

    StyleModelFileResult result = STYLE_MODEL_FILE_ERR_FORMAT;
    StyleStatistic *statistics = NULL;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "statistics");
    const char *schema = string_member(root, "schema");
    const char *name = string_member(root, "style_name");
    const char *corpus = string_member(root, "training_corpus");
    const char *version = string_member(root, "model_version");
    unsigned long schema_version = 0, piece_count = 0;
    if (!schema || strcmp(schema, FILE_SCHEMA) != 0 || !count_member(root, "schema_version", &schema_version) ||
        schema_version != FILE_SCHEMA_VERSION || !name || !corpus || !version || !count_member(root, "pieces", &piece_count) ||
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
