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

#define VARIANTS_COUNT_MAX 16
#define VARIANTS_COUNT_DEFAULT 6

typedef struct {
    const char *input;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
    const char *form_text;
    const char *pickup_text;
    const char *chain_path;
    const char *count_text;
    const char *seed_text;
    MotiveExtras extras;
    bool list_ops;
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
    fprintf(out, "  --chain <file>       a JSON file {\"steps\": [{\"op\": \"invert\", \"params\": {\"tonal\": true}}]}, the same\n");
    fprintf(out, "                       chain for every variant. Default: each variant gets its own random chain\n");
    fprintf(out, "  --count <n>          how many variants, 1 to %d (default %d); variant i uses seed + i\n", VARIANTS_COUNT_MAX, VARIANTS_COUNT_DEFAULT);
    fprintf(out, "  --seed <n>           whole number from 0. Default: from the clock. The seed is printed\n");
    fprintf(out, "  --json               print as JSON: the seed, the source, and for each variant its seed, chain, kind, kept,\n");
    fprintf(out, "                       changed, derivation, recognizability and motive (with its analysis)\n");
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
        } else if (motive_extras_take(&options->extras, argc, argv, &i, false, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--list-ops") == 0) {
            options->list_ops = true;
        } else if (strcmp(argument, "--meter") == 0 || strcmp(argument, "--key") == 0 ||
                   strcmp(argument, "--format") == 0 || strcmp(argument, "--form") == 0 ||
                   strcmp(argument, "--pickup") == 0 || strcmp(argument, "--chain") == 0 ||
                   strcmp(argument, "--count") == 0 || strcmp(argument, "--seed") == 0) {
            const char **slot = strcmp(argument, "--meter") == 0    ? &options->meter_text
                                : strcmp(argument, "--key") == 0    ? &options->key_text
                                : strcmp(argument, "--format") == 0 ? &options->format_text
                                : strcmp(argument, "--form") == 0   ? &options->form_text
                                : strcmp(argument, "--pickup") == 0 ? &options->pickup_text
                                : strcmp(argument, "--chain") == 0  ? &options->chain_path
                                : strcmp(argument, "--count") == 0  ? &options->count_text
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
        } else if (!options->input) {
            options->input = argument;
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
            const VariantParam *param = &operation->params[p];
            printf("  --%s  %s  %s\n", param->name, variant_param_kind_name(param->kind), param->description);
        }
        printf("  forms: ");
        print_forms(operation);
    }
}


/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
static bool parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

/* Loads the source motive as the motive command does. Returns 0 or the status of the error it printed. */
static int load_source(const Options *options, Motive *out) {
    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    if (options->meter_text) {
        if (!motive_parse_meter(options->meter_text, &motive_options.meter)) {
            return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8",
                            options->meter_text);
        }
        motive_options.has_meter = true;
    }
    if (options->key_text) {
        if (!motive_parse_key(options->key_text, &motive_options.key)) {
            return cli_fail(CLI_EXIT_USAGE,
                            "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                            "for example \"D minor\"",
                            options->key_text);
        }
        motive_options.has_key = true;
    }
    MotiveFormat format = MOTIVE_FORMAT_AUTO;
    if (options->format_text && !motive_format_parse(options->format_text, &format)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "--format must be auto, midi, musicxml, notes, degrees, rhythm, contour or json, not '%s'",
                        options->format_text);
    }
    if (options->form_text) {
        MotiveForm form;
        if (!motive_form_parse(options->form_text, &form)) {
            return cli_fail(CLI_EXIT_USAGE, "--form must be full, rhythm or contour, not '%s'", options->form_text);
        }
        MotiveFormat wanted = form == MOTIVE_FORM_RHYTHM ? MOTIVE_FORMAT_RHYTHM : MOTIVE_FORMAT_CONTOUR;
        if (form != MOTIVE_FORM_FULL && format != MOTIVE_FORMAT_AUTO && format != wanted) {
            return cli_fail(CLI_EXIT_USAGE, "--form %s reads a %s, which --format %s does not", options->form_text,
                            options->form_text, options->format_text);
        }
        if (form == MOTIVE_FORM_FULL && (format == MOTIVE_FORMAT_RHYTHM || format == MOTIVE_FORMAT_CONTOUR)) {
            return cli_fail(CLI_EXIT_USAGE, "--form full does not read a %s; use --form %s or another --format",
                            options->format_text, options->format_text);
        }
        if (form != MOTIVE_FORM_FULL) {
            format = wanted;
        }
    }

    MotiveError error;
    MotiveResult result = motive_load_file(options->input, format, &motive_options, out, &error);
    if (result == MOTIVE_OK && (options->pickup_text || motive_extras_given(&options->extras))) {
        /* The pickup and the chord onsets are in beats of the motive's meter, which a file may declare, so they are
           read after the first load. */
        if (options->pickup_text) {
            PlanTime pickup;
            if (!parse_pickup(options->pickup_text, out->meter.beat_unit, &pickup)) {
                return cli_fail(CLI_EXIT_USAGE, "pickup '%s' is not a number of beats; write 0, 1 or 1/2",
                                options->pickup_text);
            }
            motive_options.has_pickup = true;
            motive_options.pickup = pickup;
        }
        int status = motive_extras_options(&options->extras, &out->meter, &motive_options);
        if (status != 0) {
            return status;
        }
        result = motive_load_file(options->input, format, &motive_options, out, &error);
    }
    if (result != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options->input,
                        motive_result_name(result), error.location[0] ? " at " : "", error.location, error.message);
    }
    return 0;
}

/* --- the chain file ------------------------------------------------------ */

static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    if (fseek(file, 0, SEEK_END) == 0) {
        long length = ftell(file);
        if (length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
            text = malloc((size_t)length + 1);
            if (text && fread(text, 1, (size_t)length, file) == (size_t)length) {
                text[length] = '\0';
            } else {
                free(text);
                text = NULL;
            }
        }
    }
    fclose(file);
    return text;
}

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
    const VariantParam *param = &operation->params[index];
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
    char *text = read_whole_file(path);
    if (!text) {
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

/* The motive as text in its own form: a note list, a rhythm or a contour. Caller frees. */
static char *motive_text(const Motive *motive) {
    return motive->form == MOTIVE_FORM_RHYTHM    ? motive_to_rhythm_text(motive)
           : motive->form == MOTIVE_FORM_CONTOUR ? motive_to_contour_text(motive)
                                                 : motive_to_notes_text(motive);
}

static void print_text(unsigned long seed, const Motive *source, const Variant *variants, size_t count) {
    char *source_text = motive_text(source);
    printf("Source: %s\n", source_text ? source_text : "");
    printf("Seed: %lu\n", seed);
    free(source_text);
    for (size_t i = 0; i < count; i++) {
        const Variant *variant = &variants[i];
        printf("\nVariant %zu (seed %lu):", i + 1, variant->record.seed);
        for (size_t s = 0; s < variant->record.step_count; s++) {
            printf("%s %s", s ? "," : "", variant->record.steps[s].op);
        }
        char *text = motive_text(&variant->motive);
        printf("\n  %s\n  %s\n", text ? text : "", variant->record.derivation);
        free(text);
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
    cJSON_AddItemToObject(object, "motive", motive);
    return object;
}

/* {"seed", "source", "variants"}, printed with 2-space indent. False on failure. */
static bool print_json(unsigned long seed, const Motive *source, const Variant *variants, size_t count) {
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

/* The variants command proper: load the source, make --count variants, print them. */
static int make_variants(const Options *options) {
    unsigned long count = VARIANTS_COUNT_DEFAULT;
    if (options->count_text && (!cli_parse_whole_number(options->count_text, VARIANTS_COUNT_MAX, &count) || count < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'", VARIANTS_COUNT_MAX,
                        options->count_text);
    }
    unsigned long seed = cli_default_seed();
    if (options->seed_text && !cli_parse_whole_number(options->seed_text, CLI_SEED_MAX - VARIANTS_COUNT_MAX, &seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to %lu, not '%s'",
                        CLI_SEED_MAX - VARIANTS_COUNT_MAX, options->seed_text);
    }
    VariantChain file_chain;
    if (options->chain_path) {
        int status = read_chain(options->chain_path, &file_chain);
        if (status != 0) {
            return status;
        }
    }
    Motive source;
    int status = load_source(options, &source);
    if (status != 0) {
        return status;
    }

    Variant *variants = calloc(count, sizeof(*variants));
    if (!variants) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    size_t made = 0;
    for (unsigned long i = 0; i < count; i++) {
        Variant *variant = &variants[made];
        unsigned long variant_seed = seed + i;
        if (options->chain_path) {
            variant->chain = file_chain;
        } else if (!variant_random_chain(&source, variant_seed, &variant->chain)) {
            free(variants);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot draw a chain for this motive");
        }
        VariantResult result = variant_apply_chain(&source, &variant->chain, variant_seed, &variant->motive, &variant->record);
        if (result == VARIANT_OK) {
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
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "no variant could be made of this motive");
    }
    if (options->json) {
        if (!print_json(seed, &source, variants, made)) {
            free(variants);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        print_text(seed, &source, variants, made);
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
    if (!options.list_ops && !options.input) {
        return cli_fail(CLI_EXIT_USAGE, "variants needs --list-ops or a motive input\n%s", VARIANTS_USAGE_LINE);
    }
    if (options.list_ops && options.input) {
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
