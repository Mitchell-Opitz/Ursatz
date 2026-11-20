#include "composition/sections.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UrsatzStatus fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) __attribute__((format(printf, 4, 5)));

static UrsatzStatus fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof error->message, format, arguments);
        va_end(arguments);
        snprintf(error->path, sizeof error->path, "%s", path);
    }
    return status;
}

static bool is_loop(const cJSON *request) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request, "schema_version");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(request, "form");
    return (cJSON_IsNumber(version) && version->valuedouble == 1) || (cJSON_IsString(form) && strcmp(form->valuestring, "game-loop") == 0);
}

static const char *text_param(const cJSON *params, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool given(const cJSON *params, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    return item && !cJSON_IsNull(item);
}

/* The index of the section with this id, -1 when there is none. */
static int find_section(const cJSON *sections, const char *id, const char *key) {
    int index = 0;
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(section, key);
        if (cJSON_IsString(name) && strcmp(name->valuestring, id) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

cJSON *workshop_sections_ids(const cJSON *request) {
    cJSON *ids = cJSON_CreateArray();
    bool loop = is_loop(request);
    const cJSON *section;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(section, loop ? "id" : "name");
        if (ids && cJSON_IsString(id)) {
            cJSON_AddItemToArray(ids, cJSON_CreateString(id->valuestring));
        }
    }
    return ids;
}

/* Replaces member `name` of the object with a copy of value (removing it first), keeping its place when it had one. */
static bool put(cJSON *object, const char *name, const cJSON *value) {
    cJSON *copy = cJSON_Duplicate(value, true);
    if (!copy) {
        return false;
    }
    if (cJSON_GetObjectItemCaseSensitive(object, name)) {
        cJSON_ReplaceItemInObjectCaseSensitive(object, name, copy);
        return true;
    }
    return cJSON_AddItemToObject(object, name, copy);
}

static bool put_string(cJSON *object, const char *name, const char *value) {
    cJSON *text = cJSON_CreateString(value);
    if (!text) {
        return false;
    }
    if (cJSON_GetObjectItemCaseSensitive(object, name)) {
        cJSON_ReplaceItemInObjectCaseSensitive(object, name, text);
        return true;
    }
    return cJSON_AddItemToObject(object, name, text);
}

static bool put_fill(cJSON *object, const char *kind) {
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(object, "fill");
    const cJSON *current_kind = cJSON_GetObjectItemCaseSensitive(current, "kind");
    if (cJSON_IsString(current_kind) && strcmp(current_kind->valuestring, kind) == 0) {
        return true; /* the fill is already of this kind: a motif fill keeps its motive */
    }
    cJSON *fill = cJSON_CreateObject();
    if (!fill || !cJSON_AddStringToObject(fill, "kind", kind)) {
        cJSON_Delete(fill);
        return false;
    }
    if (cJSON_GetObjectItemCaseSensitive(object, "fill")) {
        cJSON_ReplaceItemInObjectCaseSensitive(object, "fill", fill);
        return true;
    }
    return cJSON_AddItemToObject(object, "fill", fill);
}

/* Sets (or, for the word `none` / `home`, removes) a member whose value is an object or a word. */
static bool put_object_or_word(cJSON *object, const char *name, const cJSON *value, const char *remove_word) {
    if (cJSON_IsString(value) && strcmp(value->valuestring, remove_word) == 0) {
        cJSON_DeleteItemFromObjectCaseSensitive(object, name);
        return true;
    }
    return put(object, name, value);
}

/* The members of `params` the section.set command writes, in the order it lists them. */
static const char *const kSetMembers[] = {"role", "letter", "function", "cadence", "pickup", "fill", "region", "harmony"};

static bool set_members(cJSON *section, const cJSON *params, bool loop, char *list, size_t size) {
    bool ok = true;
    size_t used = 0;
    for (size_t i = 0; ok && i < sizeof kSetMembers / sizeof kSetMembers[0]; i++) {
        const char *name = kSetMembers[i];
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, name);
        if (!given(params, name)) {
            continue;
        }
        if (strcmp(name, "role") == 0) {
            /* a section that says `part` keeps saying it */
            ok = put_string(section, cJSON_GetObjectItemCaseSensitive(section, "part") ? "part" : "role", value->valuestring);
        } else if (strcmp(name, "letter") == 0 || strcmp(name, "pickup") == 0) {
            ok = cJSON_IsString(value) ? put_string(section, name, value->valuestring) : put(section, name, value);
        } else if (strcmp(name, "function") == 0 || strcmp(name, "cadence") == 0) {
            if (strcmp(value->valuestring, "none") == 0) {
                cJSON_DeleteItemFromObjectCaseSensitive(section, name);
            } else {
                ok = put_string(section, name, value->valuestring);
            }
        } else if (strcmp(name, "fill") == 0) {
            ok = put_fill(section, value->valuestring);
        } else if (strcmp(name, "region") == 0) {
            ok = put_object_or_word(section, name, value, "home");
        } else {
            ok = put_object_or_word(section, name, value, "none");
        }
        used += (size_t)snprintf(list + used, used < size ? size - used : 0, "%s%s", used ? ", " : "", name);
    }
    (void)loop;
    return ok;
}

static void default_note(char *note, size_t size, const char *command, const cJSON *params, const char *members) {
    const char *id = text_param(params, "id");
    const char *after = text_param(params, "after");
    const char *before = text_param(params, "before");
    if (strcmp(command, "section.insert") == 0) {
        snprintf(note, size, "insert section %s %s %s", id ? id : "", after ? "after" : "before", after ? after : (before ? before : ""));
    } else if (strcmp(command, "section.delete") == 0) {
        snprintf(note, size, "delete section %s", id ? id : "");
    } else if (strcmp(command, "section.move") == 0) {
        snprintf(note, size, "move section %s %s %s", id ? id : "", after ? "after" : "before", after ? after : (before ? before : ""));
    } else if (strcmp(command, "section.resize") == 0) {
        const cJSON *bars = cJSON_GetObjectItemCaseSensitive(params, "bars");
        const cJSON *beats = cJSON_GetObjectItemCaseSensitive(params, "beats");
        snprintf(note, size, "resize section %s to %.0f %s", id ? id : "", cJSON_IsNumber(bars) ? bars->valuedouble : beats ? beats->valuedouble : 0,
                 cJSON_IsNumber(bars) ? "bars" : "beats");
    } else {
        snprintf(note, size, "set %s of section %s", members, id ? id : "");
    }
}

UrsatzStatus workshop_sections_edit(const char *command, const cJSON *request, const cJSON *params, cJSON **edited, char *note, size_t note_size,
                                    UrsatzError *error) {
    *edited = NULL;
    if (!cJSON_IsObject(request)) {
        return fail(error, URSATZ_ERR_REQUEST, "/target", "the piece has no request to edit");
    }
    bool loop = is_loop(request);
    bool is_set = strcmp(command, "section.set") == 0;
    bool form_member = given(params, "region") || given(params, "harmony") || given(params, "fill");
    bool other_member = given(params, "role") || given(params, "letter") || given(params, "function") || given(params, "cadence") ||
                        given(params, "pickup");
    if (!loop && (!is_set || other_member || !form_member)) {
        return fail(error, URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    }
    cJSON *copy = cJSON_Duplicate(request, true);
    if (!copy) {
        return fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON *sections = cJSON_GetObjectItemCaseSensitive(copy, "sections");
    if (!cJSON_IsArray(sections)) {
        if (loop || !is_set) {
            cJSON_Delete(copy);
            return fail(error, URSATZ_ERR_REQUEST, "/target", "the request has no sections");
        }
        sections = cJSON_AddArrayToObject(copy, "sections");
        if (!sections) {
            cJSON_Delete(copy);
            return fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    const char *key = loop ? "id" : "name";
    const char *id = text_param(params, "id");
    int index = find_section(sections, id, key);
    UrsatzStatus status = URSATZ_OK;
    char members[160] = "";
    if (!id) {
        status = fail(error, URSATZ_ERR_REQUEST, "/params/id", "id is required");
    } else if (strcmp(command, "section.insert") == 0) {
        const char *after = text_param(params, "after");
        const char *before = text_param(params, "before");
        const char *anchor_name = after ? after : before;
        int anchor = find_section(sections, anchor_name, "id");
        if (index >= 0) {
            status = fail(error, URSATZ_ERR_CONFLICT, "/params/id", "section '%s' exists", id);
        } else if (anchor < 0) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, after ? "/params/after" : "/params/before", "unknown section '%s'", anchor_name);
        } else {
            cJSON *section = cJSON_CreateObject();
            const char *role = text_param(params, "role");
            bool ok = section && cJSON_AddStringToObject(section, "id", id) && cJSON_AddStringToObject(section, "role", role ? role : "body");
            if (ok && given(params, "letter")) {
                ok = cJSON_AddStringToObject(section, "letter", text_param(params, "letter")) != NULL;
            }
            if (ok && given(params, "bars")) {
                ok = put(section, "bars", cJSON_GetObjectItemCaseSensitive(params, "bars"));
            } else if (ok) {
                ok = put(section, "beats", cJSON_GetObjectItemCaseSensitive(params, "beats"));
            }
            if (ok && given(params, "pickup")) {
                const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(params, "pickup");
                ok = cJSON_IsString(pickup) ? cJSON_AddStringToObject(section, "pickup", pickup->valuestring) != NULL : put(section, "pickup", pickup);
            }
            if (ok && given(params, "cadence")) {
                ok = cJSON_AddStringToObject(section, "cadence", text_param(params, "cadence")) != NULL;
            }
            if (ok && given(params, "function")) {
                ok = cJSON_AddStringToObject(section, "function", text_param(params, "function")) != NULL;
            }
            if (ok && given(params, "fill")) {
                ok = put_fill(section, text_param(params, "fill"));
            }
            if (ok && given(params, "region")) {
                ok = put(section, "region", cJSON_GetObjectItemCaseSensitive(params, "region"));
            }
            if (ok && given(params, "harmony")) {
                ok = put(section, "harmony", cJSON_GetObjectItemCaseSensitive(params, "harmony"));
            }
            if (!ok || !cJSON_InsertItemInArray(sections, after ? anchor + 1 : anchor, section)) {
                cJSON_Delete(section);
                status = fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
            }
        }
    } else if (strcmp(command, "section.delete") == 0) {
        if (index < 0) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section '%s'", id);
        } else if (cJSON_GetArraySize(sections) <= 1) {
            status = fail(error, URSATZ_ERR_REQUEST, "/params/id", "a piece keeps at least one section");
        } else {
            cJSON_DeleteItemFromArray(sections, index);
        }
    } else if (strcmp(command, "section.move") == 0) {
        const char *after = text_param(params, "after");
        const char *before = text_param(params, "before");
        const char *anchor_name = after ? after : before;
        int anchor = find_section(sections, anchor_name, "id");
        if (index < 0) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section '%s'", id);
        } else if (anchor < 0) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, after ? "/params/after" : "/params/before", "unknown section '%s'", anchor_name);
        } else if (anchor == index) {
            status = fail(error, URSATZ_ERR_REQUEST, after ? "/params/after" : "/params/before", "a section cannot move next to itself");
        } else {
            cJSON *moved = cJSON_DetachItemFromArray(sections, index);
            int position = find_section(sections, anchor_name, "id");
            if (!moved || !cJSON_InsertItemInArray(sections, after ? position + 1 : position, moved)) {
                cJSON_Delete(moved);
                status = fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
            }
        }
    } else if (strcmp(command, "section.resize") == 0) {
        cJSON *section = index >= 0 ? cJSON_GetArrayItem(sections, index) : NULL;
        if (!section) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section '%s'", id);
        } else {
            const char *given_unit = given(params, "bars") ? "bars" : "beats";
            const char *other_unit = given(params, "bars") ? "beats" : "bars";
            cJSON_DeleteItemFromObjectCaseSensitive(section, other_unit);
            if (!put(section, given_unit, cJSON_GetObjectItemCaseSensitive(params, given_unit))) {
                status = fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
            }
        }
    } else {
        cJSON *section = index >= 0 ? cJSON_GetArrayItem(sections, index) : NULL;
        if (loop && !section) {
            status = fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section '%s'", id);
        } else {
            if (!section) {
                /* a form piece: the overlay entry for the section of the form, made when it is missing */
                section = cJSON_CreateObject();
                if (!section || !cJSON_AddStringToObject(section, "name", id) || !cJSON_AddItemToArray(sections, section)) {
                    cJSON_Delete(section);
                    section = NULL;
                    status = fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
                }
            }
            if (section && !set_members(section, params, loop, members, sizeof members)) {
                status = fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
            }
        }
    }
    if (status != URSATZ_OK) {
        cJSON_Delete(copy);
        return status;
    }
    if (note && note_size > 0) {
        const char *given_note = text_param(params, "note");
        if (given_note && given_note[0]) {
            snprintf(note, note_size, "%s", given_note);
        } else {
            default_note(note, note_size, command, params, members);
        }
    }
    *edited = copy;
    return URSATZ_OK;
}
