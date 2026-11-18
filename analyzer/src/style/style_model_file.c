#include <ursatz_analyzer/style_profile.h>
#include <ursatz_analyzer/version.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "model/identity/entity_id.h"
#include "theory/style/style_model_build.h"



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
        cJSON_AddItemToObject(root, "schema", cJSON_CreateString(STYLE_MODEL_FILE_SCHEMA));
        cJSON_AddItemToObject(root, "schema_version", cJSON_CreateNumber(STYLE_MODEL_FILE_SCHEMA_VERSION));
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

