#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"

/* The collection queries (R-646): the named sets of Library pieces. */

/* The members every collection answer carries. */
static bool add_summary_members(cJSON *object, const char *id, const char *name, const char *description, size_t piece_count,
                                const char *updated_at) {
    return cJSON_AddStringToObject(object, "id", id) && cJSON_AddStringToObject(object, "name", name) &&
           cJSON_AddStringToObject(object, "description", description) &&
           cJSON_AddNumberToObject(object, "piece_count", (double)piece_count) && cJSON_AddStringToObject(object, "updated_at", updated_at);
}

static UrsatzStatus collection_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "collection operation failed");
}

/* The collection params id names, or the failure ("invalid id" for an id that cannot name one). */
static UrsatzStatus read_collection(const WorkshopCall *call, WorkshopCollection **collection, UrsatzError *error) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/id", "invalid id");
    }
    UrsatzStatus result = workshop_collection_get(id, collection);
    return result == URSATZ_OK ? result : collection_failure(error, result);
}

WORKSHOP_HANDLER(workshop_q_collection_list) {
    (void)call;
    WorkshopCollectionEntry *entries = NULL;
    size_t count = 0;
    UrsatzStatus result = workshop_collection_list(&entries, &count);
    if (result != URSATZ_OK) {
        return collection_failure(error, result);
    }
    cJSON *object = workshop_reply_object(reply);
    cJSON *list = object ? cJSON_AddArrayToObject(object, "corpora") : NULL;
    bool built = list != NULL;
    for (size_t i = 0; built && i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        built = item && add_summary_members(item, entries[i].id, entries[i].name, entries[i].description, entries[i].piece_count,
                                            entries[i].updated_at);
        if (built) {
            cJSON_AddItemToArray(list, item);
        } else {
            cJSON_Delete(item);
        }
    }
    workshop_collection_free_entries(entries, count);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_q_collection_get) {
    WorkshopCollection *collection = NULL;
    UrsatzStatus result = read_collection(call, &collection, error);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *object = workshop_reply_object(reply);
    cJSON *pieces = object ? cJSON_AddArrayToObject(object, "pieces") : NULL;
    bool built = pieces && add_summary_members(object, collection->id, collection->name, collection->description,
                                               collection->piece_count, collection->updated_at) &&
                 cJSON_AddStringToObject(object, "created_at", collection->created_at);
    for (size_t i = 0; built && i < collection->piece_count; i++) {
        cJSON *piece_id = cJSON_CreateString(collection->pieces[i]);
        built = piece_id != NULL;
        if (built) {
            cJSON_AddItemToArray(pieces, piece_id);
        }
    }
    workshop_collection_destroy(collection);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_q_collection_pieces) {
    WorkshopCollection *collection = NULL;
    UrsatzStatus result = read_collection(call, &collection, error);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *object = workshop_reply_object(reply);
    cJSON *pieces = object ? cJSON_AddArrayToObject(object, "pieces") : NULL;
    bool built = pieces != NULL;
    for (size_t i = 0; built && i < collection->piece_count; i++) {
        cJSON *piece = cJSON_CreateObject();
        built = piece && cJSON_AddStringToObject(piece, "id", collection->pieces[i]);
        if (built) {
            cJSON_AddItemToArray(pieces, piece);
        } else {
            cJSON_Delete(piece);
        }
    }
    workshop_collection_destroy(collection);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}
