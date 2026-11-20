#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "base/events_hub.h"
#include "coordinator/dispatch.h"

/* The Library queries (R-646): the pieces a person uploaded, as the GUI lists, opens, exports and plays them. */

#define DOCUMENT_SECTIONS_MAX 64
#define DOCUMENT_SECTION_NAME_MAX 64

static const cJSON *member_of(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static const char *piece_dir(void) {
    return WORKSHOP_DIR_PIECES;
}

/* The string a query parameter holds: the first of an array of strings, NULL when absent or empty. */
static const char *first_text(const cJSON *params, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (cJSON_IsArray(item)) {
        item = cJSON_GetArrayItem(item, 0);
    }
    return cJSON_IsString(item) && item->valuestring[0] != '\0' ? item->valuestring : NULL;
}

/* Whether value is one of the texts the parameter holds (a parameter that may repeat is an array). */
static bool has_text(const cJSON *params, const char *name, const char *value) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (cJSON_IsString(item)) {
        return strcmp(item->valuestring, value) == 0;
    }
    if (!cJSON_IsArray(item)) {
        return false;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, item) {
        if (cJSON_IsString(entry) && strcmp(entry->valuestring, value) == 0) {
            return true;
        }
    }
    return false;
}

/* The collection the query is limited to (param collection, optional): its members' ids go into filter, which points into *collection until
 * it is destroyed. Fails for an id that names no collection. */
static UrsatzStatus limit_to_collection(const cJSON *params, WorkshopPieceFilter *filter, WorkshopCollection **collection, UrsatzError *error) {
    *collection = NULL;
    const char *id = first_text(params, "collection");
    if (!id) {
        return URSATZ_OK;
    }
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/collection", "invalid id");
    }
    UrsatzStatus result = workshop_collection_get(id, collection);
    if (result != URSATZ_OK) {
        return workshop_call_fail_service(error, result, "collection operation failed");
    }
    filter->members_only = true;
    filter->member_ids = (const char *const *)(*collection)->pieces;
    filter->member_count = (*collection)->piece_count;
    return URSATZ_OK;
}

/* The filter of the Library list: composer, source, tag, unassigned (composer or source, may repeat), untagged=1 and collection (its members only). */
WORKSHOP_HANDLER(workshop_q_library_list) {
    WorkshopPieceFilter filter;
    memset(&filter, 0, sizeof(filter));
    WorkshopCollection *collection = NULL;
    UrsatzStatus limited = limit_to_collection(call->params, &filter, &collection, error);
    if (limited != URSATZ_OK) {
        return limited;
    }
    filter.composer = first_text(call->params, "composer");
    filter.source = first_text(call->params, "source");
    filter.tag = first_text(call->params, "tag");
    filter.composer_unassigned = has_text(call->params, "unassigned", "composer");
    filter.source_unassigned = has_text(call->params, "unassigned", "source");
    const char *untagged = first_text(call->params, "untagged");
    filter.untagged = untagged && strcmp(untagged, "1") == 0;
    cJSON *pieces = NULL;
    UrsatzStatus result = workshop_library_list_json(piece_dir(), &filter, &pieces);
    workshop_collection_destroy(collection);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, "", result == URSATZ_ERR_ALLOC ? "out of memory" : "failed to list pieces");
    }
    return workshop_reply_member(reply, "pieces", pieces, error);
}

/* The distinct composers, sources and tags with piece counts, of the pieces of param collection when it is given. */
WORKSHOP_HANDLER(workshop_q_library_groups) {
    WorkshopPieceFilter scope;
    memset(&scope, 0, sizeof(scope));
    WorkshopCollection *collection = NULL;
    UrsatzStatus limited = limit_to_collection(call->params, &scope, &collection, error);
    if (limited != URSATZ_OK) {
        return limited;
    }
    cJSON *groups = NULL;
    UrsatzStatus result = workshop_library_groups_json(piece_dir(), &scope, &groups);
    workshop_collection_destroy(collection);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, "", result == URSATZ_ERR_ALLOC ? "out of memory" : "failed to list pieces");
    }
    return workshop_reply_tree(reply, groups, error);
}

/* One piece with its stored analysis: params id and document ("summary" for the summary only). */
WORKSHOP_HANDLER(workshop_q_library_piece) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const char *mode = first_text(call->params, "document");
    cJSON *piece = NULL;
    UrsatzStatus result = workshop_library_piece_json(piece_dir(), id, mode && strcmp(mode, "summary") == 0, &piece);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, "", result == URSATZ_ERR_NOT_FOUND ? "piece not found" : "failed to load piece");
    }
    /* A piece whose analysis is stale is a fact other pages want to know (R-648). */
    if (cJSON_IsTrue(member_of(piece, "stale"))) {
        cJSON *event = cJSON_CreateObject();
        if (event && cJSON_AddStringToObject(event, "piece_id", id)) {
            workshop_event("analysis-stale", event);
        } else {
            cJSON_Delete(event);
        }
    }
    return workshop_reply_tree(reply, piece, error);
}

/* Reads the comma separated document sections: lowercase letters, digits and underscores, none empty, at most 64. */
static bool parse_sections(const char *list, char names_text[][DOCUMENT_SECTION_NAME_MAX + 1], const char **names, size_t *count) {
    *count = 0;
    const char *cursor = list;
    while (*cursor) {
        size_t length = 0;
        while (cursor[length] && cursor[length] != ',') {
            char c = cursor[length];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
                return false;
            }
            length++;
        }
        if (length == 0 || length > DOCUMENT_SECTION_NAME_MAX || *count == DOCUMENT_SECTIONS_MAX) {
            return false;
        }
        memcpy(names_text[*count], cursor, length);
        names_text[*count][length] = '\0';
        names[*count] = names_text[*count];
        (*count)++;
        cursor += length;
        if (*cursor == ',') {
            cursor++;
            if (!*cursor) {
                return false;
            }
        }
    }
    return true;
}

/* Only the named sections of the stored analysis document, read without importing the source: params id and sections ("a,b"). */
WORKSHOP_HANDLER(workshop_q_library_document) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const char *list = first_text(call->params, "sections");
    char names_text[DOCUMENT_SECTIONS_MAX][DOCUMENT_SECTION_NAME_MAX + 1];
    const char *names[DOCUMENT_SECTIONS_MAX];
    size_t count = 0;
    if (!list || strlen(list) > DOCUMENT_SECTIONS_MAX * (DOCUMENT_SECTION_NAME_MAX + 1) ||
        !parse_sections(list, names_text, names, &count)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/sections", "sections must name one or more document sections");
    }
    char *sections = NULL;
    UrsatzStatus result = workshop_analysis_get_document(piece_dir(), id, names, count, &sections);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result == URSATZ_ERR_NOT_FOUND ? result : URSATZ_ERR_INTERNAL, "",
                                  result == URSATZ_ERR_NOT_FOUND ? "piece not found" : "failed to load the document");
    }
    cJSON *tree = cJSON_Parse(sections);
    free(sections);
    if (!tree) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_member(reply, "sections", tree, error);
}

/* A measure number of the export range: digits only, at most 6. */
static bool parse_measure(const char *text, int *out) {
    long long value = 0;
    if (!workshop_parse_whole(text, 6, &value)) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* The Analyzer's text export of the piece (text/plain): params id, from and to (measure numbers, both optional). */
WORKSHOP_HANDLER(workshop_q_library_export) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const char *from_text = first_text(call->params, "from");
    const char *to_text = first_text(call->params, "to");
    int first = 0;
    int last = 0;
    static const char *const kBadRange = "from and to must be measure numbers, from not after to";
    if ((from_text && !parse_measure(from_text, &first)) || (to_text && !parse_measure(to_text, &last))) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/from", "%s", kBadRange);
    }
    if (from_text || to_text) {
        if (!from_text) {
            first = 1;
        }
        if (!to_text) {
            last = 999999;
        }
        if (first > last) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/from", "%s", kBadRange);
        }
    }
    char *text = NULL;
    UrsatzStatus result = workshop_analysis_get_piece_export(piece_dir(), id, first, last, &text);
    if (result != URSATZ_OK) {
        switch (result) {
            case URSATZ_ERR_NOT_FOUND:
                return workshop_call_fail(error, result, "", "piece not found");
            case URSATZ_ERR_FORMAT:
                return workshop_call_fail(error, result, "", "could not import the stored source");
            case URSATZ_ERR_REQUEST:
                return workshop_call_fail(error, result, "", "no measure in that range");
            default:
                return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "failed to export the analysis");
        }
    }
    UrsatzStatus replied = workshop_reply_text(reply, text, 0, "text/plain; charset=utf-8", error);
    free(text);
    return replied;
}

/* The piece's score as MusicXML (application/xml). */
WORKSHOP_HANDLER(workshop_q_library_musicxml) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char *xml = NULL;
    bool meter_defaulted = false;
    UrsatzStatus result = workshop_analysis_get_piece_musicxml(piece_dir(), id, &xml, &meter_defaulted);
    if (result != URSATZ_OK) {
        switch (result) {
            case URSATZ_ERR_NOT_FOUND:
                return workshop_call_fail(error, result, "", "piece not found");
            case URSATZ_ERR_FORMAT:
                return workshop_call_fail(error, result, "", "could not import the stored source");
            case URSATZ_ERR_REQUEST:
            case URSATZ_ERR_INTERNAL:
                return workshop_call_fail(error, result, "", "failed to export piece");
            default:
                return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "failed to export piece");
        }
    }
    UrsatzStatus replied = workshop_reply_text(reply, xml, 0, "application/xml", error);
    free(xml);
    return replied;
}

/* Whether the score's barlines come from a default 4/4: {"meter_defaulted": bool}. */
WORKSHOP_HANDLER(workshop_q_library_meter_defaulted) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool defaulted = false;
    UrsatzStatus result = workshop_analysis_get_piece_meter_defaulted(piece_dir(), id, &defaulted);
    if (result != URSATZ_OK) {
        switch (result) {
            case URSATZ_ERR_NOT_FOUND:
                return workshop_call_fail(error, result, "", "piece not found");
            case URSATZ_ERR_FORMAT:
                return workshop_call_fail(error, result, "", "could not import the stored source");
            default:
                return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "failed to export piece");
        }
    }
    cJSON *object = workshop_reply_object(reply);
    if (!object || !cJSON_AddBoolToObject(object, "meter_defaulted", defaulted)) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* What a browser needs to play the piece and follow it: the Analyzer's playback document, as the tree it is. */
WORKSHOP_HANDLER(workshop_q_library_playback) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char *json = NULL;
    UrsatzStatus result = workshop_analysis_get_piece_playback(piece_dir(), id, &json);
    if (result != URSATZ_OK) {
        switch (result) {
            case URSATZ_ERR_NOT_FOUND:
                return workshop_call_fail(error, result, "", "piece not found");
            case URSATZ_ERR_FORMAT:
                return workshop_call_fail(error, result, "", "could not import the stored source");
            default:
                return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "failed to build playback");
        }
    }
    cJSON *tree = cJSON_Parse(json);
    free(json);
    return tree ? workshop_reply_tree(reply, tree, error) : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}
