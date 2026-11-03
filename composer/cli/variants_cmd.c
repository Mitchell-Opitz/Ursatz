#include "variants_cmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/variant_registry.h>

#include "cli_options.h"
#include "motive_extras.h"
#include "motive_input.h"
#include "util/util.h"

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
    const char *function_texts[VARIANT_FUNCTIONS];
    size_t function_count;
    bool list_ops;
    bool each_op;
    bool json;
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", VARIANTS_USAGE_LINE + 7);
    fprintf(out, "Lists the variant operations of the registry, or makes variants of a motive: each variant applies a chain\n");
    fprintf(out, "of up to %d operations. A parameter the chain leaves open is drawn from the seed, so the same motive,\n", VARIANT_CHAIN_MAX);
    fprintf(out, "chain and seed give the same variant, and another seed is a reroll.\n\n");
    fprintf(out, "  --list-ops           print every operation: name, label, category, citation, parameters and the\n");
    fprintf(out, "                       forms it applies to\n");
    fprintf(out, "  <input>              a MIDI file, a MusicXML file, a note list or a motive JSON file\n");
    fprintf(out, "  --format, --form, --meter, --key, --pickup   as for the motive command\n");
    motive_extras_print_help(out, false);
    motive_input_print_basic_help(out);
    fprintf(out, "  --function <name>    keep only variants that suit a function: beginning, continuation, contrast or\n");
    fprintf(out, "                       cadential (repeatable: a variant that suits any of them). Draws up to %d chains for\n", VARIANTS_DRAWS_PER_COUNT);
    fprintf(out, "                       each variant asked for, so fewer than --count may come back\n");
    fprintf(out, "  --distance <d>       close, medium or far: close draws 1-step chains and keeps remoteness at most 1/3,\n");
    fprintf(out, "                       medium 2 steps and over 1/3 up to 2/3, far 3 or 4 steps and over 2/3. Draws up to %d\n", VARIANTS_DRAWS_PER_COUNT);
    fprintf(out, "                       chains for each variant asked for, so fewer than --count may come back; combines with\n");
    fprintf(out, "                       --function (both must hold). Not with --chain\n");
    fprintf(out, "  --each-op            one variant for every operation that applies to the motive's form, in registry\n");
    fprintf(out, "                       order, each a one-step chain with its open parameters drawn from seed + the\n");
    fprintf(out, "                       operation's position in the registry. An operation the motive refuses is listed as\n");
    fprintf(out, "                       skipped, with the reason. Not with --chain, --count, --distance or --function\n");
    fprintf(out, "  --chain <file>       a JSON file {\"steps\": [{\"op\": \"invert\", \"params\": {\"tonal\": true}}]}, the same\n");
    fprintf(out, "                       chain for every variant. Default: each variant gets its own random chain\n");
    fprintf(out, "  --count <n>          how many variants, 1 to %d (default %d); variant i uses seed + i\n", VARIANTS_COUNT_MAX, VARIANTS_COUNT_DEFAULT);
    fprintf(out, "  --seed <n>           whole number from 0. Default: from the clock. The seed is printed\n");
    fprintf(out, "  --json               print as JSON: the seed, the source, and for each variant its seed, chain, kind, kept,\n");
    fprintf(out, "                       changed, derivation, recognizability, remoteness, functions and motive (with its analysis)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument, or an unreadable chain file; 2 the input cannot\n");
    fprintf(out, "be loaded or a step of the chain is refused.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer variants --list-ops\n");
    fprintf(out, "  ursatz-composer variants theme.txt --meter 4/4 --key \"D minor\" --count 4 --seed 7\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = 0;
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (motive_extras_take(&options->in.extras, argc, argv, &i, false, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (motive_input_take_basic(&options->basic, argc, argv, &i, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--list-ops") == 0) {
            options->list_ops = true;
        } else if (strcmp(argument, "--each-op") == 0) {
            options->each_op = true;
        } else if (strcmp(argument, "--function") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "--function needs a value");
            }
            const char *name = argv[++i];
            if (options->function_count == VARIANT_FUNCTIONS) {
                return cli_fail(CLI_EXIT_USAGE, "--function is given at most %d times", VARIANT_FUNCTIONS);
            }
            options->function_texts[options->function_count++] = name;
        } else if (strcmp(argument, "--meter") == 0 || strcmp(argument, "--key") == 0 ||
                   strcmp(argument, "--format") == 0 || strcmp(argument, "--form") == 0 ||
                   strcmp(argument, "--pickup") == 0 || strcmp(argument, "--chain") == 0 ||
                   strcmp(argument, "--count") == 0 || strcmp(argument, "--seed") == 0 ||
                   strcmp(argument, "--distance") == 0) {
            const char **slot = strcmp(argument, "--meter") == 0    ? &options->in.meter_text
                                : strcmp(argument, "--key") == 0    ? &options->in.key_text
                                : strcmp(argument, "--format") == 0 ? &options->in.format_text
                                : strcmp(argument, "--form") == 0   ? &options->in.form_text
                                : strcmp(argument, "--pickup") == 0 ? &options->in.pickup_text
                                : strcmp(argument, "--chain") == 0  ? &options->chain_path
                                : strcmp(argument, "--count") == 0  ? &options->count_text
                                : strcmp(argument, "--distance") == 0 ? &options->distance_text
                                                                    : &options->seed_text;
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
            }
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
            }
            *slot = argv[++i];
        } else if (argument[0] == '-' && argument[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s'", argument);
        } else if (!options->in.path) {
            options->in.path = argument;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one file", argument);
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
    if (!util_read_file(path, &text, NULL)) {
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
            printf("  Warning: %s\n", VARIANT_WARNING_LEAVES_HARMONY);
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
    char *text = status == 0 ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!text) {
        return status != 0 ? status : cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    ComposerResponse response;
    ComposerStatus api_status = composer_api_variants(text, NULL, &response);
    free(text);
    if (api_status != COMPOSER_OK) {
        if (strncmp(response.error.path, "/chain", 6) == 0 && options->chain_path) {
            status = cli_fail(CLI_EXIT_USAGE, "%s: %s", options->chain_path, response.error.message);
        } else {
            status = motive_input_fail(&file, api_status, &response.error, &names);
        }
        composer_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
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

int run_variants(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.list_ops && !options.in.path) {
        return cli_fail(CLI_EXIT_USAGE, "variants needs --list-ops or a motive input\n%s", VARIANTS_USAGE_LINE);
    }
    if (options.list_ops && (options.in.path || motive_extras_given(&options.in.extras) || motive_input_basic_given(&options.basic))) {
        return cli_fail(CLI_EXIT_USAGE, "--list-ops takes no motive input");
    }
    if (!options.list_ops) {
        return make_variants(&options);
    }
    ComposerResponse response;
    ComposerStatus api_status = composer_api_variant_operations(&response);
    if (api_status != COMPOSER_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        composer_response_destroy(&response);
        return status;
    }
    if (options.json) {
        fputs(response.json, stdout);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        if (!document) {
            composer_response_destroy(&response);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        print_operations(document);
        cJSON_Delete(document);
    }
    composer_response_destroy(&response);
    return 0;
}
