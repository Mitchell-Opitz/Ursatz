#include "recognize.h"
#include "cli_tools.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "cli_options.h"
#include "cli_params.h"
#include "motive_input.h"
#include <common/json.h>

typedef struct {
    MotiveInput source;
    MotiveInput target;
    BasicInput basic;
    const char *target_format_text;
    const char *target_pickup_text;
    const char *target_harmony_text;
    const char *depth_text;
    bool have_source;
    bool have_target;
    bool json;
} Options;


static const CliOptionRow ROWS[] = {
    {"--target-format", offsetof(Options, target_format_text), false},
    {"--target-pickup", offsetof(Options, target_pickup_text), false},
    {"--target-harmony", offsetof(Options, target_harmony_text), false},
    {"--depth", offsetof(Options, depth_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    if (args->input_count > 0) {
        options->source.path = args->inputs[0];
        options->have_source = true;
    }
    if (args->input_count > 1) {
        options->target.path = args->inputs[1];
        options->have_target = true;
    }
    int extras_status = motive_extras_read(&options->source.extras, args, false);
    if (extras_status != 0) {
        return extras_status;
    }
    motive_input_read_basic(&options->basic, args);
    for (size_t i = 0; i < cli_repeat_count(args, "--target-feature"); i++) {
        if (cli_list_full(options->target.extras.feature_count, "motive_features")) {
            return cli_fail(CLI_EXIT_USAGE, "a motive has at most %ld features", cli_limit("motive_features"));
        }
        options->target.extras.feature_texts[options->target.extras.feature_count++] = cli_repeat_value(args, "--target-feature", i, NULL);
    }
    options->source.meter_text = cli_value(args, "--meter");
    options->source.key_text = cli_value(args, "--key");
    options->source.format_text = cli_value(args, "--input-format");
    options->source.form_text = cli_value(args, "--form");
    options->source.pickup_text = cli_value(args, "--pickup");
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    const char *print_format = cli_value(args, "--format");
    if (print_format) {
        options->json = cli_equals_ignore_case(print_format, "json");
        if (!options->json && !cli_equals_ignore_case(print_format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", print_format);
        }
    }
    return 0;
}

static void print_fraction(const char *label, const cJSON *item) {
    printf("%s: %.0f/%.0f\n", label, cJSON_GetArrayItem(item, 0)->valuedouble, cJSON_GetArrayItem(item, 1)->valuedouble);
}

static void print_text(const cJSON *document) {
    if (cJSON_IsTrue(common_json_member(document, "found"))) {
        printf("Chain:");
        const cJSON *step = NULL;
        const char *separator = "";
        cJSON_ArrayForEach(step, common_json_member(common_json_member(document, "chain"), "steps")) {
            const cJSON *op = common_json_member(step, "op");
            printf("%s %s", separator, cJSON_IsString(op) ? op->valuestring : "");
            separator = ",";
        }
        printf("\n");
    } else {
        printf("No chain found\n");
    }
    print_fraction("Distance", common_json_member(document, "distance_fraction"));
    print_fraction("Remoteness", common_json_member(document, "remoteness_fraction"));
    const cJSON *derivation = common_json_member(document, "derivation");
    printf("%s\n", cJSON_IsString(derivation) ? derivation->valuestring : "");
}

/* A part of the request: the motive's own input, harmony and features. The meter, key, role and pickups are the request's. */
static void keep_part_members(cJSON *part) {
    static const char *const SHARED[] = {"meter", "key", "role", "pickup"};
    for (size_t i = 0; i < sizeof SHARED / sizeof SHARED[0]; i++) {
        cJSON_DeleteItemFromObjectCaseSensitive(part, SHARED[i]);
    }
}

static bool add_text(cJSON *request, const char *name, const char *text) {
    return !text || cJSON_AddStringToObject(request, name, text) != NULL;
}

/* Builds the request of the options. Returns 0 or the status of the error printed. */
static int build_request(const Options *options, cJSON **out, MotiveFile *file) {
    cJSON *request = cJSON_CreateObject();
    cJSON *source = NULL;
    cJSON *target = NULL;
    cJSON *basic = NULL;
    int status = request ? 0 : cli_fail(CLI_EXIT_USAGE, "out of memory");
    /* The target is read as the source is: its form, meter, key and role; its own format, pickup and chords. */
    MotiveInput target_input = options->target;
    target_input.meter_text = options->source.meter_text;
    target_input.key_text = options->source.key_text;
    target_input.form_text = options->source.form_text;
    target_input.format_text = options->target_format_text;
    target_input.pickup_text = options->target_pickup_text;
    target_input.extras.harmony_text = options->target_harmony_text;
    target_input.extras.role_text = options->source.extras.role_text;
    if (status == 0) {
        status = motive_input_text_request(&options->source, &source, file);
    }
    if (status == 0) {
        status = motive_input_text_request(&target_input, &target, file);
    }
    if (status == 0 && motive_input_basic_given(&options->basic)) {
        MotiveInput basic_input;
        status = motive_input_basic_input(&options->basic, &options->source, &basic_input);
        if (status == 0) {
            status = motive_input_text_request(&basic_input, &basic, file);
        }
    }
    if (status == 0) {
        keep_part_members(source);
        keep_part_members(target);
        bool ok = add_text(request, "meter", options->source.meter_text) && add_text(request, "key", options->source.key_text) &&
                  add_text(request, "role", options->source.extras.role_text) &&
                  add_text(request, "pickup", options->source.pickup_text) &&
                  add_text(request, "target_pickup", options->target_pickup_text) &&
                  add_text(request, "depth", options->depth_text) && cJSON_AddItemToObject(request, "source", source);
        source = ok ? NULL : source;
        ok = ok && cJSON_AddItemToObject(request, "target", target);
        target = ok ? NULL : target;
        if (ok && basic) {
            ok = cJSON_AddItemToObject(request, "basic", basic);
            basic = ok ? NULL : basic;
        }
        /* The text output prints the exact fractions. */
        ok = ok && (options->json || cJSON_AddTrueToObject(request, "detail") != NULL);
        status = ok ? 0 : cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    cJSON_Delete(source);
    cJSON_Delete(target);
    cJSON_Delete(basic);
    if (status != 0) {
        cJSON_Delete(request);
        return status;
    }
    *out = request;
    return 0;
}

int run_recognize(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (!options.have_target) {
        return cli_needs("recognize", "a source and a target motive", true);
    }
    if (!options.source.meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "recognize needs --meter, which both motives are read in");
    }
    MotiveFile file;
    memset(&file, 0, sizeof file);
    cJSON *request = NULL;
    status = build_request(&options, &request, &file);
    if (status != 0) {
        return CLI_EXIT_USAGE;
    }
    if (!request) {
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer("recognize", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        MotiveInputNames names = {options.source.path, options.target.path, options.basic.path, NULL};
        motive_input_fail(&file, api_status, &response.error, &names);
        cli_response_destroy(&response);
        return CLI_EXIT_USAGE;
    }
    cJSON *document = cJSON_Parse(response.json);
    cli_response_destroy(&response);
    if (!document) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (options.json) {
        cli_print_json(document);
    } else {
        print_text(document);
    }
    cJSON_Delete(document);
    return 0;
}
