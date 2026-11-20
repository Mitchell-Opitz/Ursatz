#include "coordinator/command_schemas.h"

#include <stdio.h>
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
