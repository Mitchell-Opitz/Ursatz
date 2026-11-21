#include "styles.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/txn.h"
#include "base/last_error.h"
#include "items/lock.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <cJSON.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <theory/style/style_compare.h>
#include <theory/style/style_features.h>
#include <theory/style/style_labels.h>
#include <theory/style/style_model.h>
#include <theory/style/style_model_file.h>
#include <theory/style/style_profile.h>
#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include "analysis.h"
#include "items/collections.h"
#include "items/ids.h"
#include "items/library_metadata.h"
#include "analysis/style_model_io.h"
#include "analysis/styles_internal.h"

const char *styles_kPiecesDir = WORKSHOP_DIR_PIECES;

const char *styles_kStylesDir = WORKSHOP_DIR_STYLES;

WorkshopLock styles_g_lock = WORKSHOP_LOCK_INIT;

/* --- list ----------------------------------------------------------------- */

/* The names in styles/; empty when the directory is absent. false only when memory ran out. */
static bool list_style_dirs(CommonNames *names) {
    return common_dir_list(styles_kStylesDir, names, NULL) != URSATZ_ERR_ALLOC;
}

static const char *entry_text(const cJSON *entry, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(entry, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* Newest update first, then id. */
static int compare_entries(const void *left, const void *right) {
    const cJSON *a = *(const cJSON *const *)left;
    const cJSON *b = *(const cJSON *const *)right;
    int by_time = strcmp(entry_text(b, "updated_at"), entry_text(a, "updated_at"));
    return by_time != 0 ? by_time : strcmp(entry_text(a, "id"), entry_text(b, "id"));
}

UrsatzStatus workshop_style_list(cJSON **out_styles) {
    *out_styles = NULL;

    LOCK();
    CommonNames names = {0};
    if (!list_style_dirs(&names)) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    cJSON **entries = names.count > 0 ? calloc(names.count, sizeof(*entries)) : NULL;
    UrsatzStatus result = names.count > 0 && !entries ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : URSATZ_OK;
    size_t count = 0;
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *meta = NULL;
        UrsatzStatus loaded = styles_load_meta(names.items[i], &meta);
        if (loaded == URSATZ_OK) {
            entries[count] = styles_entry_of(meta);
            cJSON_Delete(meta);
            if (entries[count]) {
                count++;
            } else {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        } else if (loaded != URSATZ_ERR_NOT_FOUND) {
            result = loaded;
        }
    }
    common_names_free(&names);
    UNLOCK();

    cJSON *array = result == URSATZ_OK ? cJSON_CreateArray() : NULL;
    if (array && count > 1) {
        qsort(entries, count, sizeof(*entries), compare_entries);
    }
    for (size_t i = 0; i < count; i++) {
        if (array) {
            cJSON_AddItemToArray(array, entries[i]);
        } else {
            cJSON_Delete(entries[i]);
        }
    }
    free(entries);
    if (result == URSATZ_OK && !array) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_styles = array;
    return result;
}

UrsatzStatus workshop_style_get_entry(const char *id, cJSON **out_entry, cJSON **out_skipped) {
    *out_entry = NULL;
    *out_skipped = NULL;
    LOCK();
    cJSON *meta = NULL;
    UrsatzStatus result = styles_load_meta(id, &meta);
    UNLOCK();
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *skipped = cJSON_DetachItemFromObjectCaseSensitive(meta, "skipped");
    cJSON *entry = styles_entry_of(meta);
    cJSON_Delete(meta);
    if (!entry || !(skipped = skipped ? skipped : cJSON_CreateArray())) {
        cJSON_Delete(entry);
        cJSON_Delete(skipped);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_entry = entry;
    *out_skipped = skipped;
    return URSATZ_OK;
}

/* --- get ------------------------------------------------------------------ */

#define MAX_LABELS_PER_CATEGORY 12

/* Most frequent first, then label. */
static int compare_labels(const void *left, const void *right) {
    const StyleStatistic *a = *(const StyleStatistic *const *)left;
    const StyleStatistic *b = *(const StyleStatistic *const *)right;
    if (a->count != b->count) {
        return a->count > b->count ? -1 : 1;
    }
    return strcmp(a->label, b->label);
}

/* One category of the statistics: statistics[first, end) share its name. */
typedef struct {
    size_t first;
    size_t end;
    StyleDimension dimension;
} CategoryRange;

/* Harmony, rhythm, melody, form, then category. */
static const StyleStatistic *g_statistics;

static int compare_categories(const void *left, const void *right) {
    const CategoryRange *a = left;
    const CategoryRange *b = right;
    int by_dimension = (int)a->dimension - (int)b->dimension;
    return by_dimension != 0 ? by_dimension : strcmp(g_statistics[a->first].category, g_statistics[b->first].category);
}

/* {dimension: {category: {"labels": [{"label","count","total"}], "more": n}}} from the model file; the labels come by count
   descending (then label), at most 12, and "more" is how many were cut. */
static UrsatzStatus statistics_json(const char *model_path, cJSON **out) {
    *out = NULL;
    StyleModel *model = NULL;
    if (workshop_style_model_read(model_path, &model, NULL) != STYLE_MODEL_FILE_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleModelFileResult read = style_model_statistics(model, &statistics, &count);
    style_model_destroy(model);
    if (read != STYLE_MODEL_FILE_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }

    CategoryRange *ranges = count > 0 ? calloc(count, sizeof(*ranges)) : NULL;
    size_t range_count = 0;
    UrsatzStatus result = count > 0 && !ranges ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : URSATZ_OK;
    for (size_t start = 0, end; result == URSATZ_OK && start < count; start = end) {
        end = start;
        while (end < count && strcmp(statistics[end].category, statistics[start].category) == 0) {
            end++;
        }
        StyleDimension dimension;
        if (style_category_dimension(statistics[start].category, &dimension)) {
            ranges[range_count++] = (CategoryRange){start, end, dimension};
        }
    }
    g_statistics = statistics;
    if (range_count > 1) {
        qsort(ranges, range_count, sizeof(*ranges), compare_categories);
    }
    g_statistics = NULL;

    cJSON *object = result == URSATZ_OK ? cJSON_CreateObject() : NULL;
    for (size_t i = 0; object && i < range_count; i++) {
        const CategoryRange *range = &ranges[i];
        const char *dimension_name = style_dimension_name(range->dimension);
        cJSON *dimension = cJSON_GetObjectItemCaseSensitive(object, dimension_name);
        if (!dimension) {
            dimension = cJSON_AddObjectToObject(object, dimension_name);
        }
        cJSON *entry = dimension ? cJSON_AddObjectToObject(dimension, statistics[range->first].category) : NULL;
        cJSON *labels = entry ? cJSON_AddArrayToObject(entry, "labels") : NULL;
        size_t total_labels = range->end - range->first;
        const StyleStatistic **sorted = labels ? calloc(total_labels, sizeof(*sorted)) : NULL;
        bool ok = sorted != NULL;
        for (size_t j = 0; ok && j < total_labels; j++) {
            sorted[j] = &statistics[range->first + j];
        }
        if (ok) {
            qsort(sorted, total_labels, sizeof(*sorted), compare_labels);
        }
        size_t kept = total_labels < MAX_LABELS_PER_CATEGORY ? total_labels : MAX_LABELS_PER_CATEGORY;
        for (size_t j = 0; ok && j < kept; j++) {
            cJSON *label = cJSON_CreateObject();
            ok = label && cJSON_AddStringToObject(label, "label", sorted[j]->label) &&
                 cJSON_AddNumberToObject(label, "count", (double)sorted[j]->count) &&
                 cJSON_AddNumberToObject(label, "total", (double)sorted[j]->total);
            if (ok) {
                cJSON_AddItemToArray(labels, label);
            } else {
                cJSON_Delete(label);
            }
        }
        free(sorted);
        if (!ok || !cJSON_AddNumberToObject(entry, "more", (double)(total_labels - kept))) {
            cJSON_Delete(object);
            object = NULL;
        }
    }
    free(ranges);
    style_statistics_free(statistics, count);
    if (result == URSATZ_OK && !object) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = object;
    return result;
}

UrsatzStatus workshop_style_get(const char *id, cJSON **out_style) {
    *out_style = NULL;
    LOCK();
    cJSON *meta = NULL;
    UrsatzStatus result = styles_load_meta(id, &meta);
    cJSON *statistics = NULL;
    if (result == URSATZ_OK) {
        char dir[PATH_BYTES];
        char model_path[PATH_BYTES + 32];
        styles_style_dir_path(id, dir, sizeof(dir));
        snprintf(model_path, sizeof(model_path), "%s/style-model.json", dir);
        result = statistics_json(model_path, &statistics);
    }
    UNLOCK();
    if (result == URSATZ_OK && !cJSON_AddItemToObject(meta, "statistics", statistics)) {
        cJSON_Delete(statistics);
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(meta);
        return result;
    }
    *out_style = meta;
    return URSATZ_OK;
}

/* --- the build request ---------------------------------------------------- */

void workshop_style_request_free(WorkshopStyleRequest *request) {
    free(request->ids);
    workshop_style_brief_free(&request->brief);
    memset(request, 0, sizeof(*request));
}

UrsatzStatus workshop_style_build_request_parse(const cJSON *body, WorkshopStyleRequest *request, UrsatzError *error) {
    memset(request, 0, sizeof(*request));
    const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    const cJSON *filter_item = cJSON_GetObjectItemCaseSensitive(body, "filter");
    const cJSON *collection_item = cJSON_GetObjectItemCaseSensitive(body, "collection_id");
    const cJSON *ids_item = cJSON_GetObjectItemCaseSensitive(body, "piece_ids");
    const cJSON *brief_item = cJSON_GetObjectItemCaseSensitive(body, "brief");
    if (name_item && !cJSON_IsString(name_item)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "invalid name");
    }
    int sources = (filter_item != NULL) + (collection_item != NULL) + (ids_item != NULL) + (brief_item != NULL);
    if (sources != 1) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "give exactly one of filter, collection_id, piece_ids and brief");
    }
    request->name = name_item ? name_item->valuestring : NULL;
    WorkshopStyleSource *source = &request->source;
    const char *failure = NULL;

    if (brief_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_BRIEF;
        failure = workshop_style_brief_parse(brief_item, &request->brief);
        source->brief = &request->brief;
    } else if (filter_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_FILTER;
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(filter_item, "composer");
        const cJSON *filter_source = cJSON_GetObjectItemCaseSensitive(filter_item, "source");
        const cJSON *tag = cJSON_GetObjectItemCaseSensitive(filter_item, "tag");
        if (!cJSON_IsObject(filter_item) || (composer && !cJSON_IsString(composer) && !cJSON_IsNull(composer)) ||
            (filter_source && !cJSON_IsString(filter_source) && !cJSON_IsNull(filter_source)) ||
            (tag && !cJSON_IsString(tag) && !cJSON_IsNull(tag))) {
            failure = "invalid filter";
        } else {
            source->composer = cJSON_IsString(composer) ? composer->valuestring : NULL;
            source->source = cJSON_IsString(filter_source) ? filter_source->valuestring : NULL;
            source->tag = cJSON_IsString(tag) ? tag->valuestring : NULL;
        }
    } else if (collection_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_COLLECTION;
        if (!cJSON_IsString(collection_item) || !workshop_collection_is_valid_id(collection_item->valuestring)) {
            failure = "invalid collection_id";
        } else {
            source->collection_id = collection_item->valuestring;
        }
    } else {
        source->kind = WORKSHOP_STYLE_SOURCE_PIECES;
        int length = cJSON_IsArray(ids_item) ? cJSON_GetArraySize(ids_item) : 0;
        if (length <= 0) {
            failure = "invalid piece_ids";
        } else if ((size_t)length > WORKSHOP_STYLE_BRIEF_MAX_PIECES) {
            failure = "too many pieces";
        } else {
            request->ids = calloc((size_t)length, sizeof(*request->ids));
            if (!request->ids) {
                return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
            }
            const cJSON *item = NULL;
            cJSON_ArrayForEach(item, ids_item) {
                if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
                    failure = "invalid piece_ids";
                    break;
                }
                request->ids[source->piece_count++] = item->valuestring;
            }
            source->piece_ids = request->ids;
        }
    }
    if (failure) {
        UrsatzStatus status = strcmp(failure, "out of memory") == 0 ? URSATZ_ERR_ALLOC : URSATZ_ERR_REQUEST;
        workshop_style_request_free(request);
        return ursatz_error_set(error, status, NULL, "%s", failure);
    }
    return URSATZ_OK;
}
