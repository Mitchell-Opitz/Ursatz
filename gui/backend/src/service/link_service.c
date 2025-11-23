#include "link_service.h"

#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "analysis_service.h"
#include "collection_service.h"
#include "material_service.h"
#include "piece_metadata.h"
#include "subject_service.h"

static const char *kPiecesDir = "pieces";

/* --- id lists ------------------------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} IdList;

static char *dup_string(const char *value) {
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

void ursatz_gui_link_ids_free(char **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

static bool id_list_contains(const IdList *list, const char *id) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], id) == 0) {
            return true;
        }
    }
    return false;
}

/* Adds a copy of id unless the list has it. false only when memory ran out. */
static bool id_list_add(IdList *list, const char *id) {
    if (id_list_contains(list, id)) {
        return true;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        char **grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count] = dup_string(id);
    if (!list->items[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

static int compare_ids(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static void id_list_free(IdList *list) {
    ursatz_gui_link_ids_free(list->items, list->count);
    memset(list, 0, sizeof(*list));
}

/* Adds every string of a JSON array. */
static bool id_list_add_array(IdList *list, const cJSON *array) {
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (cJSON_IsString(item) && !id_list_add(list, item->valuestring)) {
            return false;
        }
    }
    return true;
}

/* --- scopes --------------------------------------------------------------- */

static bool text_given(const char *text) {
    return text && text[0] != '\0';
}

static void free_summaries(UrsatzGuiPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

/* Every Library piece, or those that match the filter (NULL for all). */
static UrsatzGuiLinkResult add_library_pieces(IdList *ids, const UrsatzGuiMetaFilter *filter) {
    UrsatzGuiPieceSummary *pieces = NULL;
    size_t count = 0;
    if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_GUI_ANALYSIS_OK) {
        return URSATZ_GUI_LINK_ERR_FAILED;
    }
    UrsatzGuiLinkResult result = URSATZ_GUI_LINK_OK;
    for (size_t i = 0; result == URSATZ_GUI_LINK_OK && i < count; i++) {
        if ((!filter || ursatz_gui_meta_matches(&pieces[i], filter)) && !id_list_add(ids, pieces[i].id)) {
            result = URSATZ_GUI_LINK_ERR_ALLOC;
        }
    }
    free_summaries(pieces, count);
    return result;
}

static UrsatzGuiLinkResult add_collection_pieces(IdList *ids, const char *collection_id) {
    if (!text_given(collection_id) || !ursatz_gui_collection_is_valid_id(collection_id)) {
        return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
    }
    UrsatzGuiCollection *collection = NULL;
    UrsatzGuiCollectionResult result = ursatz_gui_collection_get(collection_id, &collection);
    if (result == URSATZ_GUI_COLLECTION_ERR_NOT_FOUND) {
        return URSATZ_GUI_LINK_ERR_NOT_FOUND;
    }
    if (result != URSATZ_GUI_COLLECTION_OK) {
        return result == URSATZ_GUI_COLLECTION_ERR_ALLOC ? URSATZ_GUI_LINK_ERR_ALLOC : URSATZ_GUI_LINK_ERR_FAILED;
    }
    bool ok = true;
    for (size_t i = 0; ok && i < collection->piece_count; i++) {
        ok = id_list_add(ids, collection->pieces[i]);
    }
    ursatz_gui_collection_destroy(collection);
    return ok ? URSATZ_GUI_LINK_OK : URSATZ_GUI_LINK_ERR_ALLOC;
}

static UrsatzGuiLinkResult subject_result(UrsatzGuiSubjectResult result) {
    switch (result) {
        case URSATZ_GUI_SUBJECT_OK:
            return URSATZ_GUI_LINK_OK;
        case URSATZ_GUI_SUBJECT_ERR_INVALID_ID:
            return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
        case URSATZ_GUI_SUBJECT_ERR_NOT_FOUND:
            return URSATZ_GUI_LINK_ERR_NOT_FOUND;
        case URSATZ_GUI_SUBJECT_ERR_ALLOC:
            return URSATZ_GUI_LINK_ERR_ALLOC;
        default:
            return URSATZ_GUI_LINK_ERR_FAILED;
    }
}

static UrsatzGuiLinkResult add_subject_pieces(IdList *ids, const char *subject_id) {
    if (!text_given(subject_id)) {
        return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
    }
    cJSON *subject = NULL;
    UrsatzGuiLinkResult result = subject_result(ursatz_gui_subject_get(subject_id, &subject));
    if (result != URSATZ_GUI_LINK_OK) {
        return result;
    }
    bool ok = id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"));
    cJSON_Delete(subject);
    return ok ? URSATZ_GUI_LINK_OK : URSATZ_GUI_LINK_ERR_ALLOC;
}

/* True when any of the subject's items is one of the project's. */
static bool subject_links_item_of(const cJSON *subject, const IdList *item_ids) {
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(subject, "items")) {
        if (cJSON_IsString(item) && id_list_contains(item_ids, item->valuestring)) {
            return true;
        }
    }
    return false;
}

/* The Library pieces linked to every subject that links at least one item of the project. */
static UrsatzGuiLinkResult add_project_pieces(IdList *ids, const char *project_id) {
    if (!text_given(project_id)) {
        return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
    }
    cJSON *project = NULL;
    UrsatzGuiMaterialResult found = ursatz_gui_material_project_get(project_id, &project);
    if (found == URSATZ_GUI_MATERIAL_ERR_INVALID_ID) {
        return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
    }
    if (found == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND) {
        return URSATZ_GUI_LINK_ERR_NOT_FOUND;
    }
    if (found != URSATZ_GUI_MATERIAL_OK) {
        return found == URSATZ_GUI_MATERIAL_ERR_ALLOC ? URSATZ_GUI_LINK_ERR_ALLOC : URSATZ_GUI_LINK_ERR_FAILED;
    }
    cJSON_Delete(project);

    cJSON *items = NULL;
    found = ursatz_gui_material_item_list(project_id, NULL, &items);
    if (found != URSATZ_GUI_MATERIAL_OK) {
        return found == URSATZ_GUI_MATERIAL_ERR_ALLOC ? URSATZ_GUI_LINK_ERR_ALLOC : URSATZ_GUI_LINK_ERR_FAILED;
    }
    IdList item_ids = {0};
    const cJSON *item = NULL;
    bool ok = true;
    cJSON_ArrayForEach(item, items) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) && !id_list_add(&item_ids, id->valuestring)) {
            ok = false;
            break;
        }
    }
    cJSON_Delete(items);
    if (!ok) {
        id_list_free(&item_ids);
        return URSATZ_GUI_LINK_ERR_ALLOC;
    }

    UrsatzGuiLinkResult result = URSATZ_GUI_LINK_OK;
    cJSON *subjects = NULL;
    if (item_ids.count > 0) {
        result = subject_result(ursatz_gui_subject_list(&subjects));
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, subjects) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
        if (result != URSATZ_GUI_LINK_OK || !cJSON_IsString(id)) {
            break;
        }
        /* The list entries carry counts, not the links: read the subject itself. */
        cJSON *subject = NULL;
        UrsatzGuiSubjectResult got = ursatz_gui_subject_get(id->valuestring, &subject);
        if (got == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND) {
            continue;
        }
        result = subject_result(got);
        if (result == URSATZ_GUI_LINK_OK && subject_links_item_of(subject, &item_ids) &&
            !id_list_add_array(ids, cJSON_GetObjectItemCaseSensitive(subject, "pieces"))) {
            result = URSATZ_GUI_LINK_ERR_ALLOC;
        }
        cJSON_Delete(subject);
    }
    cJSON_Delete(subjects);
    id_list_free(&item_ids);
    return result;
}

UrsatzGuiLinkResult ursatz_gui_link_scope_resolve(const UrsatzGuiLinkScope *scope, char ***out_ids,
                                                  size_t *out_count) {
    if (!scope || !out_ids || !out_count) {
        return URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
    }
    *out_ids = NULL;
    *out_count = 0;

    IdList ids = {0};
    UrsatzGuiLinkResult result = URSATZ_GUI_LINK_OK;
    switch (scope->kind) {
        case URSATZ_GUI_LINK_SCOPE_ALL:
            result = add_library_pieces(&ids, NULL);
            break;
        case URSATZ_GUI_LINK_SCOPE_COLLECTION:
            result = add_collection_pieces(&ids, scope->collection_id);
            break;
        case URSATZ_GUI_LINK_SCOPE_FILTER: {
            UrsatzGuiMetaFilter filter = {0};
            filter.composer = text_given(scope->composer) ? scope->composer : NULL;
            filter.source = text_given(scope->source) ? scope->source : NULL;
            filter.tag = text_given(scope->tag) ? scope->tag : NULL;
            result = filter.composer || filter.source || filter.tag ? add_library_pieces(&ids, &filter)
                                                                     : URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
            break;
        }
        case URSATZ_GUI_LINK_SCOPE_SUBJECT:
            result = add_subject_pieces(&ids, scope->subject_id);
            break;
        case URSATZ_GUI_LINK_SCOPE_PROJECT:
            result = add_project_pieces(&ids, scope->project_id);
            break;
        default:
            result = URSATZ_GUI_LINK_ERR_INVALID_SCOPE;
            break;
    }
    if (result == URSATZ_GUI_LINK_OK && ids.count == 0) {
        result = URSATZ_GUI_LINK_ERR_EMPTY;
    }
    if (result != URSATZ_GUI_LINK_OK) {
        id_list_free(&ids);
        return result;
    }
    qsort(ids.items, ids.count, sizeof(*ids.items), compare_ids);
    *out_ids = ids.items;
    *out_count = ids.count;
    return URSATZ_GUI_LINK_OK;
}
