#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include "base/knowledge.h"
#include "composition/composer_client.h"
#include "coordinator/dispatch.h"
#include "items/txn.h"

/*
 * The knowledge commands (R-768, R-664): add, update and delete her entries in <data>/knowledge/user.json, and keep a harmony plan as her schema. A
 * candidate file is built into a snapshot with the built-in entries and the folder's other files before it is staged, so a refusal is the loader's message
 * at /entry; after the commit the workshop's snapshot is rebuilt and the Composer's vocabulary forgotten, so the new entry is offered at once.
 */

#define USER_FILE "knowledge/user.json"
#define USER_NAME "user.json"
#define NOTE_MAX 500

/* The file's document: {"schema", "schema_version", "source", "entries": []}, read from the folder or, when the file is not there, empty. */
static UrsatzStatus read_user_file(cJSON **out, UrsatzError *error) {
    char *text = NULL;
    cJSON *document = NULL;
    if (common_path_exists(USER_FILE)) {
        if (common_file_read(USER_FILE, &text, NULL, NULL) != URSATZ_OK || !(document = cJSON_Parse(text)) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(document, "entries"))) {
            free(text);
            cJSON_Delete(document);
            return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "knowledge/user.json cannot be read");
        }
        free(text);
    } else {
        document = cJSON_CreateObject();
        if (document && (!cJSON_AddStringToObject(document, "schema", "ursatz-knowledge") || !cJSON_AddNumberToObject(document, "schema_version", 1) ||
                         !cJSON_AddStringToObject(document, "source", "user") || !cJSON_AddArrayToObject(document, "entries"))) {
            cJSON_Delete(document);
            document = NULL;
        }
    }
    if (!document) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = document;
    return URSATZ_OK;
}

static cJSON *entry_named(cJSON *document, const char *id, int *index) {
    int at = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(document, "entries")) {
        if (strcmp(common_json_string(entry, "id"), id) == 0) {
            *index = at;
            return entry;
        }
        at++;
    }
    return NULL;
}

/* The name of the other file of the folder that defines id, or NULL. */
static bool other_file_defines(const char *data_dir, const char *id, char *out, size_t size) {
    char folder[800];
    CommonNames names;
    if (!common_path_join(folder, sizeof(folder), data_dir, "knowledge") || !common_path_is_dir(folder) || common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        return false;
    }
    bool found = false;
    for (size_t i = 0; !found && i < names.count; i++) {
        size_t length = strlen(names.items[i]);
        char path[900];
        char *text = NULL;
        if (length <= 5 || strcmp(names.items[i] + length - 5, ".json") != 0 || strcmp(names.items[i], USER_NAME) == 0 || !common_path_join(path, sizeof(path), folder, names.items[i]) ||
            common_file_read(path, &text, NULL, NULL) != URSATZ_OK) {
            continue;
        }
        cJSON *document = cJSON_Parse(text);
        free(text);
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(document, "entries")) {
            if (strcmp(common_json_string(entry, "id"), id) == 0) {
                snprintf(out, size, "%s", names.items[i]);
                found = true;
            }
        }
        cJSON_Delete(document);
    }
    common_names_free(&names);
    return found;
}

/* Checks the candidate document as the loader would read it beside the other files, stages it and commits; the snapshot and the vocabulary follow. */
static UrsatzStatus commit_document(cJSON *document, const char *data_dir, UrsatzError *error) {
    char *text = cJSON_Print(document);
    char message[512];
    if (!text) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (!workshop_knowledge_check(data_dir, USER_NAME, text, message, sizeof(message))) {
        cJSON_free(text);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/entry", "%s", message);
    }
    UrsatzStatus status = common_dir_make("knowledge", NULL) == URSATZ_OK ? URSATZ_OK : URSATZ_ERR_IO;
    WorkshopTxn *txn = status == URSATZ_OK ? workshop_txn_begin(NULL, "knowledge", "", 0) : NULL;
    if (!txn) {
        cJSON_free(text);
        return workshop_call_fail(error, URSATZ_ERR_IO, "", "the journal cannot be read");
    }
    UrsatzError failure = {{0}, {0}};
    status = workshop_txn_stage_file(txn, USER_FILE, text, strlen(text));
    cJSON_free(text);
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        return workshop_call_fail(error, status, "", "the knowledge file cannot be staged");
    }
    status = workshop_txn_commit(txn, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message[0] ? failure.message : "the knowledge file cannot be saved");
    }
    if (workshop_knowledge_rebuild() != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "the knowledge could not be rebuilt after the change");
    }
    workshop_composer_vocabulary_reset();
    return URSATZ_OK;
}

static bool read_note(const WorkshopCall *call, UrsatzError *error) {
    bool bad = false;
    const char *note = workshop_param_string(call->params, "note", &bad);
    if (bad || (note && strlen(note) > NOTE_MAX)) {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/note", "note is a text of up to %d characters", NOTE_MAX);
        return false;
    }
    return true;
}

static bool id_is_hers(const char *id, UrsatzError *error, const char *path) {
    if (strncmp(id, "user.", 5) != 0) {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "a kept entry's id starts with user.");
        return false;
    }
    return true;
}

static UrsatzStatus entry_reply(WorkshopReply *reply, const cJSON *entry, UrsatzError *error) {
    cJSON *copy = cJSON_Duplicate(entry, true);
    return copy ? workshop_reply_member(reply, "entry", copy, error) : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* Adds entry (params), or with `existing` replaces the one with the id; the document is committed. */
static UrsatzStatus store_entry(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error, const char *id, const cJSON *entry_param, bool replace) {
    const char *data_dir = workshop_data_dir(call->workshop);
    cJSON *document = NULL;
    UrsatzStatus status = read_user_file(&document, error);
    if (status != URSATZ_OK) {
        return status;
    }
    int index = 0;
    cJSON *existing = entry_named(document, id, &index);
    char file[128];
    if (!replace && (existing || workshop_knowledge_snapshot() == NULL || knowledge_snapshot_entry_json(workshop_knowledge_snapshot(), id) != NULL)) {
        cJSON_Delete(document);
        return workshop_call_fail(error, URSATZ_ERR_CONFLICT, "/entry/id", "the knowledge already has an entry %s", id);
    }
    if (replace && !existing) {
        status = other_file_defines(data_dir, id, file, sizeof(file)) ? workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/id", "%s is in %s; edit that file", id, file)
                                                                          : workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "no entry %s in knowledge/user.json", id);
        cJSON_Delete(document);
        return status;
    }
    cJSON *entry = cJSON_Duplicate(entry_param, true);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(document, "entries");
    cJSON_DeleteItemFromObjectCaseSensitive(entry, "id");
    cJSON *with_id = cJSON_CreateObject();
    bool ok = entry && with_id && cJSON_AddStringToObject(with_id, "id", id);
    for (cJSON *member = ok ? entry->child : NULL; ok && member; member = member->next) {
        ok = cJSON_AddItemToObject(with_id, member->string, cJSON_Duplicate(member, true));
    }
    cJSON_Delete(entry);
    if (!ok) {
        cJSON_Delete(with_id);
        cJSON_Delete(document);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (replace) {
        cJSON_ReplaceItemInArray(entries, index, with_id);
    } else {
        cJSON_AddItemToArray(entries, with_id);
    }
    status = commit_document(document, data_dir, error);
    if (status == URSATZ_OK) {
        status = entry_reply(reply, with_id, error);
    }
    cJSON_Delete(document);
    return status;
}

/* knowledge.add: entry (its id starts user.), note. */
WORKSHOP_HANDLER(workshop_c_knowledge_add) {
    const cJSON *entry = cJSON_GetObjectItemCaseSensitive(call->params, "entry");
    const char *id = cJSON_IsObject(entry) ? common_json_string(entry, "id") : "";
    if (!read_note(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!cJSON_IsObject(entry) || id[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/entry", "entry is an object with an id");
    }
    if (!id_is_hers(id, error, "/params/entry/id")) {
        return URSATZ_ERR_REQUEST;
    }
    return store_entry(call, reply, error, id, entry, false);
}

/* knowledge.update: id, entry, note. */
WORKSHOP_HANDLER(workshop_c_knowledge_update) {
    const char *id = NULL;
    const cJSON *entry = cJSON_GetObjectItemCaseSensitive(call->params, "entry");
    if (!workshop_param_require_string(call->params, "id", &id, error) || !read_note(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!cJSON_IsObject(entry)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/entry", "entry is an object");
    }
    if (!id_is_hers(id, error, "/params/id")) {
        return URSATZ_ERR_REQUEST;
    }
    return store_entry(call, reply, error, id, entry, true);
}

/* knowledge.delete: id, note. */
WORKSHOP_HANDLER(workshop_c_knowledge_delete) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error) || !read_note(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!id_is_hers(id, error, "/params/id")) {
        return URSATZ_ERR_REQUEST;
    }
    const char *data_dir = workshop_data_dir(call->workshop);
    cJSON *document = NULL;
    UrsatzStatus status = read_user_file(&document, error);
    if (status != URSATZ_OK) {
        return status;
    }
    int index = 0;
    char file[128];
    if (!entry_named(document, id, &index)) {
        status = other_file_defines(data_dir, id, file, sizeof(file)) ? workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/id", "%s is in %s; edit that file", id, file)
                                                                          : workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "no entry %s in knowledge/user.json", id);
        cJSON_Delete(document);
        return status;
    }
    cJSON_DeleteItemFromArray(cJSON_GetObjectItemCaseSensitive(document, "entries"), index);
    status = commit_document(document, data_dir, error);
    cJSON_Delete(document);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *answer = workshop_reply_object(reply);
    return answer && cJSON_AddStringToObject(answer, "id", id) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

static bool slug_ok(const char *slug) {
    size_t length = strlen(slug);
    if (length == 0 || length > 60 || slug[0] == '-' || slug[length - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        bool word = (slug[i] >= 'a' && slug[i] <= 'z') || (slug[i] >= '0' && slug[i] <= '9');
        if (!word && (slug[i] != '-' || slug[i + 1] == '-')) {
            return false;
        }
    }
    return true;
}

/*
 * knowledge.keep-schema: item (an hrm- id), version, name, slug, loop (default true), note. The composed version's stages become `user.schema.<slug>`, a
 * progression schema of hers (R-768): the Composer's stages, no music computed here. Refused when the version is not composed.
 */
WORKSHOP_HANDLER(workshop_c_knowledge_keep_schema) {
    const char *item_id = NULL;
    const char *name = NULL;
    const char *slug = NULL;
    bool bad = false;
    const char *note = workshop_param_string(call->params, "note", &bad);
    const cJSON *version_member = cJSON_GetObjectItemCaseSensitive(call->params, "version");
    const cJSON *loop_member = cJSON_GetObjectItemCaseSensitive(call->params, "loop");
    if (!workshop_param_require_string(call->params, "item", &item_id, error) || !workshop_param_require_string(call->params, "name", &name, error) ||
        !workshop_param_require_string(call->params, "slug", &slug, error) || !read_note(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!cJSON_IsNumber(version_member) || version_member->valuedouble < 1 || version_member->valuedouble != (double)(int)version_member->valuedouble) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/version", "version is a whole number from 1");
    }
    if (loop_member && !cJSON_IsNull(loop_member) && !cJSON_IsBool(loop_member)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/loop", "loop is true or false");
    }
    if (strlen(name) == 0 || strlen(name) > 80 || !slug_ok(slug)) {
        return workshop_call_fail(error, strlen(name) == 0 || strlen(name) > 80 ? URSATZ_ERR_REQUEST : URSATZ_ERR_REQUEST, strlen(name) == 0 || strlen(name) > 80 ? "/params/name" : "/params/slug",
                                  "%s", strlen(name) == 0 || strlen(name) > 80 ? "name is 1 to 80 characters" : "slug is lower-case words joined by hyphens, as sad-lift");
    }
    bool loop = !cJSON_IsBool(loop_member) || cJSON_IsTrue(loop_member);
    cJSON *meta = NULL;
    cJSON *current = NULL;
    UrsatzStatus status = workshop_material_item_get(item_id, &meta, &current);
    bool is_harmony = status == URSATZ_OK && strcmp(common_json_string(meta, "type"), "harmony") == 0;
    cJSON_Delete(meta);
    cJSON_Delete(current);
    if (status != URSATZ_OK || !is_harmony) {
        return workshop_call_fail(error, status == URSATZ_ERR_NOT_FOUND ? URSATZ_ERR_NOT_FOUND : URSATZ_ERR_REQUEST, "/params/item", "%s is not a harmony item", item_id);
    }
    cJSON *body = NULL;
    status = workshop_material_item_get_version(item_id, version_member->valueint, &body);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/version", "no version %d of %s", version_member->valueint, item_id);
    }
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "result"), "harmony");
    const cJSON *stages = cJSON_GetObjectItemCaseSensitive(harmony, "stages");
    if (!cJSON_IsObject(harmony) || !cJSON_IsArray(stages) || cJSON_GetArraySize(stages) == 0) {
        cJSON_Delete(body);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/version", "the harmony version is not composed");
    }
    char id[128];
    snprintf(id, sizeof(id), "user.schema.%s", slug);
    char citation[700];
    snprintf(citation, sizeof(citation), "kept from %s version %d%s%s", item_id, version_member->valueint, note && note[0] ? ": " : "", note && note[0] ? note : "");
    cJSON *entry = cJSON_CreateObject();
    cJSON *content = entry ? cJSON_AddObjectToObject(entry, "content") : NULL;
    cJSON *materials = content ? cJSON_AddArrayToObject(content, "materials") : NULL;
    cJSON *citations = materials ? cJSON_AddArrayToObject(entry, "citations") : NULL;
    cJSON *cite = citations ? cJSON_CreateObject() : NULL;
    cJSON *examples = cite ? cJSON_AddArrayToObject(entry, "examples") : NULL;
    cJSON *example = examples ? cJSON_CreateObject() : NULL;
    cJSON *texts = example ? cJSON_CreateArray() : NULL;
    bool ok = texts && cJSON_AddStringToObject(entry, "kind", "schema") && cJSON_AddStringToObject(entry, "subtype", "progression") && cJSON_AddStringToObject(entry, "name", name) &&
              cJSON_AddItemToObject(content, "stages", cJSON_Duplicate(stages, true)) && cJSON_AddBoolToObject(content, "loop", loop) &&
              cJSON_AddStringToObject(content, "role", loop ? "loop" : "cadence") &&
              cJSON_AddStringToObject(content, "realizes", loop ? "omt.vocabulary.prolongational" : "omt.vocabulary.cadential") &&
              cJSON_AddItemToArray(materials, cJSON_CreateString(common_json_string(harmony, "material"))) &&
              (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(harmony, "with")) || cJSON_AddItemToArray(materials, cJSON_CreateString(common_json_string(harmony, "with")))) &&
              cJSON_AddStringToObject(cite, "source", "michelle") && cJSON_AddStringToObject(cite, "chapter", "harmony plan") && cJSON_AddStringToObject(cite, "note", citation) &&
              cJSON_AddItemToArray(citations, cite) && cJSON_AddStringToObject(entry, "status", "active") && cJSON_AddNumberToObject(entry, "version", 1) &&
              cJSON_AddStringToObject(example, "key", common_json_string(harmony, "key"));
    const cJSON *chord = NULL;
    cJSON_ArrayForEach(chord, cJSON_GetObjectItemCaseSensitive(harmony, "chords")) {
        ok = ok && cJSON_AddItemToArray(texts, cJSON_CreateString(common_json_string(chord, "text")));
    }
    ok = ok && cJSON_AddItemToObject(example, "chords", texts);
    ok = ok && cJSON_AddItemToArray(examples, example);
    cJSON_Delete(body);
    if (!ok) {
        cJSON_Delete(entry);
        if (!cite || cJSON_GetArraySize(citations) == 0) {
            cJSON_Delete(cite);
        }
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    status = store_entry(call, reply, error, id, entry, false);
    cJSON_Delete(entry);
    return status;
}
