#include "coordinator/command_text.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <text/text_tree.h>

#include "coordinator/command_schemas.h"

static UrsatzStatus refuse(UrsatzError *error, int line, const char *format, ...) __attribute__((format(printf, 3, 4)));

static UrsatzStatus refuse(UrsatzError *error, int line, const char *format, ...) {
    if (error) {
        char message[400];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(message, sizeof message, format, arguments);
        va_end(arguments);
        if (line > 0) {
            snprintf(error->message, sizeof error->message, "line %d: %s", line, message);
        } else {
            snprintf(error->message, sizeof error->message, "%s", message);
        }
        snprintf(error->path, sizeof error->path, "/params/text");
    }
    return URSATZ_ERR_REQUEST;
}

static const cJSON *find_param(const cJSON *schema, const char *name) {
    const cJSON *param;
    cJSON_ArrayForEach(param, cJSON_GetObjectItemCaseSensitive(schema, "params")) {
        if (strcmp(cJSON_GetObjectItemCaseSensitive(param, "name")->valuestring, name) == 0) {
            return param;
        }
    }
    return NULL;
}

static bool is_rational_unit(const cJSON *unit) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(unit, "num");
    const cJSON *d = cJSON_GetObjectItemCaseSensitive(unit, "den");
    return cJSON_IsObject(unit) && cJSON_IsNumber(n) && cJSON_IsNumber(d) && unit->child && unit->child->next && !unit->child->next->next;
}

/* The text of a scalar unit that may stand for a string: a string as it is, a number or a rational as written; NULL for anything else. */
static char *scalar_text(const cJSON *unit) {
    if (cJSON_IsString(unit)) {
        return strdup(unit->valuestring);
    }
    if (cJSON_IsNumber(unit) || is_rational_unit(unit)) {
        return text_scalar_print(unit);
    }
    return NULL;
}

/* A unit as the value of a param of `type`, a new tree, or NULL when it is not one (the caller names the line and the param). `items` types a list's units. */
static cJSON *convert(const char *type, const cJSON *param, const cJSON *unit, const char *items) {
    if (strcmp(type, "string") == 0) {
        char *text = scalar_text(unit);
        cJSON *out = text ? cJSON_CreateString(text) : NULL;
        free(text);
        return out;
    }
    if (strcmp(type, "integer") == 0) {
        return cJSON_IsNumber(unit) ? cJSON_Duplicate(unit, true) : NULL;
    }
    if (strcmp(type, "boolean") == 0) {
        return cJSON_IsBool(unit) ? cJSON_Duplicate(unit, true) : NULL;
    }
    if (strcmp(type, "rational") == 0) {
        if (is_rational_unit(unit) || cJSON_IsNumber(unit)) {
            char *text = text_scalar_print(unit);
            cJSON *out = text ? cJSON_CreateString(text) : NULL;
            free(text);
            return out;
        }
        return cJSON_IsString(unit) ? cJSON_CreateString(unit->valuestring) : NULL;
    }
    if (strcmp(type, "enum") == 0) {
        return cJSON_IsString(unit) ? cJSON_CreateString(unit->valuestring) : NULL;
    }
    if (strcmp(type, "object") == 0) {
        return cJSON_IsObject(unit) && !is_rational_unit(unit) ? cJSON_Duplicate(unit, true) : NULL;
    }
    if (strcmp(type, "array") == 0) {
        if (!cJSON_IsArray(unit)) {
            return NULL;
        }
        cJSON *list = cJSON_CreateArray();
        const cJSON *item;
        cJSON_ArrayForEach(item, unit) {
            cJSON *converted = items && strcmp(items, "object") != 0 ? convert(items, param, item, NULL) : cJSON_Duplicate(item, true);
            if (!converted) {
                cJSON_Delete(list);
                return NULL;
            }
            cJSON_AddItemToArray(list, converted);
        }
        return list;
    }
    return NULL;
}

static const char *type_words(const char *type) {
    return strcmp(type, "string") == 0     ? "a string"
           : strcmp(type, "integer") == 0  ? "a whole number"
           : strcmp(type, "boolean") == 0  ? "true or false"
           : strcmp(type, "rational") == 0 ? "a rational like 3/4"
           : strcmp(type, "object") == 0   ? "an object (write its members below the command)"
           : strcmp(type, "array") == 0    ? "a list like [a b]"
                                           : "one of its words";
}

/* Adds param `name` = value to params, refusing a second value. */
static bool add_param(cJSON *params, const char *name, cJSON *value, int line, UrsatzError *error) {
    if (cJSON_GetObjectItemCaseSensitive(params, name)) {
        cJSON_Delete(value);
        refuse(error, line, "%s is given twice", name);
        return false;
    }
    cJSON_AddItemToObject(params, name, value);
    return true;
}

/* One command line and the member lines below it. */
static UrsatzStatus parse_command(const TextLines *lines, size_t *pos, cJSON *commands, UrsatzError *error) {
    const TextLine *line = &lines->lines[*pos];
    (*pos)++;
    TextError text_error = {0, ""};
    cJSON *units = text_units_parse(line->text, line->number, &text_error);
    if (!units) {
        return refuse(error, line->number, "%s", text_error.message);
    }
    const cJSON *name = units->child;
    if (!cJSON_IsString(name)) {
        cJSON_Delete(units);
        return refuse(error, line->number, "a command starts with its name");
    }
    const cJSON *schema = workshop_command_schema(name->valuestring);
    if (!schema) {
        UrsatzStatus status = refuse(error, line->number, "unknown command '%s'", name->valuestring);
        cJSON_Delete(units);
        return status;
    }
    cJSON *command = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    UrsatzStatus status = URSATZ_OK;
    if (!command || !params || !cJSON_AddStringToObject(command, "command", name->valuestring)) {
        cJSON_Delete(command);
        cJSON_Delete(params);
        cJSON_Delete(units);
        return refuse(error, 0, "out of memory");
    }
    const cJSON *unit = name->next;
    const cJSON *target_kind = cJSON_GetObjectItemCaseSensitive(schema, "target");
    if (cJSON_IsString(target_kind)) {
        const char *kind = target_kind->valuestring;
        size_t kind_length = strlen(kind);
        if (!cJSON_IsString(unit) || strncmp(unit->valuestring, kind, kind_length) != 0 || unit->valuestring[kind_length] != ':' ||
            unit->valuestring[kind_length + 1] == '\0' || unit->valuestring[kind_length + 1] == '@') {
            status = refuse(error, line->number, "%s works on %s:<id>", name->valuestring, kind);
        } else {
            const char *id = unit->valuestring + kind_length + 1;
            const char *at = strstr(id, "@v");
            char *target = strndup(id, at ? (size_t)(at - id) : strlen(id));
            long version = -1;
            if (at) {
                char *end = NULL;
                version = strtol(at + 2, &end, 10);
                if (end == at + 2 || *end != '\0' || version < 1) {
                    status = refuse(error, line->number, "'%s' is not a version: write %s:<id>@v<n>", at, kind);
                }
            } else if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(schema, "base"))) {
                status = refuse(error, line->number, "give the version you read: %s:<id>@v<n>", kind);
            }
            if (status == URSATZ_OK) {
                cJSON_AddStringToObject(command, "target", target);
                if (version > 0) {
                    cJSON_AddNumberToObject(command, "base_version", (double)version);
                }
            }
            free(target);
            unit = unit->next;
        }
    }
    bool takes_file = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(schema, "bytes"));
    while (status == URSATZ_OK && unit) {
        if (!cJSON_IsString(unit)) {
            status = refuse(error, line->number, "a param name is expected here");
            break;
        }
        const char *param_name = unit->valuestring;
        const cJSON *value = unit->next;
        if (takes_file && strcmp(param_name, "file") == 0) {
            if (!cJSON_IsString(value)) {
                status = refuse(error, line->number, "file needs the path of the file");
            } else {
                cJSON_AddStringToObject(command, "file", value->valuestring);
                unit = value->next;
            }
            continue;
        }
        const cJSON *param = find_param(schema, param_name);
        if (!param) {
            status = refuse(error, line->number, "%s takes no param '%s'", name->valuestring, param_name);
            break;
        }
        const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
        if (!value) {
            status = refuse(error, line->number, "%s needs a value: %s", param_name, type_words(type));
            break;
        }
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(param, "items");
        cJSON *converted = convert(type, param, value, cJSON_IsString(items) ? items->valuestring : NULL);
        if (!converted) {
            status = refuse(error, line->number, "%s must be %s", param_name, type_words(type));
            break;
        }
        if (!add_param(params, param_name, converted, line->number, error)) {
            status = URSATZ_ERR_REQUEST;
            break;
        }
        unit = value->next;
    }
    cJSON_Delete(units);
    /* the member lines below: params in the tree form */
    if (status == URSATZ_OK && *pos < lines->count && lines->lines[*pos].indent > 0) {
        if (lines->lines[*pos].indent != 2) {
            status = refuse(error, lines->lines[*pos].number, "unexpected indent");
        } else {
            cJSON *block = text_block_parse(lines, pos, 2, &text_error);
            if (!block) {
                status = refuse(error, text_error.line, "%s", text_error.message);
            } else if (!cJSON_IsObject(block)) {
                status = refuse(error, lines->lines[*pos - 1].number, "members of the params expected below a command, not list items");
            } else {
                cJSON *member = block->child;
                while (status == URSATZ_OK && member) {
                    const cJSON *param = find_param(schema, member->string);
                    cJSON *next = member->next;
                    if (!param) {
                        status = refuse(error, lines->lines[*pos - 1].number, "%s takes no param '%s'", name->valuestring, member->string);
                    } else {
                        cJSON *taken = cJSON_DetachItemFromObjectCaseSensitive(block, member->string);
                        const char *type = cJSON_GetObjectItemCaseSensitive(param, "type")->valuestring;
                        const cJSON *items = cJSON_GetObjectItemCaseSensitive(param, "items");
                        cJSON *converted = strcmp(type, "object") == 0 || strcmp(type, "array") == 0
                                               ? (strcmp(type, "object") == 0 ? (cJSON_IsObject(taken) ? taken : NULL)
                                                                              : (cJSON_IsArray(taken) ? taken : NULL))
                                               : convert(type, param, taken, cJSON_IsString(items) ? items->valuestring : NULL);
                        if (converted != taken) {
                            cJSON_Delete(taken);
                        }
                        if (!converted) {
                            status = refuse(error, lines->lines[*pos - 1].number, "%s must be %s", member->string, type_words(type));
                        } else if (!add_param(params, member->string, converted, lines->lines[*pos - 1].number, error)) {
                            status = URSATZ_ERR_REQUEST;
                        }
                    }
                    member = next;
                }
            }
            cJSON_Delete(block);
        }
        if (status == URSATZ_OK && *pos < lines->count && lines->lines[*pos].indent > 0) {
            status = refuse(error, lines->lines[*pos].number, "unexpected indent");
        }
    }
    if (status != URSATZ_OK || !cJSON_AddItemToObject(command, "params", params)) {
        cJSON_Delete(params);
        cJSON_Delete(command);
        return status != URSATZ_OK ? status : refuse(error, 0, "out of memory");
    }
    cJSON_AddItemToArray(commands, command);
    return URSATZ_OK;
}

UrsatzStatus workshop_command_text_parse(const char *text, cJSON **out, UrsatzError *error) {
    *out = NULL;
    TextLines lines;
    TextError text_error = {0, ""};
    if (!text_lines_split(text, &lines, &text_error)) {
        return refuse(error, text_error.line, "%s", text_error.message);
    }
    cJSON *commands = cJSON_CreateArray();
    size_t pos = 0;
    UrsatzStatus status = commands ? URSATZ_OK : refuse(error, 0, "out of memory");
    if (status == URSATZ_OK && lines.count > 0 && lines.lines[0].indent == 0 && strncmp(lines.lines[0].text, "ursatz-text ", 12) == 0) {
        if (strcmp(lines.lines[0].text, "ursatz-text 1 commands") != 0) {
            status = refuse(error, lines.lines[0].number, "the header must be ursatz-text 1 commands");
        }
        pos = 1;
    }
    while (status == URSATZ_OK && pos < lines.count) {
        if (lines.lines[pos].indent != 0) {
            status = refuse(error, lines.lines[pos].number, "unexpected indent: a command starts at the left margin");
            break;
        }
        status = parse_command(&lines, &pos, commands, error);
    }
    text_lines_free(&lines);
    cJSON *answer = status == URSATZ_OK ? cJSON_CreateObject() : NULL;
    if (status == URSATZ_OK && (!answer || !cJSON_AddItemToObject(answer, "commands", commands))) {
        cJSON_Delete(answer);
        cJSON_Delete(commands);
        return refuse(error, 0, "out of memory");
    }
    if (status != URSATZ_OK) {
        cJSON_Delete(commands);
        return status;
    }
    *out = answer;
    return URSATZ_OK;
}
