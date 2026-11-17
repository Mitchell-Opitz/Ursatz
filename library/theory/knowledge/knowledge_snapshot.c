#include "theory/knowledge/knowledge_snapshot.h"

#include <cJSON.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/knowledge/knowledge_embedded.h"

#define MATERIAL_STEP_MAX 12
#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL
#define SOURCES_SCHEMA "ursatz-knowledge-sources"
#define USER_PREFIX "user."

typedef struct {
    KnowledgeEntry entry;
    PitchMaterial material;
    bool has_material;
    char *strings; /* the id, kind, subtype and name texts the entry and the material point into */
} Slot;

struct KnowledgeSnapshot {
    Slot *slots;
    size_t count;
    size_t capacity;
    char hash[17];
};

static void fail(char *message, size_t message_size, const char *path, const char *id, const char *field,
                 const char *problem) {
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size, "%s: %s: %s: %s", path, id != NULL ? id : "-", field, problem);
    }
}

static uint64_t fnv(uint64_t hash, const char *text) {
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++) {
        hash = (hash ^ *p) * FNV_PRIME;
    }
    return hash;
}

static void hex16(uint64_t value, char out[17]) {
    snprintf(out, 17, "%016llx", (unsigned long long)value);
}

void knowledge_snapshot_destroy(KnowledgeSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    for (size_t i = 0; i < snapshot->count; i++) {
        free(snapshot->slots[i].strings);
    }
    free(snapshot->slots);
    free(snapshot);
}

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static bool is_integer(const cJSON *item) {
    return cJSON_IsNumber(item) && item->valuedouble == (double)item->valueint;
}

/* Reads a steps array: 1 to 12 integers from 0 to 11, ascending, the first 0. Returns NULL, or the problem. */
static const char *read_steps(const cJSON *array, int steps[MATERIAL_STEP_MAX], size_t *count) {
    if (!cJSON_IsArray(array)) {
        return "not an array";
    }
    size_t n = (size_t)cJSON_GetArraySize(array);
    if (n < 1 || n > MATERIAL_STEP_MAX) {
        return "not 1 to 12 steps";
    }
    size_t i = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!is_integer(item) || item->valueint < 0 || item->valueint > 11) {
            return "a step is not an integer from 0 to 11";
        }
        if ((i == 0 && item->valueint != 0) || (i > 0 && item->valueint <= steps[i - 1])) {
            return "steps are not ascending from 0";
        }
        steps[i++] = item->valueint;
    }
    *count = n;
    return NULL;
}

static bool read_status(const char *text, KnowledgeEntryStatus *status) {
    if (strcmp(text, "reference") == 0) {
        *status = KNOWLEDGE_STATUS_REFERENCE;
    } else if (strcmp(text, "active") == 0) {
        *status = KNOWLEDGE_STATUS_ACTIVE;
    } else if (strcmp(text, "retired") == 0) {
        *status = KNOWLEDGE_STATUS_RETIRED;
    } else {
        return false;
    }
    return true;
}

static const cJSON *text_field(const cJSON *entry, const char *name, bool required, const char **problem) {
    const cJSON *item = member(entry, name);
    if (item == NULL) {
        *problem = required ? "missing" : NULL;
        return NULL;
    }
    if (!cJSON_IsString(item) || item->valuestring[0] == '\0') {
        *problem = "not a non-empty string";
        return NULL;
    }
    *problem = NULL;
    return item;
}

/* Reads one entry into a new slot. */
static KnowledgeStatus read_entry(KnowledgeSnapshot *snapshot, const cJSON *entry, const char *path, bool user, char *message,
                                  size_t message_size) {
    const char *problem = NULL;
    const char *id = NULL;
    if (!cJSON_IsObject(entry)) {
        fail(message, message_size, path, NULL, "entry", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *id_item = text_field(entry, "id", true, &problem);
    if (id_item == NULL) {
        fail(message, message_size, path, NULL, "id", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    id = id_item->valuestring;
    for (size_t i = 0; i < snapshot->count; i++) {
        if (strcmp(snapshot->slots[i].entry.id, id) == 0) {
            fail(message, message_size, path, id, "id", "already defined");
            return KNOWLEDGE_ERR_DUPLICATE;
        }
    }
    bool user_id = strncmp(id, USER_PREFIX, strlen(USER_PREFIX)) == 0;
    if (user != user_id) {
        fail(message, message_size, path, id, "id", user ? "a user id starts with \"user.\"" : "a repository id may not start with \"user.\"");
        return KNOWLEDGE_ERR_NAMESPACE;
    }
    const cJSON *kind = text_field(entry, "kind", true, &problem);
    if (kind == NULL) {
        fail(message, message_size, path, id, "kind", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *subtype = text_field(entry, "subtype", false, &problem);
    if (problem != NULL) {
        fail(message, message_size, path, id, "subtype", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *name = text_field(entry, "name", true, &problem);
    if (name == NULL) {
        fail(message, message_size, path, id, "name", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *status_item = text_field(entry, "status", true, &problem);
    KnowledgeEntryStatus status = KNOWLEDGE_STATUS_REFERENCE;
    if (status_item == NULL || !read_status(status_item->valuestring, &status)) {
        fail(message, message_size, path, id, "status", status_item == NULL ? problem : "not reference, active or retired");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *version = member(entry, "version");
    if (!is_integer(version) || version->valueint < 1) {
        fail(message, message_size, path, id, "version", "not an integer from 1");
        return KNOWLEDGE_ERR_FIELD;
    }

    PitchMaterial material;
    memset(&material, 0, sizeof(material));
    bool has_material = strcmp(kind->valuestring, "material") == 0 && subtype != NULL &&
                        strcmp(subtype->valuestring, "unordered") == 0;
    if (has_material) {
        const cJSON *content = member(entry, "content");
        const cJSON *steps = cJSON_IsObject(content) ? member(content, "steps") : NULL;
        if (steps == NULL) {
            fail(message, message_size, path, id, "content.steps", "missing");
            return KNOWLEDGE_ERR_FIELD;
        }
        problem = read_steps(steps, material.steps, &material.step_count);
        if (problem != NULL) {
            fail(message, message_size, path, id, "content.steps", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *descending = member(content, "descending_steps");
        if (descending != NULL) {
            problem = read_steps(descending, material.descending_steps, &material.descending_step_count);
            if (problem != NULL) {
                fail(message, message_size, path, id, "content.descending_steps", problem);
                return KNOWLEDGE_ERR_FIELD;
            }
        }
    }

    char *text = cJSON_PrintUnformatted(entry);
    if (text == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    if (snapshot->count == snapshot->capacity) {
        size_t capacity = snapshot->capacity == 0 ? 16 : snapshot->capacity * 2;
        Slot *grown = realloc(snapshot->slots, capacity * sizeof(Slot));
        if (grown == NULL) {
            cJSON_free(text);
            return KNOWLEDGE_ERR_ALLOC;
        }
        snapshot->slots = grown;
        snapshot->capacity = capacity;
    }
    size_t id_size = strlen(id) + 1;
    size_t kind_size = strlen(kind->valuestring) + 1;
    size_t subtype_size = subtype != NULL ? strlen(subtype->valuestring) + 1 : 0;
    size_t name_size = strlen(name->valuestring) + 1;
    char *strings = malloc(id_size + kind_size + subtype_size + name_size);
    if (strings == NULL) {
        cJSON_free(text);
        return KNOWLEDGE_ERR_ALLOC;
    }
    Slot *slot = &snapshot->slots[snapshot->count];
    memset(slot, 0, sizeof(*slot));
    slot->strings = strings;
    slot->entry.id = strings;
    memcpy(strings, id, id_size);
    strings += id_size;
    slot->entry.kind = strings;
    memcpy(strings, kind->valuestring, kind_size);
    strings += kind_size;
    if (subtype != NULL) {
        slot->entry.subtype = strings;
        memcpy(strings, subtype->valuestring, subtype_size);
        strings += subtype_size;
    }
    slot->entry.name = strings;
    memcpy(strings, name->valuestring, name_size);
    slot->entry.status = status;
    slot->entry.version = version->valueint;
    hex16(fnv(FNV_OFFSET, text), slot->entry.hash);
    cJSON_free(text);
    if (has_material) {
        slot->material = material;
        slot->material.id = slot->entry.id;
        slot->material.name = slot->entry.name;
        slot->has_material = true;
    }
    snapshot->count++;
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_file(KnowledgeSnapshot *snapshot, const char *path, const char *text, size_t size, bool user,
                                 char *message, size_t message_size) {
    cJSON *root = cJSON_ParseWithLength(text, size);
    if (root == NULL) {
        fail(message, message_size, path, NULL, "file", "not valid JSON");
        return KNOWLEDGE_ERR_PARSE;
    }
    KnowledgeStatus status = KNOWLEDGE_OK;
    const cJSON *schema = member(root, "schema");
    if (cJSON_IsString(schema) && strcmp(schema->valuestring, SOURCES_SCHEMA) == 0) {
        cJSON_Delete(root);
        return KNOWLEDGE_OK; /* the list of sources has no entries */
    }
    const cJSON *entries = member(root, "entries");
    if (!cJSON_IsArray(entries)) {
        fail(message, message_size, path, NULL, "entries", "not an array");
        status = KNOWLEDGE_ERR_FIELD;
    } else {
        const cJSON *entry;
        cJSON_ArrayForEach(entry, entries) {
            status = read_entry(snapshot, entry, path, user, message, message_size);
            if (status != KNOWLEDGE_OK) {
                break;
            }
        }
    }
    cJSON_Delete(root);
    return status;
}

static int compare_slots(const void *a, const void *b) {
    return strcmp(((const Slot *)a)->entry.id, ((const Slot *)b)->entry.id);
}

KnowledgeStatus knowledge_snapshot_build(const KnowledgeUserFile *user_files, size_t user_file_count, KnowledgeSnapshot **out,
                                         char *message, size_t message_size) {
    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    if (out == NULL || (user_files == NULL && user_file_count > 0)) {
        fail(message, message_size, "-", NULL, "argument", "NULL");
        return KNOWLEDGE_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < user_file_count; i++) {
        if (user_files[i].path == NULL || user_files[i].text == NULL) {
            fail(message, message_size, "-", NULL, "user file", "NULL path or text");
            return KNOWLEDGE_ERR_NULL_ARGUMENT;
        }
    }
    KnowledgeSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    KnowledgeStatus status = KNOWLEDGE_OK;
    for (size_t i = 0; i < knowledge_embedded_file_count && status == KNOWLEDGE_OK; i++) {
        const KnowledgeEmbeddedFile *file = &knowledge_embedded_files[i];
        status = read_file(snapshot, file->path, file->text, file->size, false, message, message_size);
    }
    for (size_t i = 0; i < user_file_count && status == KNOWLEDGE_OK; i++) {
        status = read_file(snapshot, user_files[i].path, user_files[i].text, strlen(user_files[i].text), true, message,
                           message_size);
    }
    if (status != KNOWLEDGE_OK) {
        knowledge_snapshot_destroy(snapshot);
        return status;
    }
    qsort(snapshot->slots, snapshot->count, sizeof(Slot), compare_slots);
    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < snapshot->count; i++) {
        hash = fnv(hash, snapshot->slots[i].entry.id);
        hash = fnv(hash, "=");
        hash = fnv(hash, snapshot->slots[i].entry.hash);
        hash = fnv(hash, "\n");
    }
    hex16(hash, snapshot->hash);
    *out = snapshot;
    return KNOWLEDGE_OK;
}

const char *knowledge_snapshot_hash(const KnowledgeSnapshot *snapshot) {
    return snapshot != NULL ? snapshot->hash : NULL;
}

size_t knowledge_snapshot_entry_count(const KnowledgeSnapshot *snapshot) {
    return snapshot != NULL ? snapshot->count : 0;
}

const KnowledgeEntry *knowledge_snapshot_entry_at(const KnowledgeSnapshot *snapshot, size_t index) {
    return snapshot != NULL && index < snapshot->count ? &snapshot->slots[index].entry : NULL;
}

static const Slot *find_slot(const KnowledgeSnapshot *snapshot, const char *id) {
    if (snapshot == NULL || id == NULL) {
        return NULL;
    }
    size_t low = 0;
    size_t high = snapshot->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(id, snapshot->slots[middle].entry.id);
        if (order == 0) {
            return &snapshot->slots[middle];
        }
        if (order < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return NULL;
}

const KnowledgeEntry *knowledge_snapshot_entry(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = find_slot(snapshot, id);
    return slot != NULL ? &slot->entry : NULL;
}

const PitchMaterial *knowledge_snapshot_material(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = find_slot(snapshot, id);
    return slot != NULL && slot->has_material ? &slot->material : NULL;
}
