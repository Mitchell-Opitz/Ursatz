#include "formats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>

/* The members a format requires, one letter for the type of each: s string, n number, o object, a array, b boolean,  */
typedef struct {
    const char *name;
    const char *members[10];
    const char *types;
    int since; /* the first version of the format this row describes; a file is checked against the newest row at or below its own version */
} Format;

static const Format kFormats[] = {
    {WORKSHOP_FORMAT_DATA_FOLDER, {"created_at"}, "s", 1},
    {WORKSHOP_FORMAT_DATA_FOLDER, {"created_at"}, "s", 2},
    {WORKSHOP_FORMAT_CONFIG, {NULL}, "", 1},
    {WORKSHOP_FORMAT_PROJECT, {"id", "name", "description", "created_at", "updated_at"}, "sssss", 1},
    {WORKSHOP_FORMAT_ITEM, {"id", "type", "name", "current_version", "created_at", "updated_at"}, "sssnss", 1},
    {WORKSHOP_FORMAT_ITEM_VERSION, {"id", "kind", "version", "note", "lineage", "actor", "created_at"}, "ssnsoss", 1},
    {WORKSHOP_FORMAT_MOTIF_BODY, {"uses", "motif"}, "ao", 1},
    {WORKSHOP_FORMAT_THEME_BODY, {"uses", "theme"}, "ao", 1},
    {WORKSHOP_FORMAT_THEME_BODY, {"uses", "request"}, "ao", 2},
    {WORKSHOP_FORMAT_PIECE_BODY, {"uses", "piece"}, "ao", 1},
    {WORKSHOP_FORMAT_PIECE_BODY, {"uses", "request"}, "ao", 2},
    {WORKSHOP_FORMAT_HARMONY_BODY, {"uses", "request"}, "ao", 1},
    {WORKSHOP_FORMAT_OP_CHECKS, {"checks"}, "o", 1},
    {WORKSHOP_FORMAT_SUBJECT, {"id", "name", "kind", "note", "items", "pieces", "related", "created_at", "updated_at"}, "ssssaaass", 1},
    {WORKSHOP_FORMAT_LIBRARY_PIECE, {"filename", "source_format", "uploaded_at", "analyzer_version", "document_schema_version"}, "ssssn", 1},
    {WORKSHOP_FORMAT_COLLECTION, {"id", "name", "description", "pieces", "created_at", "updated_at"}, "sssass", 1},
    {WORKSHOP_FORMAT_STYLE, {"id", "name", "created_at", "updated_at", "source", "pieces"}, "ssssoa", 1},
    {WORKSHOP_FORMAT_LINK_RUN, {"id", "scope", "status", "created_at"}, "soss", 1},
    {WORKSHOP_FORMAT_RENDER, {"api_version", "composer_version", "composition_key", "render_key", "tempo"}, "ssssn", 1},
    {WORKSHOP_FORMAT_SETTINGS, {"scope", "values"}, "so", 1},
    {WORKSHOP_FORMAT_SETTINGS_SNAPSHOT, {"off", "values", "overrides"}, "aaa", 1},
    {WORKSHOP_FORMAT_LISTENING_SESSION, {"id", "created_at", "actor", "what", "taste", "note", "batches", "ratings", "picks"}, "ssssssaaa", 1},
    {WORKSHOP_FORMAT_ITEM_RATINGS, {"item_id", "ratings"}, "sa", 1},
    {WORKSHOP_FORMAT_ITEM_CRITIQUES, {"item_id", "critiques", "verdicts"}, "saa", 1},
};

/* The row of format for a file of version (0: the newest), or NULL for a name that is not a format. */
static const Format *find_format(const char *name, int version) {
    const Format *found = NULL;
    for (size_t i = 0; name && i < sizeof(kFormats) / sizeof(kFormats[0]); i++) {
        if (strcmp(kFormats[i].name, name) == 0 && (version == 0 || kFormats[i].since <= version) && (!found || kFormats[i].since > found->since)) {
            found = &kFormats[i];
        }
    }
    return found;
}

int workshop_format_version(const char *format) {
    const Format *newest = find_format(format, 0);
    return newest ? newest->since : 0;
}

void workshop_format_strip(cJSON *object) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, "format");
    cJSON_DeleteItemFromObjectCaseSensitive(object, "format_version");
}

bool workshop_format_stamp(cJSON *object, const char *format) {
    int version = workshop_format_version(format);
    workshop_format_strip(object);
    cJSON *name = cJSON_CreateString(format);
    cJSON *number = cJSON_CreateNumber(version);
    if (!version || !name || !number) {
        cJSON_Delete(name);
        cJSON_Delete(number);
        return false;
    }
    /* first members, so a person reading the file sees what it is before anything else */
    cJSON_AddItemToObject(object, "format", name);
    cJSON_AddItemToObject(object, "format_version", number);
    cJSON *format_item = cJSON_DetachItemFromObjectCaseSensitive(object, "format");
    cJSON *version_item = cJSON_DetachItemFromObjectCaseSensitive(object, "format_version");
    cJSON_InsertItemInArray(object, 0, version_item);
    cJSON_InsertItemInArray(object, 0, format_item);
    return true;
}

static bool type_ok(const cJSON *member, char type) {
    switch (type) {
    case 's':
        return cJSON_IsString(member);
    case 'n':
        return cJSON_IsNumber(member);
    case 'o':
        return cJSON_IsObject(member);
    case 'a':
        return cJSON_IsArray(member);
    case 'b':
        return cJSON_IsBool(member);
    default:
        return true;
    }
}

UrsatzStatus workshop_format_check(const cJSON *object, const char *format, const char *path, UrsatzError *error) {
    const char *where = path ? path : "";
    if (!cJSON_IsObject(object)) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, where, "%s is not a JSON object", where);
    }
    const char *named = common_json_string(object, "format");
    if (format && strcmp(named, format) != 0) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, where, "%s is not a %s file: its format is \"%s\"", where, format, named);
    }
    const Format *known = find_format(named, 0);
    if (!known) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, where, "%s has no workshop format: its format is \"%s\"", where, named);
    }
    long long version = 0;
    if (!common_json_long(object, "format_version", &version) || version < 1) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, where, "%s: format_version must be a whole number from 1", where);
    }
    if (version > workshop_format_version(known->name)) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, where,
                                "%s is format_version %lld of %s: this build reads up to version %d; use a newer Ursatz", where, version,
                                known->name, workshop_format_version(known->name));
    }
    known = find_format(named, (int)version);
    for (size_t i = 0; known->members[i]; i++) {
        const cJSON *member = cJSON_GetObjectItemCaseSensitive(object, known->members[i]);
        if (!member || !type_ok(member, known->types[i])) {
            return ursatz_error_set(error, URSATZ_ERR_FORMAT, known->members[i], "%s: %s is missing or has the wrong type (%s)", where,
                                    known->members[i], known->name);
        }
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_format_read(const char *path, const char *format, bool strip, cJSON **out, UrsatzError *error) {
    *out = NULL;
    cJSON *root = NULL;
    UrsatzStatus status = common_json_read_file(path, &root, error);
    if (status != URSATZ_OK) {
        return status;
    }
    status = workshop_format_check(root, format, path, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(root);
        return status;
    }
    if (strip) {
        workshop_format_strip(root);
    }
    *out = root;
    return URSATZ_OK;
}

UrsatzStatus workshop_format_write(const char *path, const char *format, cJSON *object, UrsatzError *error) {
    if (!workshop_format_stamp(object, format)) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory");
    }
    UrsatzStatus status = workshop_format_check(object, format, path, error);
    if (status == URSATZ_OK) {
        status = common_json_write_file(path, object, true, error);
    }
    workshop_format_strip(object);
    return status;
}

static bool ends_with(const char *path, const char *suffix) {
    size_t length = strlen(path);
    size_t suffix_length = strlen(suffix);
    return length > suffix_length && strcmp(path + length - suffix_length, suffix) == 0;
}

/* True when path ends in the segment tail (a whole segment, so "item.json" does not end "my-item.json"). */
static bool ends_with_segment(const char *path, const char *tail) {
    size_t length = strlen(path);
    size_t tail_length = strlen(tail);
    return length >= tail_length && strcmp(path + length - tail_length, tail) == 0 &&
           (length == tail_length || path[length - tail_length - 1] == '/');
}

static bool starts_with(const char *path, const char *prefix) {
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

/* True when the folder holding the file at path is a version folder, "v<digits>". */
static bool is_version_folder(const char *path) {
    const char *end = strrchr(path, '/');
    if (!end || end == path) {
        return false;
    }
    const char *start = end;
    while (start > path && start[-1] != '/') {
        start--;
    }
    if (*start != 'v' || start + 1 == end) {
        return false;
    }
    for (const char *c = start + 1; c < end; c++) {
        if (*c < '0' || *c > '9') {
            return false;
        }
    }
    return true;
}

const char *workshop_format_of_path(const char *path) {
    if (!path) {
        return NULL;
    }
    if (strcmp(path, "data-folder.json") == 0) {
        return WORKSHOP_FORMAT_DATA_FOLDER;
    }
    if (strcmp(path, "ursatz.json") == 0) {
        return WORKSHOP_FORMAT_CONFIG;
    }
    if (starts_with(path, "projects/")) {
        if (ends_with_segment(path, "project.json")) {
            return WORKSHOP_FORMAT_PROJECT;
        }
        if (ends_with_segment(path, "item.json")) {
            return WORKSHOP_FORMAT_ITEM;
        }
        if (ends_with_segment(path, "envelope.json")) {
            return WORKSHOP_FORMAT_ITEM_VERSION;
        }
        if (ends_with_segment(path, "body.json")) {
            return "";
        }
        if (ends_with_segment(path, "op-checks.json")) {
            return WORKSHOP_FORMAT_OP_CHECKS;
        }
        if (ends_with_segment(path, "settings.json")) {
            /* projects/<prj>/settings.json and projects/<prj>/items/<id>/settings.json are scopes; one under a version folder is its snapshot */
            return is_version_folder(path) ? WORKSHOP_FORMAT_SETTINGS_SNAPSHOT : WORKSHOP_FORMAT_SETTINGS;
        }
        return NULL;
    }
    if (starts_with(path, "subjects/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_SUBJECT;
    }
    if (starts_with(path, "library/pieces/") && ends_with_segment(path, "piece.json")) {
        return WORKSHOP_FORMAT_LIBRARY_PIECE;
    }
    if (starts_with(path, "library/collections/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_COLLECTION;
    }
    if (starts_with(path, "library/links/") && ends_with_segment(path, "run.json")) {
        return WORKSHOP_FORMAT_LINK_RUN;
    }
    if (starts_with(path, "settings/styles/") && ends_with_segment(path, "style.json")) {
        return WORKSHOP_FORMAT_STYLE;
    }
    if (starts_with(path, "settings/styles/") && ends_with_segment(path, "settings.json")) {
        return WORKSHOP_FORMAT_SETTINGS;
    }
    if (starts_with(path, "settings/taste/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_SETTINGS;
    }
    if (starts_with(path, "ratings/sessions/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_LISTENING_SESSION;
    }
    if (starts_with(path, "ratings/items/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_ITEM_RATINGS;
    }
    if (starts_with(path, "ratings/critiques/") && ends_with(path, ".json")) {
        return WORKSHOP_FORMAT_ITEM_CRITIQUES;
    }
    if (starts_with(path, "cache/renders/") && ends_with_segment(path, "render.json")) {
        return WORKSHOP_FORMAT_RENDER;
    }
    return NULL;
}
