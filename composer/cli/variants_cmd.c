#include "variants_cmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
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
            if (strcmp(name, "beginning") != 0 && strcmp(name, "continuation") != 0 && strcmp(name, "contrast") != 0 &&
                strcmp(name, "cadential") != 0) {
                return cli_fail(CLI_EXIT_USAGE,
                                "--function must be beginning, continuation, contrast or cadential, not '%s'", name);
            }
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

static const char *const FORM_NAMES[] = {"full", "rhythm", "contour"};

static void print_forms(const VariantOperation *operation) {
    const char *separator = "";
    for (unsigned form = 0; form < 3; form++) {
        if (operation->forms & VARIANT_FORM_BIT(form)) {
            printf("%s%s", separator, FORM_NAMES[form]);
            separator = ", ";
        }
    }
    printf("\n");
}

static void print_operations(void) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        printf("%s  %s  (%s; %s)\n", operation->name, operation->label, variant_category_name(operation->category),
               operation->citation);
        for (size_t p = 0; p < operation->param_count; p++) {
            VariantParam described;
            const VariantParam *param = &described;
            if (!variant_operation_param(operation, p, &described)) {
                continue;
            }
            printf("  --%s  %s  %s\n", param->name, variant_param_kind_name(param->kind), param->description);
        }
        printf("  forms: ");
        print_forms(operation);
    }
}


/* --- the chain file ------------------------------------------------------ */

static bool whole_number(const cJSON *item, long *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble) {
        return false;
    }
    *out = (long)item->valuedouble;
    return true;
}

/* "n:m" with both whole numbers. */
static bool parse_ratio(const char *text, long *numerator, long *denominator) {
    char *end = NULL;
    *numerator = strtol(text, &end, 10);
    if (end == text || *end != ':') {
        return false;
    }
    const char *rest = end + 1;
    *denominator = strtol(rest, &end, 10);
    return end != rest && *end == '\0';
}

/* Sets one parameter of a step from its JSON value. Returns 0 or the status of the error it printed. */
static int set_param(VariantStep *step, size_t number, const char *path, const cJSON *item) {
    const VariantOperation *operation = variant_registry_find(step->op);
    int index = variant_operation_param_index(operation, item->string);
    if (index < 0) {
        return cli_fail(CLI_EXIT_USAGE, "%s: step %zu: %s has no parameter '%s'", path, number, step->op, item->string);
    }
    VariantParam described;
    if (!variant_operation_param(operation, (size_t)index, &described)) {
        return cli_fail(CLI_EXIT_USAGE, "%s: step %zu: %s has no parameter '%s'", path, number, step->op, item->string);
    }
    const VariantParam *param = &described;
    long value = 0;
    long den = 0;
    bool ok = false;
    switch (param->kind) {
    case VARIANT_PARAM_BOOL:
        ok = cJSON_IsBool(item);
        value = cJSON_IsTrue(item) ? 1 : 0;
        break;
    case VARIANT_PARAM_INT:
    case VARIANT_PARAM_PITCH:
        ok = whole_number(item, &value);
        break;
    case VARIANT_PARAM_RATIO:
        ok = cJSON_IsString(item) && parse_ratio(item->valuestring, &value, &den);
        break;
    case VARIANT_PARAM_POSITION:
        if (cJSON_IsString(item)) {
            ok = true;
            if (strcmp(item->valuestring, "leading") == 0) {
                value = 1;
            } else if (strcmp(item->valuestring, "trailing") == 0) {
                value = 2;
            } else if (strcmp(item->valuestring, "middle") == 0) {
                value = 3;
            } else {
                ok = false;
            }
        }
        break;
    }
    if (!ok) {
        return cli_fail(CLI_EXIT_USAGE, "%s: step %zu: '%s' of %s must be a %s", path, number, param->name, step->op,
                        param->kind == VARIANT_PARAM_BOOL       ? "true or false"
                        : param->kind == VARIANT_PARAM_RATIO    ? "ratio text like \"2:1\""
                        : param->kind == VARIANT_PARAM_POSITION ? "position: \"leading\", \"trailing\" or \"middle\""
                                                                : "whole number");
    }
    if (!variant_step_set(step, param->name, value, den)) {
        return cli_fail(CLI_EXIT_USAGE, "%s: step %zu: '%s' of %s is out of range (%ld to %ld)", path, number,
                        param->name, step->op, param->min, param->max);
    }
    return 0;
}

/* Reads {"steps": [{"op", "params"}]} into *chain. Returns 0 or the status of the error it printed. */
static int read_chain(const char *path, VariantChain *chain) {
    char *text;
    if (!util_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read chain file '%s'", path);
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        return cli_fail(CLI_EXIT_USAGE, "chain file '%s' is not JSON", path);
    }
    int status = 0;
    const cJSON *steps = cJSON_GetObjectItemCaseSensitive(root, "steps");
    if (!cJSON_IsArray(steps)) {
        status = cli_fail(CLI_EXIT_USAGE, "%s: expected {\"steps\": [...]}", path);
    } else if (cJSON_GetArraySize(steps) < 1 || cJSON_GetArraySize(steps) > VARIANT_CHAIN_MAX) {
        status = cli_fail(CLI_EXIT_USAGE, "%s: a chain has 1 to %d steps", path, VARIANT_CHAIN_MAX);
    }
    memset(chain, 0, sizeof(*chain));
    if (status != 0) {
        cJSON_Delete(root);
        return status;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, steps) {
        size_t number = chain->step_count + 1;
        const cJSON *op = cJSON_GetObjectItemCaseSensitive(item, "op");
        const cJSON *params = cJSON_GetObjectItemCaseSensitive(item, "params");
        VariantStep *step = &chain->steps[chain->step_count];
        if (!cJSON_IsString(op)) {
            status = cli_fail(CLI_EXIT_USAGE, "%s: step %zu needs an \"op\" name", path, number);
        } else if (!variant_step_init(step, op->valuestring)) {
            status = cli_fail(CLI_EXIT_USAGE, "%s: step %zu: unknown operation '%s'; --list-ops shows them", path,
                              number, op->valuestring);
        } else if (params && !cJSON_IsObject(params)) {
            status = cli_fail(CLI_EXIT_USAGE, "%s: step %zu: \"params\" must be an object", path, number);
        }
        if (status == 0 && params) {
            const cJSON *param = NULL;
            cJSON_ArrayForEach(param, params) {
                status = set_param(step, number, path, param);
                if (status != 0) {
                    break;
                }
            }
        }
        if (status != 0) {
            break;
        }
        chain->step_count++;
    }
    cJSON_Delete(root);
    return status;
}

/* --- the variants -------------------------------------------------------- */

typedef struct {
    VariantChain chain;
    Motive motive;
    VariantRecord record;
} Variant;

/* An operation --each-op could not apply to the motive, and why. */
typedef struct {
    const char *op;
    char reason[VARIANT_ERROR_MAX];
} Skipped;

static void print_text(unsigned long seed, const Motive *source, const Variant *variants, size_t count,
                       const Skipped *skipped, size_t skipped_count) {
    char *source_text = cli_motive_text(source);
    printf("Source: %s\n", source_text ? source_text : "");
    printf("Seed: %lu\n", seed);
    free(source_text);
    for (size_t i = 0; i < count; i++) {
        const Variant *variant = &variants[i];
        printf("\nVariant %zu (seed %lu):", i + 1, variant->record.seed);
        for (size_t s = 0; s < variant->record.step_count; s++) {
            printf("%s %s", s ? "," : "", variant->record.steps[s].op);
        }
        char *text = cli_motive_text(&variant->motive);
        printf("\n  %s\n  %s\n", text ? text : "", variant->record.derivation);
        if (variant->record.leaves_harmony) {
            printf("  Warning: %s\n", VARIANT_WARNING_LEAVES_HARMONY);
        }
        printf("  Remoteness: %llu/%llu\n", variant->record.remoteness_num, variant->record.remoteness_den);
        printf("  Functions:");
        for (size_t f = 0; f < variant->record.function_count; f++) {
            printf("%s %s", f ? "," : "", variant->record.functions[f]);
        }
        printf("%s\n", variant->record.function_count ? "" : " none");
        free(text);
    }
    if (skipped_count > 0) {
        printf("\n");
    }
    for (size_t i = 0; i < skipped_count; i++) {
        printf("Skipped %s: %s\n", skipped[i].op, skipped[i].reason);
    }
}

/* The motive and its analysis as the motive command's --json document, parsed. NULL on failure. */
static cJSON *motive_document(const Motive *motive) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis)) {
        return NULL;
    }
    char *text = motive_analysis_to_json(motive, &analysis);
    cJSON *document = text ? cJSON_Parse(text) : NULL;
    free(text);
    return document;
}

/* A variant: its record as variant_record_to_json writes it, then "motive". NULL on failure. */
static cJSON *variant_object(const Variant *variant) {
    char *text = variant_record_to_json(&variant->record);
    cJSON *object = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *motive = object ? motive_document(&variant->motive) : NULL;
    if (!motive) {
        cJSON_Delete(object);
        return NULL;
    }
    /* A variant that leaves its harmony is kept and marked: the warning joins the analysis's own. */
    cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motive, "analysis");
    cJSON *warnings = cJSON_GetObjectItemCaseSensitive(analysis, "warnings");
    if (variant->record.leaves_harmony &&
        (!cJSON_IsArray(warnings) ||
         !cJSON_AddItemToArray(warnings, cJSON_CreateString(VARIANT_WARNING_LEAVES_HARMONY)))) {
        cJSON_Delete(motive);
        cJSON_Delete(object);
        return NULL;
    }
    cJSON_AddItemToObject(object, "motive", motive);
    return object;
}

/* {"seed", "source", "variants"} and, for --each-op, "skipped", printed with 2-space indent. False on failure. */
static bool print_json(unsigned long seed, const Motive *source, const Variant *variants, size_t count,
                       const Skipped *skipped, size_t skipped_count, bool each_op) {
    cJSON *document = cJSON_CreateObject();
    cJSON *source_document = document ? motive_document(source) : NULL;
    cJSON *array = source_document ? cJSON_CreateArray() : NULL;
    if (!array) {
        cJSON_Delete(source_document);
        cJSON_Delete(document);
        return false;
    }
    cJSON_AddNumberToObject(document, "seed", (double)seed);
    cJSON_AddItemToObject(document, "source", source_document);
    cJSON_AddItemToObject(document, "variants", array);
    for (size_t i = 0; i < count; i++) {
        cJSON *object = variant_object(&variants[i]);
        if (!object) {
            cJSON_Delete(document);
            return false;
        }
        cJSON_AddItemToArray(array, object);
    }
    cJSON *skipped_array = each_op ? cJSON_AddArrayToObject(document, "skipped") : NULL;
    for (size_t i = 0; each_op && i < skipped_count; i++) {
        cJSON *entry = skipped_array ? cJSON_CreateObject() : NULL;
        if (!entry || !cJSON_AddStringToObject(entry, "op", skipped[i].op) ||
            !cJSON_AddStringToObject(entry, "reason", skipped[i].reason)) {
            cJSON_Delete(entry);
            cJSON_Delete(document);
            return false;
        }
        cJSON_AddItemToArray(skipped_array, entry);
    }
    if (each_op && !skipped_array) {
        cJSON_Delete(document);
        return false;
    }
    char *printed = cJSON_Print(document);
    cJSON_Delete(document);
    if (!printed) {
        return false;
    }
    /* cJSON indents with tabs and puts one after each colon; the other commands write 2 spaces and one space. */
    for (const char *c = printed; *c; c++) {
        if (*c == '\n') {
            putchar('\n');
            size_t tabs = 0;
            while (c[1] == '\t') {
                tabs++;
                c++;
            }
            printf("%*s", (int)(tabs * 2), "");
        } else {
            putchar(*c == '\t' ? ' ' : *c);
        }
    }
    putchar('\n');
    cJSON_free(printed);
    return true;
}

typedef struct {
    size_t min_steps;
    size_t max_steps;
    /* The remoteness band as fractions: over low_num/low_den (0 for none) and at most high_num/high_den. */
    unsigned long long low_num, low_den, high_num, high_den;
} Distance;

static const Distance DISTANCE_CLOSE = {1, 1, 0, 1, 1, 3};
static const Distance DISTANCE_MEDIUM = {2, 2, 1, 3, 2, 3};
static const Distance DISTANCE_FAR = {3, VARIANT_CHAIN_MAX, 2, 3, 1, 1};

/* Whether the record's remoteness is inside the band. */
static bool in_distance(const Distance *distance, const VariantRecord *record) {
    unsigned long long num = record->remoteness_num;
    unsigned long long den = record->remoteness_den;
    return num * distance->low_den > distance->low_num * den && num * distance->high_den <= distance->high_num * den;
}

/* Whether the variant suits any function --function named. */
static bool suits_a_function(const Options *options, const VariantRecord *record) {
    for (size_t f = 0; f < record->function_count; f++) {
        for (size_t w = 0; w < options->function_count; w++) {
            if (strcmp(record->functions[f], options->function_texts[w]) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* Sets the variant's remoteness from the basic motive and the functions it suits. */
static void measure_variant(const Options *options, const Motive *basic, Variant *variant) {
    /* From the basic motive, searched as deep as the chain that made the variant unless the basic is another motive. */
    size_t depth = motive_input_basic_given(&options->basic) ? VARIANT_CHAIN_MAX : variant->record.step_count;
    if (variant_set_remoteness(basic, &variant->motive, depth, &variant->record) != VARIANT_OK) {
        variant->record.remoteness_num = 1;
        variant->record.remoteness_den = 1;
    }
    variant_functions(basic, &variant->motive, &variant->record, variant->record.functions,
                      &variant->record.function_count);
}

/* --each-op: one one-step variant for each operation that takes the motive's form. */
static int make_each_op(const Options *options, unsigned long seed, const Motive *source, const Motive *basic) {
    size_t rows = variant_registry_count();
    Variant *variants = calloc(rows, sizeof(*variants));
    Skipped *skipped = calloc(rows, sizeof(*skipped));
    if (!variants || !skipped) {
        free(variants);
        free(skipped);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    size_t made = 0;
    size_t skipped_count = 0;
    for (size_t i = 0; i < rows; i++) {
        const VariantOperation *operation = variant_registry_at(i);
        if ((operation->forms & VARIANT_FORM_BIT(source->form)) == 0) {
            continue;
        }
        Variant *variant = &variants[made];
        variant_step_init(&variant->chain.steps[0], operation->name);
        variant->chain.step_count = 1;
        VariantResult result =
            variant_apply_chain(source, &variant->chain, seed + (unsigned long)i, &variant->motive, &variant->record);
        if (result == VARIANT_OK) {
            measure_variant(options, basic, variant);
            made++;
        } else {
            skipped[skipped_count].op = operation->name;
            /* The error often starts with the operation's name; the entry has that already. */
            const char *reason = variant->record.error[0] ? variant->record.error : variant_result_name(result);
            size_t name_length = strlen(operation->name);
            if (strncmp(reason, operation->name, name_length) == 0 && reason[name_length] == ':' &&
                reason[name_length + 1] == ' ') {
                reason += name_length + 2;
            }
            snprintf(skipped[skipped_count].reason, sizeof(skipped[skipped_count].reason), "%s", reason);
            skipped_count++;
        }
    }
    int status = 0;
    if (made == 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "no operation could be applied to this motive");
    } else if (options->json) {
        if (!print_json(seed, source, variants, made, skipped, skipped_count, true)) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        print_text(seed, source, variants, made, skipped, skipped_count);
    }
    free(variants);
    free(skipped);
    return status;
}

/* The variants command proper: load the source, make --count variants, print them. */
static int make_variants(const Options *options) {
    unsigned long count = VARIANTS_COUNT_DEFAULT;
    if (options->count_text && (!request_parse_whole_number(options->count_text, VARIANTS_COUNT_MAX, &count) || count < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'", VARIANTS_COUNT_MAX,
                        options->count_text);
    }
    unsigned long seed = cli_default_seed();
    unsigned long seed_max = CLI_SEED_MAX - VARIANTS_COUNT_MAX * VARIANTS_DRAWS_PER_COUNT;
    if (options->seed_text && !request_parse_whole_number(options->seed_text, seed_max, &seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to %lu, not '%s'", seed_max,
                        options->seed_text);
    }
    if (options->each_op && (options->chain_path || options->count_text || options->distance_text ||
                             options->function_count > 0)) {
        return cli_fail(CLI_EXIT_USAGE, "--each-op takes no --chain, --count, --distance or --function");
    }
    const Distance *distance = NULL;
    if (options->distance_text) {
        if (options->chain_path) {
            return cli_fail(CLI_EXIT_USAGE, "--distance takes random chains, not --chain");
        }
        distance = strcmp(options->distance_text, "close") == 0    ? &DISTANCE_CLOSE
                   : strcmp(options->distance_text, "medium") == 0 ? &DISTANCE_MEDIUM
                   : strcmp(options->distance_text, "far") == 0    ? &DISTANCE_FAR
                                                                   : NULL;
        if (!distance) {
            return cli_fail(CLI_EXIT_USAGE, "--distance must be close, medium or far, not '%s'", options->distance_text);
        }
    }
    VariantChain file_chain;
    if (options->chain_path) {
        int status = read_chain(options->chain_path, &file_chain);
        if (status != 0) {
            return status;
        }
    }
    Motive source;
    int status = motive_input_load(&options->in, &source);
    if (status != 0) {
        return status;
    }
    Motive basic = source;
    if (motive_input_basic_given(&options->basic)) {
        status = motive_input_load_basic(&options->basic, &options->in, &basic);
        if (status != 0) {
            return status;
        }
    }

    if (options->each_op) {
        return make_each_op(options, seed, &source, &basic);
    }

    Variant *variants = calloc(count, sizeof(*variants));
    if (!variants) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    size_t made = 0;
    bool filtered = options->function_count > 0 || distance;
    const char *asked = options->function_count > 0 && distance ? "function and distance"
                        : distance                              ? "distance"
                                                                : "function";
    unsigned long draws = filtered ? count * VARIANTS_DRAWS_PER_COUNT : count;
    for (unsigned long i = 0; i < draws && made < count; i++) {
        Variant *variant = &variants[made];
        unsigned long variant_seed = seed + i;
        if (options->chain_path) {
            variant->chain = file_chain;
        } else if (!(distance ? variant_random_chain_of(&source, variant_seed, distance->min_steps, distance->max_steps,
                                                        &variant->chain)
                              : variant_random_chain(&source, variant_seed, &variant->chain))) {
            free(variants);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot draw a chain for this motive");
        }
        VariantResult result = variant_apply_chain(&source, &variant->chain, variant_seed, &variant->motive, &variant->record);
        if (result == VARIANT_OK) {
            measure_variant(options, &basic, variant);
            if ((options->function_count > 0 && !suits_a_function(options, &variant->record)) ||
                (distance && !in_distance(distance, &variant->record))) {
                continue;
            }
            made++;
        } else if (options->chain_path) {
            int failed = cli_fail(CLI_EXIT_UNSATISFIABLE, "step %zu of the chain: %s", variant->record.failed_step + 1,
                                  variant->record.error[0] ? variant->record.error : variant_result_name(result));
            free(variants);
            return failed;
        } else {
            fprintf(stderr, "Skipped seed %lu: %s\n", variant_seed,
                    variant->record.error[0] ? variant->record.error : variant_result_name(result));
        }
    }
    if (made == 0) {
        free(variants);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, filtered
                                                    ? "no variant that suits the %s was found in %lu draws"
                                                    : "no variant could be made of this motive",
                        asked, draws);
    }
    if (filtered && made < count) {
        fprintf(stderr, "Only %zu of %lu variants suit the %s within %lu draws\n", made, count, asked, draws);
    }
    if (options->json) {
        if (!print_json(seed, &source, variants, made, NULL, 0, false)) {
            free(variants);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        print_text(seed, &source, variants, made, NULL, 0);
    }
    free(variants);
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
    if (options.json) {
        char *json = variant_registry_to_json();
        if (!json) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        fputs(json, stdout);
        free(json);
    } else {
        print_operations();
    }
    return 0;
}
