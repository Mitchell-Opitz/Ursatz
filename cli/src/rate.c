#include "rate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "address.h"
#include "cli_options.h"
#include "flags.h"
#include "session.h"
#include "workflow.h"

/* The exit status of a failed workshop call, after printing it: 1 for a request that was refused, 2 otherwise (as apply does). */
static int fail_call(UrsatzStatus status, const UrsatzError *error) {
    return cli_fail(status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error->message, error->path[0] ? " at " : "", error->path);
}

static bool whole(const char *text, long long min, long long max, long long *out) {
    char *end = NULL;
    long long value = strtoll(text, &end, 10);
    if (!text[0] || *end != '\0' || value < min || value > max) {
        return false;
    }
    *out = value;
    return true;
}

/* Sends one command ({"command", "params"} and a target when given); the id of its reply is copied into id (empty when it has none). 0 or the exit status. */
static int send(CliSession *session, const char *name, const char *target, cJSON *params, char *id, size_t id_size) {
    cJSON *command = cJSON_CreateObject();
    if (!command || !cJSON_AddStringToObject(command, "command", name) || (target && !cJSON_AddStringToObject(command, "target", target)) ||
        !cJSON_AddItemToObject(command, "params", params)) {
        cJSON_Delete(command);
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_command(session, command, NULL, &reply, &error);
    cJSON_Delete(command);
    if (status != URSATZ_OK) {
        int exit_status = fail_call(status, &error);
        workshop_reply_destroy(&reply);
        return exit_status;
    }
    snprintf(id, id_size, "%s", cli_json_string(reply.json, "id"));
    workshop_reply_destroy(&reply);
    return 0;
}

static cJSON *params_with(const char *key, const char *text, const char *note) {
    cJSON *params = cJSON_CreateObject();
    if (params && key && !cJSON_AddStringToObject(params, key, text)) {
        cJSON_Delete(params);
        return NULL;
    }
    if (params && note && !cJSON_AddStringToObject(params, "note", note)) {
        cJSON_Delete(params);
        return NULL;
    }
    return params;
}

/* rate item:<id>@v<n> --rating <1-5>. */
static int rate_version(const CliArgs *args, const CliAddress *address) {
    long long rating = 0;
    const char *text = cli_repeat_count(args, "--rating") == 1 ? cli_repeat_value(args, "--rating", 0, NULL) : NULL;
    if (!text || !whole(text, 1, 5, &rating)) {
        return cli_fail(CLI_EXIT_USAGE, "rating an item version needs one --rating from 1 to 5\n%s", cli_usage_of("rate"));
    }
    if (address->version < 1) {
        return cli_fail(CLI_EXIT_USAGE, "write the version to rate: item:<id>@v<n>\n%s", cli_usage_of("rate"));
    }
    CliSession *session = NULL;
    int status = cli_session_open(cli_global(), &session);
    if (status != 0) {
        return status;
    }
    cJSON *params = params_with(NULL, NULL, cli_value(args, "--note"));
    char id[128];
    if (!params || !cJSON_AddNumberToObject(params, "version", (double)address->version) || !cJSON_AddNumberToObject(params, "rating", (double)rating)) {
        cJSON_Delete(params);
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    } else {
        status = send(session, "ratings.rate-version", address->id, params, id, sizeof id);
    }
    if (status == 0) {
        printf("%s v%ld %lld\n", id, address->version, rating);
    }
    cli_session_close(session);
    return status;
}

int run_rate(const CliArgs *args) {
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "rate needs a session id or an item address\n%s", cli_usage_of("rate"));
    }
    CliAddress address;
    if (cli_is_address(args->inputs[0])) {
        if (!cli_address_parse(args->inputs[0], &address) || address.kind != CLI_ADDRESS_ITEM) {
            return cli_fail(CLI_EXIT_USAGE, "'%s' is not an item address: write item:<id>@v<n>", args->inputs[0]);
        }
        return rate_version(args, &address);
    }
    long long round = 0;
    long long seed = 0;
    const char *round_text = cli_value(args, "--round");
    const char *seed_text = cli_value(args, "--seed");
    if (!round_text || !whole(round_text, 1, 10000, &round)) {
        return cli_fail(CLI_EXIT_USAGE, "--round is required: a whole number from 1 to 10000\n%s", cli_usage_of("rate"));
    }
    if (!seed_text || !whole(seed_text, 0, 4294967295LL, &seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed is required: the seed of the sample, 0 to 4294967295\n%s", cli_usage_of("rate"));
    }
    size_t ratings = cli_repeat_count(args, "--rating");
    const char *better = cli_value(args, "--better");
    if (ratings == 0 && !better) {
        return cli_fail(CLI_EXIT_USAGE, "rate needs a --rating <batch>=<1-5> or --better <batch>|same\n%s", cli_usage_of("rate"));
    }
    /* every --rating is read before anything is sent */
    char batches[64][16];
    long long values[64];
    if (ratings > 64) {
        return cli_fail(CLI_EXIT_USAGE, "at most 64 --rating flags");
    }
    for (size_t i = 0; i < ratings; i++) {
        const char *text = cli_repeat_value(args, "--rating", i, NULL);
        const char *equals = strchr(text, '=');
        size_t length = equals ? (size_t)(equals - text) : 0;
        if (!equals || length < 1 || length >= sizeof batches[i] || !whole(equals + 1, 1, 5, &values[i])) {
            return cli_fail(CLI_EXIT_USAGE, "--rating is <batch>=<1-5>, not '%s'", text);
        }
        snprintf(batches[i], sizeof batches[i], "%.*s", (int)length, text);
    }
    CliSession *session = NULL;
    int status = cli_session_open(cli_global(), &session);
    if (status != 0) {
        return status;
    }
    const char *note = cli_value(args, "--note");
    for (size_t i = 0; status == 0 && i < ratings; i++) {
        cJSON *params = params_with("session", args->inputs[0], note);
        char id[128];
        if (!params || !cJSON_AddNumberToObject(params, "round", (double)round) || !cJSON_AddStringToObject(params, "batch", batches[i]) ||
            !cJSON_AddNumberToObject(params, "seed", (double)seed) || !cJSON_AddNumberToObject(params, "rating", (double)values[i])) {
            cJSON_Delete(params);
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
            break;
        }
        status = send(session, "ratings.rate", NULL, params, id, sizeof id);
        if (status == 0) {
            printf("%s %s %lld\n", id, batches[i], values[i]);
        }
    }
    if (status == 0 && better) {
        cJSON *params = params_with("session", args->inputs[0], note);
        char id[128];
        if (!params || !cJSON_AddNumberToObject(params, "round", (double)round) || !cJSON_AddNumberToObject(params, "seed", (double)seed) ||
            !cJSON_AddStringToObject(params, "better", better)) {
            cJSON_Delete(params);
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        } else {
            status = send(session, "ratings.pick", NULL, params, id, sizeof id);
        }
        if (status == 0) {
            printf("%s better %s\n", id, better);
        }
    }
    cli_session_close(session);
    return status;
}
