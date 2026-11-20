#include "apply.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_tools.h"
#include "session.h"
#include "workflow.h"

/* Standard input read whole (a malloc'ed, NUL-terminated text); NULL when out of memory. */
static char *read_stream(FILE *stream, size_t *size) {
    size_t capacity = 8192;
    size_t used = 0;
    char *text = malloc(capacity + 1);
    while (text) {
        used += fread(text + used, 1, capacity - used, stream);
        if (used < capacity) {
            break;
        }
        capacity *= 2;
        char *grown = realloc(text, capacity + 1);
        if (!grown) {
            free(text);
            return NULL;
        }
        text = grown;
    }
    if (text) {
        text[used] = '\0';
        *size = used;
    }
    return text;
}

/* The commands of the input as an array: one JSON array, or JSON Lines (blank lines skipped). NULL after printing the failure. */
static cJSON *parse_commands(const char *text) {
    const char *start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    if (*start == '[') {
        cJSON *array = cJSON_Parse(start);
        if (!cJSON_IsArray(array)) {
            cJSON_Delete(array);
            cli_fail(CLI_EXIT_USAGE, "the commands are not a JSON array");
            return NULL;
        }
        return array;
    }
    cJSON *array = cJSON_CreateArray();
    size_t number = 0;
    const char *line = text;
    while (array && *line) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        char *copy = malloc(length + 1);
        if (!copy) {
            cJSON_Delete(array);
            cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
            return NULL;
        }
        memcpy(copy, line, length);
        copy[length] = '\0';
        number++;
        char *blank = copy;
        while (*blank == ' ' || *blank == '\t' || *blank == '\r') {
            blank++;
        }
        if (*blank != '\0') {
            cJSON *command = cJSON_Parse(blank);
            if (!command) {
                free(copy);
                cJSON_Delete(array);
                cli_fail(CLI_EXIT_USAGE, "line %zu is not JSON", number);
                return NULL;
            }
            cJSON_AddItemToArray(array, command);
        }
        free(copy);
        line = end ? end + 1 : line + length;
    }
    return array;
}

/* Checks one command object: {command, target?, base_version?, params?, file?} and no actor. Returns 0 or the exit status after printing. */
static int check_command(const cJSON *command, size_t number) {
    if (!cJSON_IsObject(command)) {
        return cli_fail(CLI_EXIT_USAGE, "command %zu is not a JSON object", number);
    }
    if (cJSON_GetObjectItemCaseSensitive(command, "actor")) {
        return cli_fail(CLI_EXIT_USAGE, "command %zu: the actor comes from --actor", number);
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(command, "command");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
        return cli_fail(CLI_EXIT_USAGE, "command %zu has no command name", number);
    }
    const cJSON *file = cJSON_GetObjectItemCaseSensitive(command, "file");
    if (file && !cJSON_IsString(file)) {
        return cli_fail(CLI_EXIT_USAGE, "command %zu: file is a path", number);
    }
    return 0;
}

int run_apply(const CliArgs *args) {
    const char *format = cli_value(args, "--format");
    bool json = false;
    if (format) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
    }
    bool wait = cli_switch(args, "--wait");
    const char *source = args->input_count > 0 ? args->inputs[0] : "-";
    char *text = NULL;
    size_t size = 0;
    char *source_path = strcmp(source, "-") == 0 ? NULL : cli_path_absolute(source);
    bool read = source_path ? cli_read_file(source_path, &text, &size) : (text = read_stream(stdin, &size)) != NULL;
    free(source_path);
    if (!read) {
        free(text);
        return cli_fail(CLI_EXIT_USAGE, "cannot read the commands '%s'", source);
    }
    cJSON *commands = parse_commands(text);
    free(text);
    if (!commands) {
        return CLI_EXIT_USAGE;
    }
    size_t count = (size_t)cJSON_GetArraySize(commands);
    /* Every command is checked, and every file it names read, before the first one runs: a mistake in the input applies nothing. */
    uint8_t **files = calloc(count ? count : 1, sizeof *files);
    size_t *file_sizes = calloc(count ? count : 1, sizeof *file_sizes);
    char **file_names = calloc(count ? count : 1, sizeof *file_names);
    int status = files && file_sizes && file_names ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    for (size_t i = 0; status == 0 && i < count; i++) {
        cJSON *command = cJSON_GetArrayItem(commands, (int)i);
        status = check_command(command, i + 1);
        const cJSON *file = status == 0 ? cJSON_GetObjectItemCaseSensitive(command, "file") : NULL;
        if (file) {
            char *absolute = cli_path_absolute(file->valuestring);
            char *data = NULL;
            size_t length = 0;
            if (!absolute || !cli_read_file(absolute, &data, &length)) {
                status = cli_fail(CLI_EXIT_USAGE, "command %zu: cannot read the file '%s'", i + 1, file->valuestring);
            } else {
                files[i] = (uint8_t *)data;
                file_sizes[i] = length;
                char *copy = strdup(absolute);
                file_names[i] = copy ? strdup(basename(copy)) : NULL;
                free(copy);
            }
            free(absolute);
        }
    }
    CliSession *session = NULL;
    if (status == 0) {
        status = cli_session_open(cli_global(), &session);
    }
    for (size_t i = 0; status == 0 && i < count; i++) {
        cJSON *command = cJSON_GetArrayItem(commands, (int)i);
        cJSON_DeleteItemFromObjectCaseSensitive(command, "file");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(command, "command");
        const cJSON *target = cJSON_GetObjectItemCaseSensitive(command, "target");
        WorkshopBytes bytes = {files[i], file_sizes[i], file_names[i] ? file_names[i] : ""};
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        UrsatzStatus result = cli_session_command(session, command, files[i] ? &bytes : NULL, &reply, &error);
        if (result != URSATZ_OK) {
            const cJSON *current = cJSON_GetObjectItemCaseSensitive(reply.json, "current_version");
            char version[48] = "";
            if (cJSON_IsNumber(current)) {
                snprintf(version, sizeof version, " (current version %.0f)", current->valuedouble);
            }
            status = cli_fail(result == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "command %zu (%s): %s%s%s%s", i + 1, name->valuestring,
                              error.message, error.path[0] ? " at " : "", error.path, version);
            workshop_reply_destroy(&reply);
            break;
        }
        const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
        const char *job_id = cJSON_IsObject(job) ? cli_json_string(job, "id") : "";
        if (json) {
            char *line = cJSON_PrintUnformatted(reply.json);
            printf("%s\n", line ? line : "{}");
            cJSON_free(line);
        } else {
            printf("%zu  %s  %s  journal %.0f", i + 1, name->valuestring, cJSON_IsString(target) ? target->valuestring : "-",
                   cli_json_number(reply.json, "journal_seq"));
            if (job_id[0] != '\0') {
                printf("  job %s", job_id);
            }
        }
        workshop_reply_destroy(&reply);
        if (wait && job_id[0] != '\0') {
            cJSON *finished = NULL;
            UrsatzError wait_error;
            memset(&wait_error, 0, sizeof wait_error);
            if (cli_session_wait_job(session, job_id, &finished, &wait_error) != URSATZ_OK) {
                printf("\n");
                status = cli_fail(CLI_EXIT_UNSATISFIABLE, "command %zu (%s): %s", i + 1, name->valuestring, wait_error.message);
                break;
            }
            const char *state = cli_json_string(finished, "state");
            if (!json) {
                printf("  %s", state);
            }
            if (strcmp(state, "done") != 0) {
                const char *why = cli_json_string(cJSON_GetObjectItemCaseSensitive(finished, "result"), "error");
                if (!json && why[0] != '\0') {
                    printf("  %s", why);
                }
            }
            cJSON_Delete(finished);
        }
        if (!json) {
            printf("\n");
        }
    }
    cli_session_close(session);
    for (size_t i = 0; i < count; i++) {
        free(files[i]);
        free(file_names[i]);
    }
    free(files);
    free(file_sizes);
    free(file_names);
    cJSON_Delete(commands);
    return status;
}
