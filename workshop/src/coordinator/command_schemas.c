#include "coordinator/command_schemas.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/command_files.h"

#define MAX_FAMILIES 16
#define MAX_COMMANDS 128

static cJSON *families[MAX_FAMILIES];
static size_t family_count;
static const cJSON *commands[MAX_COMMANDS];
static size_t command_count;
static bool opened;
static UrsatzStatus open_status;
static UrsatzError open_error;

static UrsatzStatus fail(const char *path, const char *format, const char *a, const char *b) {
    open_status = URSATZ_ERR_INTERNAL;
    snprintf(open_error.path, sizeof open_error.path, "%s", path ? path : "");
    snprintf(open_error.message, sizeof open_error.message, format, a ? a : "", b ? b : "");
    return open_status;
}

static UrsatzStatus load(void) {
    for (size_t i = 0; i < workshop_command_file_count; i++) {
        const WorkshopCommandFile *file = &workshop_command_files[i];
        cJSON *tree = cJSON_ParseWithLength(file->data, file->size);
        const cJSON *schema = cJSON_GetObjectItemCaseSensitive(tree, "schema");
        const cJSON *list = cJSON_GetObjectItemCaseSensitive(tree, "commands");
        if (!tree || !cJSON_IsString(schema) || strcmp(schema->valuestring, "ursatz-commands") != 0 || !cJSON_IsArray(list)) {
            cJSON_Delete(tree);
            return fail("", "the command file %s is not a command file%s", file->path, "");
        }
        if (family_count >= MAX_FAMILIES) {
            cJSON_Delete(tree);
            return fail("", "too many command files at %s%s", file->path, "");
        }
        families[family_count++] = tree;
        const cJSON *command;
        cJSON_ArrayForEach(command, list) {
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(command, "name");
            if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
                return fail("", "a command of %s has no name%s", file->path, "");
            }
            for (size_t k = 0; k < command_count; k++) {
                if (strcmp(cJSON_GetObjectItemCaseSensitive(commands[k], "name")->valuestring, name->valuestring) == 0) {
                    return fail("", "the command %s is defined twice (second time in %s)", name->valuestring, file->path);
                }
            }
            if (command_count >= MAX_COMMANDS) {
                return fail("", "too many commands at %s%s", file->path, "");
            }
            commands[command_count++] = command;
        }
    }
    /* every schema has a row of the dispatch table and every row a schema */
    size_t rows = 0;
    const char *const *names = workshop_command_names(&rows);
    for (size_t i = 0; i < rows; i++) {
        if (!workshop_command_schema(names[i])) {
            return fail("", "the command %s has no schema in workshop/commands%s", names[i], "");
        }
    }
    for (size_t k = 0; k < command_count; k++) {
        const char *name = cJSON_GetObjectItemCaseSensitive(commands[k], "name")->valuestring;
        bool found = false;
        for (size_t i = 0; i < rows && !found; i++) {
            found = strcmp(names[i], name) == 0;
        }
        if (!found) {
            return fail("", "the schema of %s has no command in the dispatch table%s", name, "");
        }
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_command_schemas_open(UrsatzError *error) {
    if (!opened) {
        opened = true;
        open_status = load();
    }
    if (open_status != URSATZ_OK && error) {
        *error = open_error;
    }
    return open_status;
}

const cJSON *workshop_command_schema(const char *name) {
    for (size_t k = 0; name && k < command_count; k++) {
        if (strcmp(cJSON_GetObjectItemCaseSensitive(commands[k], "name")->valuestring, name) == 0) {
            return commands[k];
        }
    }
    return NULL;
}

size_t workshop_command_family_count(void) {
    return family_count;
}

const cJSON *workshop_command_family_at(size_t index) {
    return index < family_count ? families[index] : NULL;
}

/* --- the checks of one command --------------------------------------------------------------------------------------------------- */

static UrsatzStatus refuse(UrsatzError *error, const char *path, const char *format, ...) __attribute__((format(printf, 3, 4)));

static UrsatzStatus refuse(UrsatzError *error, const char *path, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof error->message, format, arguments);
        va_end(arguments);
        snprintf(error->path, sizeof error->path, "%s", path);
    }
    return URSATZ_ERR_REQUEST;
}

static const char *type_words(const cJSON *param, char *buffer, size_t size) {
    const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
    if (strcmp(type, "string") == 0) {
        return "a string";
    }
    if (strcmp(type, "integer") == 0) {
        return "a whole number";
    }
    if (strcmp(type, "boolean") == 0) {
        return "true or false";
    }
    if (strcmp(type, "rational") == 0) {
        return "a rational like 3/4";
    }
    if (strcmp(type, "value") == 0) {
        return "a number, true or false, a word, n/d or a list";
    }
    if (strcmp(type, "object") == 0) {
        const cJSON *words = cJSON_GetObjectItemCaseSensitive(param, "words");
        if (cJSON_IsArray(words) && cJSON_GetArraySize(words) > 0) {
            snprintf(buffer, size, "an object or the word %s", cJSON_GetArrayItem(words, 0)->valuestring);
            return buffer;
        }
        return "an object";
    }
    if (strcmp(type, "array") == 0) {
        return "a list";
    }
    /* an enum: one of its words */
    size_t used = (size_t)snprintf(buffer, size, "one of ");
    const cJSON *value;
    cJSON_ArrayForEach(value, cJSON_GetObjectItemCaseSensitive(param, "values")) {
        used += (size_t)snprintf(buffer + (used < size ? used : size), used < size ? size - used : 0, "%s%s", value == cJSON_GetObjectItemCaseSensitive(param, "values")->child ? "" : ", ",
                                 value->valuestring);
    }
    return buffer;
}

static bool is_whole(const cJSON *value) {
    return cJSON_IsNumber(value) && isfinite(value->valuedouble) && value->valuedouble == floor(value->valuedouble) && fabs(value->valuedouble) < 9007199254740992.0;
}

/* A rational as text (n or n/d, d above 0) or a whole number. */
static bool is_rational(const cJSON *value) {
    if (is_whole(value)) {
        return true;
    }
    if (!cJSON_IsString(value)) {
        return false;
    }
    const char *p = value->valuestring;
    if (*p == '-') {
        p++;
    }
    size_t digits = 0;
    while (*p >= '0' && *p <= '9') {
        p++;
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    if (*p == '\0') {
        return true;
    }
    if (*p != '/') {
        return false;
    }
    p++;
    long long denominator = 0;
    digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 18) {
        denominator = denominator * 10 + (*p - '0');
        p++;
        digits++;
    }
    return digits > 0 && *p == '\0' && denominator > 0;
}

/* A setting's value (R-676): a number, a boolean, a string (a ratio is the string n/d), or a list of numbers and strings. */
static bool is_setting_value(const cJSON *value, bool inside_list) {
    if (cJSON_IsNumber(value) || cJSON_IsString(value)) {
        return true;
    }
    if (cJSON_IsBool(value)) {
        return !inside_list;
    }
    if (cJSON_IsArray(value) && !inside_list) {
        const cJSON *item;
        cJSON_ArrayForEach(item, value) {
            if (!is_setting_value(item, true)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static bool type_holds(const char *type, const cJSON *param, const cJSON *value) {
    if (strcmp(type, "value") == 0) {
        return is_setting_value(value, false);
    }
    if (strcmp(type, "string") == 0) {
        return cJSON_IsString(value);
    }
    if (strcmp(type, "integer") == 0) {
        return is_whole(value);
    }
    if (strcmp(type, "boolean") == 0) {
        return cJSON_IsBool(value);
    }
    if (strcmp(type, "rational") == 0) {
        return is_rational(value);
    }
    if (strcmp(type, "object") == 0) {
        const cJSON *word;
        cJSON_ArrayForEach(word, cJSON_GetObjectItemCaseSensitive(param, "words")) {
            if (cJSON_IsString(value) && strcmp(word->valuestring, value->valuestring) == 0) {
                return true; /* a word in place of the object (a region `home`, a harmony `none`) */
            }
        }
        return cJSON_IsObject(value);
    }
    if (strcmp(type, "array") == 0) {
        return cJSON_IsArray(value);
    }
    if (strcmp(type, "enum") == 0) {
        const cJSON *word;
        cJSON_ArrayForEach(word, cJSON_GetObjectItemCaseSensitive(param, "values")) {
            if (cJSON_IsString(value) && strcmp(word->valuestring, value->valuestring) == 0) {
                return true;
            }
        }
        return false;
    }
    return false;
}

static bool given(const cJSON *value) {
    return value && !cJSON_IsNull(value);
}

UrsatzStatus workshop_command_check(const cJSON *schema, const char *target, const cJSON *params, UrsatzError *error) {
    const char *name = cJSON_GetObjectItemCaseSensitive(schema, "name")->valuestring;
    bool wants_target = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(schema, "target"));
    if (wants_target && target[0] == '\0') {
        return refuse(error, "/target", "%s needs a target", name);
    }
    if (!wants_target && target[0] != '\0') {
        return refuse(error, "/target", "%s takes no target", name);
    }
    const cJSON *declared = cJSON_GetObjectItemCaseSensitive(schema, "params");
    const cJSON *member;
    cJSON_ArrayForEach(member, params) {
        bool known = false;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, declared) {
            if (member->string && strcmp(cJSON_GetObjectItemCaseSensitive(entry, "name")->valuestring, member->string) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            char path[160];
            snprintf(path, sizeof path, "/params/%s", member->string ? member->string : "");
            return refuse(error, path, "%s takes no param '%s'", name, member->string ? member->string : "");
        }
    }
    const cJSON *param;
    cJSON_ArrayForEach(param, declared) {
        const char *param_name = cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring;
        const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
        char path[160];
        snprintf(path, sizeof path, "/params/%s", param_name);
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, param_name);
        if (!given(value)) {
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(param, "required"))) {
                return refuse(error, path, "%s is required", param_name);
            }
            continue;
        }
        if (!type_holds(type, param, value)) {
            char words[160];
            return refuse(error, path, "%s must be %s", param_name, type_words(param, words, sizeof words));
        }
        if (strcmp(type, "integer") == 0) {
            const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(param, "minimum");
            const cJSON *maximum = cJSON_GetObjectItemCaseSensitive(param, "maximum");
            if ((cJSON_IsNumber(minimum) && value->valuedouble < minimum->valuedouble) || (cJSON_IsNumber(maximum) && value->valuedouble > maximum->valuedouble)) {
                if (cJSON_IsNumber(minimum) && cJSON_IsNumber(maximum)) {
                    return refuse(error, path, "%s must be from %.0f to %.0f", param_name, minimum->valuedouble, maximum->valuedouble);
                }
                return cJSON_IsNumber(minimum) ? refuse(error, path, "%s must be %.0f or more", param_name, minimum->valuedouble)
                                               : refuse(error, path, "%s must be %.0f or less", param_name, maximum->valuedouble);
            }
        }
        const cJSON *max_length = cJSON_GetObjectItemCaseSensitive(param, "max_length");
        if (strcmp(type, "string") == 0 && cJSON_IsNumber(max_length) && strlen(value->valuestring) > (size_t)max_length->valuedouble) {
            return refuse(error, path, "%s is longer than %.0f characters", param_name, max_length->valuedouble);
        }
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(param, "items");
        if (strcmp(type, "array") == 0 && cJSON_IsString(items)) {
            const cJSON *item;
            int index = 0;
            cJSON_ArrayForEach(item, value) {
                if (!type_holds(items->valuestring, param, item)) {
                    char item_path[200];
                    snprintf(item_path, sizeof item_path, "%s/%d", path, index);
                    return refuse(error, item_path, "%s must be a list of %s", param_name,
                                  strcmp(items->valuestring, "string") == 0     ? "strings"
                                  : strcmp(items->valuestring, "integer") == 0  ? "whole numbers"
                                  : strcmp(items->valuestring, "boolean") == 0  ? "true or false"
                                  : strcmp(items->valuestring, "rational") == 0 ? "rationals like 3/4"
                                                                                : "objects");
                }
                index++;
            }
        }
    }
    const cJSON *group;
    cJSON_ArrayForEach(group, cJSON_GetObjectItemCaseSensitive(schema, "one_of")) {
        int count = 0;
        const cJSON *member;
        cJSON_ArrayForEach(member, group) {
            count += given(cJSON_GetObjectItemCaseSensitive(params, member->valuestring)) ? 1 : 0;
        }
        if (count != 1) {
            char list[256] = "";
            size_t used = 0;
            cJSON_ArrayForEach(member, group) {
                used += (size_t)snprintf(list + (used < sizeof list ? used : sizeof list), used < sizeof list ? sizeof list - used : 0, "%s%s",
                                         member == group->child ? "" : ", ", member->valuestring);
            }
            char path[160];
            snprintf(path, sizeof path, "/params/%s", group->child->valuestring);
            return refuse(error, path, "give exactly one of %s", list);
        }
    }
    return URSATZ_OK;
}
