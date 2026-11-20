#include "recognize.h"
#include "cli_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "cli_options.h"
#include "motive_input.h"

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
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", RECOGNIZE_USAGE_LINE + 7);
    fprintf(out, "Names the chain of registry operations (see variants --list-ops) that turns the source motive into the\n");
    fprintf(out, "target, with every parameter set, how far the target is from the source, and what it kept and changed.\n");
    fprintf(out, "The chain replayed with the variants command gives a motive equal to the target. Operations may differ from\n");
    fprintf(out, "those that made the target: inverting then transposing equals transposing then inverting.\n\n");
    fprintf(out, "  <source> <target>    each a MIDI file, a MusicXML file, a note list or a motive JSON file\n");
    fprintf(out, "  --format, --form, --meter, --key, --pickup   as for the motive command, for the source; the target takes\n");
    fprintf(out, "                       the same form, meter and key\n");
    motive_extras_print_help(out, false);
    fprintf(out, "  --target-format <f>  the target's format (default auto)\n");
    fprintf(out, "  --target-pickup <b>  the target's pickup in beats (default none)\n");
    fprintf(out, "  --target-harmony \"<text>\"   the target's own chords, as --harmony\n");
    fprintf(out, "  --target-feature <l:a-b>     a labelled run of the target's notes; repeatable. The target has the source's\n");
    fprintf(out, "                       --role\n");
    motive_input_print_basic_help(out);
    fprintf(out, "  --depth <n>          the most operations in the chain, 1 to %ld (default %ld)\n", cli_limit("variant_chain"), cli_limit("variant_chain"));
    fprintf(out, "  --json               print {found, chain, distance, remoteness, kept, changed, derivation}\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 the search ran, found or not; 1 usage, an invalid argument or an input that cannot be loaded.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz recognize theme.txt variant.txt --meter 4/4 --key \"D minor\" --json\n");
}

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->help = args->help;
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
    options->target_format_text = cli_value(args, "--target-format");
    options->target_pickup_text = cli_value(args, "--target-pickup");
    options->target_harmony_text = cli_value(args, "--target-harmony");
    options->depth_text = cli_value(args, "--depth");
    const char *print_format = cli_value(args, "--format");
    if (print_format) {
        options->json = cli_equals_ignore_case(print_format, "json");
        if (!options->json && !cli_equals_ignore_case(print_format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", print_format);
        }
    }
    return 0;
}

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static void print_fraction(const char *label, const cJSON *item) {
    printf("%s: %.0f/%.0f\n", label, cJSON_GetArrayItem(item, 0)->valuedouble, cJSON_GetArrayItem(item, 1)->valuedouble);
}

static void print_text(const cJSON *document) {
    if (cJSON_IsTrue(member(document, "found"))) {
        printf("Chain:");
        const cJSON *step = NULL;
        const char *separator = "";
        cJSON_ArrayForEach(step, member(member(document, "chain"), "steps")) {
            const cJSON *op = member(step, "op");
            printf("%s %s", separator, cJSON_IsString(op) ? op->valuestring : "");
            separator = ",";
        }
        printf("\n");
    } else {
        printf("No chain found\n");
    }
    print_fraction("Distance", member(document, "distance_fraction"));
    print_fraction("Remoteness", member(document, "remoteness_fraction"));
    const cJSON *derivation = member(document, "derivation");
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
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.have_target) {
        return cli_fail(CLI_EXIT_USAGE, "recognize needs a source and a target motive\n%s", RECOGNIZE_USAGE_LINE);
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
