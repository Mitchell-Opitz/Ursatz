#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <text/text_layers.h>

#include "base/knowledge.h"
#include "coordinator/dispatch.h"

/* The knowledge queries (R-765, R-658): the palette of a key, and finding, showing and checking entries. Nothing is saved. */

#define FIND_MAX 50

/* The palette the Composer answers for params key and with, as its own document (composer_api_palette). */
WORKSHOP_HANDLER(workshop_q_knowledge_palette) {
    bool bad = false;
    const char *key = workshop_param_string(call->params, "key", &bad);
    const char *with = workshop_param_string(call->params, "with", &bad);
    if (bad || key == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "knowledge.palette takes key (required) and with, both strings");
    }
    cJSON *request = cJSON_CreateObject();
    if (request == NULL || cJSON_AddStringToObject(request, "key", key) == NULL || (with != NULL && cJSON_AddStringToObject(request, "with", with) == NULL)) {
        cJSON_Delete(request);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char *text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (text == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    ComposerResponse response;
    UrsatzStatus status = composer_api_palette(workshop_composer_context_global(), text, &response);
    cJSON_free(text);
    if (status != URSATZ_OK) {
        UrsatzStatus result = workshop_call_fail(error, status, response.error.path, "%s", response.error.message[0] ? response.error.message : "the palette failed");
        composer_response_destroy(&response);
        return result;
    }
    cJSON *document = response.json != NULL ? cJSON_Parse(response.json) : NULL;
    composer_response_destroy(&response);
    return document != NULL ? workshop_reply_tree(reply, document, error) : workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "composer failed");
}

static bool contains_ignoring_case(const char *text, const char *word) {
    size_t length = strlen(word);
    for (const char *at = text; text != NULL && *at; at++) {
        size_t i = 0;
        while (i < length && at[i] != '\0' && tolower((unsigned char)at[i]) == tolower((unsigned char)word[i])) {
            i++;
        }
        if (i == length) {
            return true;
        }
    }
    return false;
}

/* Whether the id, the name or an alias of the entry holds the word. */
static bool entry_holds(const KnowledgeSnapshot *snapshot, const KnowledgeEntry *entry, const char *word) {
    if (contains_ignoring_case(entry->id, word) || contains_ignoring_case(entry->name, word)) {
        return true;
    }
    size_t aliases = knowledge_snapshot_entry_alias_count(snapshot, entry->id);
    for (size_t i = 0; i < aliases; i++) {
        if (contains_ignoring_case(knowledge_snapshot_entry_alias(snapshot, entry->id, i), word)) {
            return true;
        }
    }
    return false;
}

static const char *status_word(KnowledgeEntryStatus status) {
    return status == KNOWLEDGE_STATUS_ACTIVE ? "active" : status == KNOWLEDGE_STATUS_RETIRED ? "retired" : "reference";
}

/* The entries whose id, name or alias holds every space-separated word of params words, of params kind when given, in id order, at most 50. */
WORKSHOP_HANDLER(workshop_q_knowledge_find) {
    bool bad = false;
    const char *words = workshop_param_string(call->params, "words", &bad);
    const char *kind = workshop_param_string(call->params, "kind", &bad);
    const KnowledgeSnapshot *snapshot = workshop_knowledge_snapshot();
    if (bad || words == NULL || snapshot == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "knowledge.find takes words (required) and kind, both strings");
    }
    char *copy = strdup(words);
    cJSON *entries = cJSON_CreateArray();
    if (copy == NULL || entries == NULL) {
        free(copy);
        cJSON_Delete(entries);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char *list[32];
    size_t word_count = 0;
    for (char *word = strtok(copy, " \t"); word != NULL && word_count < 32; word = strtok(NULL, " \t")) {
        list[word_count++] = word;
    }
    bool kind_known = kind == NULL;
    bool more = false;
    bool ok = true;
    for (size_t i = 0; ok && i < knowledge_snapshot_entry_count(snapshot); i++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry_at(snapshot, i);
        kind_known = kind_known || strcmp(entry->kind, kind) == 0;
        bool match = kind == NULL || strcmp(entry->kind, kind) == 0;
        for (size_t w = 0; match && w < word_count; w++) {
            match = entry_holds(snapshot, entry, list[w]);
        }
        if (!match) {
            continue;
        }
        if (cJSON_GetArraySize(entries) == FIND_MAX) {
            more = true;
            break;
        }
        cJSON *item = cJSON_CreateObject();
        ok = item != NULL && cJSON_AddStringToObject(item, "id", entry->id) && cJSON_AddStringToObject(item, "kind", entry->kind) &&
             (entry->subtype == NULL || cJSON_AddStringToObject(item, "subtype", entry->subtype)) && cJSON_AddStringToObject(item, "name", entry->name) &&
             cJSON_AddStringToObject(item, "status", status_word(entry->status)) && cJSON_AddBoolToObject(item, "user", strncmp(entry->id, "user.", 5) == 0) &&
             cJSON_AddItemToArray(entries, item);
        if (!ok) {
            cJSON_Delete(item);
        }
    }
    free(copy);
    if (!kind_known) {
        cJSON_Delete(entries);
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/kind", "no entry has the kind '%s'", kind);
    }
    cJSON *object = ok ? workshop_reply_object(reply) : NULL;
    if (object == NULL || !cJSON_AddItemToObject(object, "entries", entries) || !cJSON_AddBoolToObject(object, "more", more)) {
        if (object == NULL || cJSON_GetObjectItemCaseSensitive(object, "entries") != entries) {
            cJSON_Delete(entries);
        }
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* One entry: params id and view ("json", the entry as loaded, or "text", its Ursatz Text). */
WORKSHOP_HANDLER(workshop_q_knowledge_show) {
    bool bad = false;
    const char *id = workshop_param_string(call->params, "id", &bad);
    const char *view = workshop_param_string(call->params, "view", &bad);
    const KnowledgeSnapshot *snapshot = workshop_knowledge_snapshot();
    if (bad || id == NULL || snapshot == NULL || (view != NULL && strcmp(view, "json") != 0 && strcmp(view, "text") != 0)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "knowledge.show takes id (required) and view (json or text)");
    }
    const cJSON *entry = knowledge_snapshot_entry_json(snapshot, id);
    if (entry == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "no knowledge entry '%s'", id);
    }
    cJSON *object = workshop_reply_object(reply);
    if (object == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    bool text_view = view != NULL && strcmp(view, "text") == 0;
    if (!text_view) {
        cJSON *copy = cJSON_Duplicate(entry, true);
        return copy != NULL && cJSON_AddStringToObject(object, "view", "json") && cJSON_AddItemToObject(object, "entry", copy)
                   ? URSATZ_OK
                   : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char *json = cJSON_PrintUnformatted(entry);
    TextError failure;
    char *text = json != NULL ? text_from_json(json, TEXT_KIND_KNOWLEDGE_ENTRY, &failure) : NULL;
    cJSON_free(json);
    bool built = text != NULL && cJSON_AddStringToObject(object, "view", "text") && cJSON_AddStringToObject(object, "text", text);
    free(text);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "the entry has no text view");
}

/* Whether params file, a knowledge document, loads beside the embedded entries and the data folder's knowledge files: {"ok": true, "entries": n} or a refusal at /file. */
WORKSHOP_HANDLER(workshop_q_knowledge_check) {
    const cJSON *file = cJSON_GetObjectItemCaseSensitive(call->params, "file");
    if (!cJSON_IsObject(file)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/file", "knowledge.check takes file, a knowledge document (an object)");
    }
    char *text = cJSON_PrintUnformatted(file);
    if (text == NULL) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char message[512];
    bool ok = workshop_knowledge_check(workshop_data_dir(call->workshop), NULL, text, message, sizeof(message));
    cJSON_free(text);
    if (!ok) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/file", "%s", message);
    }
    const cJSON *entries = cJSON_GetObjectItemCaseSensitive(file, "entries");
    cJSON *object = workshop_reply_object(reply);
    return object && cJSON_AddTrueToObject(object, "ok") && cJSON_AddNumberToObject(object, "entries", cJSON_IsArray(entries) ? cJSON_GetArraySize(entries) : 0)
               ? URSATZ_OK
               : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}
