#include "items.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "address.h"
#include "cli_options.h"
#include "cli_params.h"
#include "cli_tools.h"
#include "render.h"
#include "session.h"
#include "workflow.h"

/* Prints the failure of a workshop call as "Error: <message>[ at <path>]"; returns the exit status (1 for a request that was refused, 2 otherwise). */
static int fail_call(UrsatzStatus status, const UrsatzError *error) {
    return cli_fail(status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error->message, error->path[0] ? " at " : "",
                    error->path);
}

/* A whole number from min to max in text: false when it is not. */
static bool read_whole(const char *text, long long min, long long max, long long *out) {
    char *end = NULL;
    long long value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max) {
        return false;
    }
    *out = value;
    return true;
}

/* The seed of --seed into params ("seed"): 0 or the exit status after printing the failure. */
static int add_seed(const CliArgs *args, cJSON *params) {
    const char *text = cli_value(args, "--seed");
    long long seed = 0;
    if (text && !read_whole(text, 0, 4294967295LL, &seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'", text);
    }
    if (text && !cJSON_AddNumberToObject(params, "seed", (double)seed)) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    return 0;
}

/* The address of the first input, which must be an item address without a version (for a command that writes the next version). 0 or the status. */
static int read_item_address(const CliArgs *args, const char *command, CliAddress *address, bool version_allowed) {
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs an item address\n%s", command, cli_usage_of(command));
    }
    if (!cli_address_parse(args->inputs[0], address) || address->kind != CLI_ADDRESS_ITEM) {
        return cli_fail(CLI_EXIT_USAGE, "'%s' is not an item address: write item:<id>%s", args->inputs[0], version_allowed ? " or item:<id>@v<n>" : "");
    }
    if (address->version > 0 && !version_allowed) {
        return cli_fail(CLI_EXIT_USAGE, "%s works on the current version of an item: write item:<id>, not '%s'", command, args->inputs[0]);
    }
    return 0;
}

/* The item's current version (item.get), *version; 0 or the exit status after printing the failure. */
static int current_version(CliSession *session, const char *id, long *version) {
    cJSON *params = cJSON_CreateObject();
    if (!params || !cJSON_AddStringToObject(params, "id", id)) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_query(session, "item.get", params, &reply, &error);
    cJSON_Delete(params);
    if (status != URSATZ_OK) {
        return fail_call(status, &error);
    }
    *version = (long)cli_json_number(cJSON_GetObjectItemCaseSensitive(reply.json, "item"), "current_version");
    workshop_reply_destroy(&reply);
    return 0;
}

/*
 * Sends the command (params taken) to the item as of base (the --base flag, else the item's current version), and unless --no-wait waits for its job.
 * Prints "item <id> version <n>" for a job that is done, "Error: <reason>" for one that failed (2), the job's id for --no-wait.
 */
static int run_job(const CliArgs *args, const char *name, const CliAddress *address, cJSON *params) {
    long base = 0;
    const char *base_text = cli_value(args, "--base");
    long long base_value = 0;
    if (base_text && !read_whole(base_text, 1, 1000000, &base_value)) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_USAGE, "--base must be a version number from 1, not '%s'", base_text);
    }
    CliSession *session = NULL;
    int status = cli_session_open(cli_global(), &session);
    if (status != 0) {
        cJSON_Delete(params);
        return status;
    }
    base = (long)base_value;
    if (base == 0) {
        status = current_version(session, address->id, &base);
    }
    cJSON *command = status == 0 ? cJSON_CreateObject() : NULL;
    if (status == 0 && (!command || !cJSON_AddStringToObject(command, "command", name) || !cJSON_AddStringToObject(command, "target", address->id) ||
                        !cJSON_AddNumberToObject(command, "base_version", (double)base) || !cJSON_AddItemToObject(command, "params", params))) {
        cJSON_Delete(command);
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        params = NULL;
    } else if (status != 0) {
        cJSON_Delete(params);
    }
    if (status == 0) {
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        UrsatzStatus result = cli_session_command(session, command, NULL, &reply, &error);
        if (result != URSATZ_OK) {
            const cJSON *current = cJSON_GetObjectItemCaseSensitive(reply.json, "current_version");
            if (cJSON_IsNumber(current)) {
                status = cli_fail(result == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s (current version %.0f)", error.message,
                                  error.path[0] ? " at " : "", error.path, current->valuedouble);
            } else {
                status = fail_call(result, &error);
            }
            workshop_reply_destroy(&reply);
        } else {
            const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
            char job_id[128];
            snprintf(job_id, sizeof job_id, "%s", cJSON_IsObject(job) ? cli_json_string(job, "id") : "");
            workshop_reply_destroy(&reply);
            if (cli_switch(args, "--no-wait")) {
                printf("job %s\n", job_id);
            } else {
                cJSON *finished = NULL;
                UrsatzError wait_error;
                memset(&wait_error, 0, sizeof wait_error);
                if (cli_session_wait_job(session, job_id, &finished, &wait_error) != URSATZ_OK) {
                    status = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", wait_error.message);
                } else if (strcmp(cli_json_string(finished, "state"), "done") != 0) {
                    const char *why = cli_json_string(cJSON_GetObjectItemCaseSensitive(finished, "result"), "error");
                    status = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", why[0] ? why : "the job did not finish");
                    cJSON_Delete(finished);
                } else {
                    const cJSON *item = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(finished, "result"), "item");
                    printf("item %s version %.0f\n", address->id, cli_json_number(item, "current_version"));
                    cJSON_Delete(finished);
                }
            }
        }
    }
    cli_session_close(session);
    return status;
}

int run_compose(const CliArgs *args) {
    CliAddress address;
    int status = read_item_address(args, "compose", &address, false);
    if (status != 0) {
        return status;
    }
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    status = add_seed(args, params);
    if (status == 0 && cli_switch(args, "--update-uses") && !cJSON_AddBoolToObject(params, "update_uses", true)) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (status != 0) {
        cJSON_Delete(params);
        return status;
    }
    return run_job(args, "item.compose", &address, params);
}

int run_explore(const CliArgs *args) {
    CliAddress address;
    int status = read_item_address(args, "explore", &address, false);
    if (status != 0) {
        return status;
    }
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    status = add_seed(args, params);
    const char *distance_text = cli_value(args, "--distance");
    long long distance = 0;
    if (status == 0 && distance_text && !read_whole(distance_text, 1, 8, &distance)) {
        status = cli_fail(CLI_EXIT_USAGE, "--distance must be a whole number from 1 to 8, not '%s'", distance_text);
    }
    if (status == 0 && distance_text && !cJSON_AddNumberToObject(params, "distance", (double)distance)) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    const char *taste = cli_value(args, "--taste");
    if (status == 0 && taste && !cJSON_AddStringToObject(params, "taste", taste)) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (status != 0) {
        cJSON_Delete(params);
        return status;
    }
    return run_job(args, "item.explore", &address, params);
}

int run_restore(const CliArgs *args) {
    CliAddress address;
    int status = read_item_address(args, "restore", &address, true);
    if (status != 0) {
        return status;
    }
    if (address.version == 0) {
        return cli_needs("restore", "the version to copy: write item:<id>@v<n>", true);
    }
    const char *base_text = cli_value(args, "--base");
    long long base_value = 0;
    if (base_text && !read_whole(base_text, 1, 1000000, &base_value)) {
        return cli_fail(CLI_EXIT_USAGE, "--base must be a version number from 1, not '%s'", base_text);
    }
    CliSession *session = NULL;
    status = cli_session_open(cli_global(), &session);
    if (status != 0) {
        return status;
    }
    long base = (long)base_value;
    if (base == 0) {
        status = current_version(session, address.id, &base);
    }
    cJSON *command = status == 0 ? cJSON_CreateObject() : NULL;
    cJSON *params = command ? cJSON_AddObjectToObject(command, "params") : NULL;
    const char *note = cli_value(args, "--note");
    if (status == 0 && (!params || !cJSON_AddStringToObject(command, "command", "item.restore") || !cJSON_AddStringToObject(command, "target", address.id) ||
                        !cJSON_AddNumberToObject(command, "base_version", (double)base) || !cJSON_AddNumberToObject(params, "version", (double)address.version) ||
                        (note && !cJSON_AddStringToObject(params, "note", note)))) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (status == 0) {
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        UrsatzStatus result = cli_session_command(session, command, NULL, &reply, &error);
        if (result != URSATZ_OK) {
            const cJSON *current = cJSON_GetObjectItemCaseSensitive(reply.json, "current_version");
            if (cJSON_IsNumber(current)) {
                status = cli_fail(result == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s (current version %.0f)", error.message,
                                  error.path[0] ? " at " : "", error.path, current->valuedouble);
            } else {
                status = fail_call(result, &error);
            }
        } else {
            printf("item %s version %.0f (restored v%.0f)\n", address.id, cli_json_number(reply.json, "version"), cli_json_number(reply.json, "restored_from"));
        }
        workshop_reply_destroy(&reply);
    }
    cJSON_Delete(command);
    cli_session_close(session);
    return status;
}

int run_regenerate_item(const CliArgs *args) {
    CliAddress address;
    int status = read_item_address(args, "regenerate", &address, false);
    if (status != 0) {
        return status;
    }
    const char *section = cli_value(args, "--section");
    const char *lane = cli_value(args, "--lane");
    if (!section) {
        return cli_fail(CLI_EXIT_USAGE, "regenerate of an item needs --section <id>\n%s", cli_usage_of("regenerate"));
    }
    if (cli_value(args, "--out") || cli_value(args, "--node") || cli_value(args, "--midi") || cli_value(args, "--musicxml")) {
        return cli_fail(CLI_EXIT_USAGE, "regenerate of an item writes the next version: --node, --out, --midi and --musicxml are for a plan file");
    }
    long long version = 0;
    const char *version_text = cli_value(args, "--version");
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (version_text && !read_whole(version_text, 1, 1000000, &version)) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_USAGE, "--version must be a version number from 1, not '%s'", version_text);
    }
    if (!version_text) {
        CliSession *session = NULL;
        status = cli_session_open(cli_global(), &session);
        long current = 0;
        if (status == 0) {
            status = current_version(session, address.id, &current);
            cli_session_close(session);
        }
        if (status != 0) {
            cJSON_Delete(params);
            return status;
        }
        version = current;
    }
    bool ok = cJSON_AddNumberToObject(params, "version", (double)version) && cJSON_AddStringToObject(params, "section_id", section) &&
              (!lane || cJSON_AddStringToObject(params, "lane", lane));
    status = ok ? add_seed(args, params) : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    if (status != 0) {
        cJSON_Delete(params);
        return status;
    }
    return run_job(args, "item.regenerate", &address, params);
}

int run_render_item(const CliArgs *args) {
    CliAddress address;
    int status = read_item_address(args, "render", &address, true);
    if (status != 0) {
        return status;
    }
    const char *midi_path = cli_value(args, "--midi");
    const char *musicxml_path = cli_value(args, "--musicxml");
    if (!midi_path && !musicxml_path) {
        return cli_needs("render", "at least one of --midi or --musicxml", true);
    }
    char *midi_absolute = midi_path ? cli_path_absolute(midi_path) : NULL;
    char *musicxml_absolute = musicxml_path ? cli_path_absolute(musicxml_path) : NULL;
    CliSession *session = NULL;
    status = cli_session_open(cli_global(), &session);
    long version = (long)address.version;
    if (status == 0 && version == 0) {
        status = current_version(session, address.id, &version);
    }
    cJSON *plan = NULL;
    if (status == 0) {
        cJSON *params = cJSON_CreateObject();
        if (!params || !cJSON_AddStringToObject(params, "id", address.id) || !cJSON_AddNumberToObject(params, "version", (double)version) ||
            !cJSON_AddStringToObject(params, "name", "plan.json")) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        } else {
            WorkshopReply reply;
            UrsatzError error;
            memset(&error, 0, sizeof error);
            UrsatzStatus result = cli_session_query(session, "item.file", params, &reply, &error);
            if (result == URSATZ_ERR_NOT_FOUND || (result != URSATZ_OK && strcmp(error.message, "file not found") == 0)) {
                status = cli_fail(CLI_EXIT_UNSATISFIABLE, "item %s version %ld has no plan.json (a motif has none; see ursatz show)", address.id, version);
            } else if (result != URSATZ_OK) {
                status = fail_call(result, &error);
            } else {
                char *text = malloc(reply.size + 1);
                if (text) {
                    memcpy(text, reply.bytes, reply.size);
                    text[reply.size] = '\0';
                    plan = cJSON_Parse(text);
                    free(text);
                }
                if (!plan) {
                    status = cli_fail(CLI_EXIT_UNSATISFIABLE, "the plan of item %s version %ld cannot be read", address.id, version);
                }
                workshop_reply_destroy(&reply);
            }
        }
        cJSON_Delete(params);
    }
    cli_session_close(session);
    if (status == 0) {
        char name[300];
        snprintf(name, sizeof name, "item:%s@v%ld", address.id, version);
        status = cli_render_plan(plan, name, midi_absolute, musicxml_absolute, cli_value(args, "--tempo"), cli_switch(args, "--plain-midi"));
    }
    free(midi_absolute);
    free(musicxml_absolute);
    return status;
}
