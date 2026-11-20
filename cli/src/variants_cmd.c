#include "variants_cmd.h"
#include "cli_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "cli_options.h"
#include "motive_extras.h"
#include "motive_input.h"

#define VARIANTS_COUNT_MAX 16
#define VARIANTS_COUNT_DEFAULT 6
/* With --function, up to this many chains are drawn for each variant asked for. */
#define VARIANTS_DRAWS_PER_COUNT 4

typedef struct {
    MotiveInput in;
    BasicInput basic;
    const char *chain_path;
    const char *count_text;
    const char *seed_text;
    const char *distance_text;
    const char *function_texts[CLI_LIST_CAPACITY];
    size_t function_count;
    bool list_ops;
    bool each_op;
    bool json;
} Options;


static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->in.path = args->input_count > 0 ? args->inputs[0] : NULL;
    int extras_status = motive_extras_read(&options->in.extras, args, false);
    if (extras_status != 0) {
        return extras_status;
    }
    motive_input_read_basic(&options->basic, args);
    options->list_ops = cli_switch(args, "--list-ops");
    options->each_op = cli_switch(args, "--each-op");
    for (size_t i = 0; i < cli_repeat_count(args, "--function"); i++) {
        if (cli_list_full(options->function_count, "variant_functions")) {
            return cli_fail(CLI_EXIT_USAGE, "--function is given at most %ld times", cli_limit("variant_functions"));
        }
        options->function_texts[options->function_count++] = cli_repeat_value(args, "--function", i, NULL);
    }
    options->in.meter_text = cli_value(args, "--meter");
    options->in.key_text = cli_value(args, "--key");
    options->in.format_text = cli_value(args, "--input-format");
    options->in.form_text = cli_value(args, "--form");
    options->in.pickup_text = cli_value(args, "--pickup");
    options->chain_path = cli_value(args, "--chain");
    options->count_text = cli_value(args, "--count");
    options->seed_text = cli_value(args, "--seed");
    options->distance_text = cli_value(args, "--distance");
    const char *print_format = cli_value(args, "--format");
    if (print_format) {
        options->json = cli_equals_ignore_case(print_format, "json");
        if (!options->json && !cli_equals_ignore_case(print_format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", print_format);
        }
    }
    return 0;
}

/* --- the operation list -------------------------------------------------- */

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static const char *string_of(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* The text lines of the operation list, from the response JSON. */
static void print_operations(const cJSON *document) {
    const cJSON *operation = NULL;
    cJSON_ArrayForEach(operation, member(document, "operations")) {
        printf("%s  %s  (%s; %s)\n", string_of(operation, "name"), string_of(operation, "label"),
               string_of(operation, "category"), string_of(operation, "citation"));
        const cJSON *param = NULL;
        cJSON_ArrayForEach(param, member(operation, "params")) {
            printf("  --%s  %s  %s\n", string_of(param, "name"), string_of(param, "kind"), string_of(param, "description"));
        }
        printf("  forms: ");
        const cJSON *form = NULL;
        const char *separator = "";
        cJSON_ArrayForEach(form, member(operation, "forms")) {
            printf("%s%s", separator, cJSON_IsString(form) ? form->valuestring : "");
            separator = ", ";
        }
        printf("\n");
    }
}

/* --- the chain file ------------------------------------------------------ */

/* Reads the chain file into *chain (a JSON value the request carries). Returns 0 or the status of the error it printed. */
static int read_chain(const char *path, cJSON **chain) {
    char *text;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read chain file '%s'", path);
    }
    *chain = cJSON_Parse(text);
    free(text);
    if (!*chain) {
        return cli_fail(CLI_EXIT_USAGE, "chain file '%s' is not JSON", path);
    }
    return 0;
}

/* --- the variants -------------------------------------------------------- */

/* The motive document's text in its own form: notes, rhythm or contour. */
static const char *document_text(const cJSON *document) {
    const cJSON *analysis = member(document, "analysis");
    const char *form = string_of(analysis, "form");
    return string_of(analysis, strcmp(form, "rhythm") == 0 ? "rhythm_text" : strcmp(form, "contour") == 0 ? "contour_text" : "notes_text");
}

static void print_fraction(const char *label, const cJSON *item) {
    printf("  %s: %.0f/%.0f\n", label, cJSON_GetArrayItem(item, 0)->valuedouble, cJSON_GetArrayItem(item, 1)->valuedouble);
}

static void print_text(const cJSON *response) {
    printf("Source: %s\n", document_text(member(response, "source")));
    printf("Seed: %.0f\n", member(response, "seed")->valuedouble);
    size_t number = 0;
    const cJSON *variant = NULL;
    cJSON_ArrayForEach(variant, member(response, "variants")) {
        number++;
        printf("\nVariant %zu (seed %.0f):", number, member(variant, "seed")->valuedouble);
        const cJSON *step = NULL;
        const char *separator = "";
        cJSON_ArrayForEach(step, member(member(variant, "chain"), "steps")) {
            printf("%s %s", separator, string_of(step, "op"));
            separator = ",";
        }
        printf("\n  %s\n  %s\n", document_text(member(variant, "motive")), string_of(variant, "derivation"));
        if (cJSON_IsTrue(member(variant, "leaves_harmony"))) {
            /* The warning joins the analysis's own, last. */
            const cJSON *warnings = member(member(member(variant, "motive"), "analysis"), "warnings");
            const cJSON *warning = cJSON_GetArrayItem(warnings, cJSON_GetArraySize(warnings) - 1);
            printf("  Warning: %s\n", cJSON_IsString(warning) ? warning->valuestring : "");
        }
        print_fraction("Remoteness", member(variant, "remoteness_fraction"));
        printf("  Functions:");
        const cJSON *function = NULL;
        separator = "";
        cJSON_ArrayForEach(function, member(variant, "functions")) {
            printf("%s %s", separator, cJSON_IsString(function) ? function->valuestring : "");
            separator = ",";
        }
        printf("%s\n", cJSON_GetArraySize(member(variant, "functions")) ? "" : " none");
    }
    const cJSON *skipped = member(response, "skipped");
    if (cJSON_GetArraySize(skipped) > 0) {
        printf("\n");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, skipped) {
        printf("Skipped %s: %s\n", string_of(entry, "op"), string_of(entry, "reason"));
    }
}

/* The members of the command's own options, added to the request. Returns 0 or the status of the error printed. */
static int add_members(cJSON *request, const Options *options) {
    bool ok = true;
    if (options->count_text) {
        ok = cJSON_AddStringToObject(request, "count", options->count_text) != NULL;
    }
    if (ok && options->seed_text) {
        ok = cJSON_AddStringToObject(request, "seed", options->seed_text) != NULL;
    }
    if (ok && options->distance_text) {
        ok = cJSON_AddStringToObject(request, "distance", options->distance_text) != NULL;
    }
    if (ok && options->each_op) {
        ok = cJSON_AddTrueToObject(request, "each_op") != NULL;
    }
    cJSON *functions = ok && options->function_count > 0 ? cJSON_AddArrayToObject(request, "functions") : NULL;
    for (size_t i = 0; ok && options->function_count > 0 && i < options->function_count; i++) {
        ok = functions && cJSON_AddItemToArray(functions, cJSON_CreateString(options->function_texts[i]));
    }
    /* The text output prints the exact fractions. */
    ok = ok && (options->json || cJSON_AddTrueToObject(request, "detail") != NULL);
    return ok ? 0 : cli_fail(CLI_EXIT_USAGE, "out of memory");
}

/* Prints the notes the entry left (draws it skipped, variants it did not find) as the command always printed them. */
static void print_notices(const cJSON *response) {
    const cJSON *notice = NULL;
    cJSON_ArrayForEach(notice, member(response, "notices")) {
        fprintf(stderr, "%s\n", cJSON_IsString(notice) ? notice->valuestring : "");
    }
}

/* The variants command proper: build the request, make the variants through the API, print them. */
static int make_variants(const Options *options) {
    cJSON *chain = NULL;
    if (options->chain_path) {
        int status = read_chain(options->chain_path, &chain);
        if (status != 0) {
            return status;
        }
    }
    MotiveFile file;
    memset(&file, 0, sizeof file);
    cJSON *request = NULL;
    int status = motive_input_text_request(&options->in, &request, &file);
    if (status != 0) {
        cJSON_Delete(chain);
        return status;
    }
    MotiveInputNames names = {options->in.path, NULL, options->basic.path, NULL};
    if (motive_input_basic_given(&options->basic)) {
        MotiveInput basic_input;
        cJSON *basic = NULL;
        status = motive_input_basic_input(&options->basic, &options->in, &basic_input);
        if (status == 0) {
            status = motive_input_text_request(&basic_input, &basic, &file);
        }
        if (status != 0 || !cJSON_AddItemToObject(request, "basic", basic)) {
            cJSON_Delete(chain);
            cJSON_Delete(request);
            return status != 0 ? status : cli_fail(CLI_EXIT_USAGE, "out of memory");
        }
    }
    if (chain && !cJSON_AddItemToObject(request, "chain", chain)) {
        cJSON_Delete(chain);
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    status = add_members(request, options);
    if (status != 0) {
        cJSON_Delete(request);
        return status;
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer("variants", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        if (strncmp(response.error.path, "/chain", 6) == 0 && options->chain_path) {
            status = cli_fail(CLI_EXIT_USAGE, "%s: %s", options->chain_path, response.error.message);
        } else {
            status = motive_input_fail(&file, api_status, &response.error, &names);
        }
        cli_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    cli_response_destroy(&response);
    if (!document) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    print_notices(document);
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");
    if (options->json) {
        cli_print_json(document);
    } else {
        print_text(document);
    }
    cJSON_Delete(document);
    return 0;
}

int run_variants(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (!options.list_ops && !options.in.path) {
        return cli_fail(CLI_EXIT_USAGE, "variants needs --list-ops or a motive input\n%s", cli_usage_of("variants"));
    }
    if (options.list_ops && (options.in.path || motive_extras_given(&options.in.extras) || motive_input_basic_given(&options.basic))) {
        return cli_fail(CLI_EXIT_USAGE, "--list-ops takes no motive input");
    }
    if (!options.list_ops) {
        return make_variants(&options);
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer("variant_operations", NULL, NULL, &response);
    if (api_status != URSATZ_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        cli_response_destroy(&response);
        return status;
    }
    if (options.json) {
        fputs(response.json, stdout);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        if (!document) {
            cli_response_destroy(&response);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        print_operations(document);
        cJSON_Delete(document);
    }
    cli_response_destroy(&response);
    return 0;
}
