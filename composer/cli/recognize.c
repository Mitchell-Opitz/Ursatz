#include "recognize.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/variant_registry.h>

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
    fprintf(out, "  --depth <n>          the most operations in the chain, 1 to %d (default %d)\n", VARIANT_CHAIN_MAX, VARIANT_CHAIN_MAX);
    fprintf(out, "  --json               print {found, chain, distance, remoteness, kept, changed, derivation}\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 the search ran, found or not; 1 usage, an invalid argument or an input that cannot be loaded.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer recognize theme.txt variant.txt --meter 4/4 --key \"D minor\" --json\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = 0;
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (motive_extras_take(&options->source.extras, argc, argv, &i, false, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (motive_input_take_basic(&options->basic, argc, argv, &i, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(argument, "--target-feature") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "--target-feature needs a value");
            }
            if (options->target.extras.feature_count == MOTIVE_MAX_FEATURES) {
                return cli_fail(CLI_EXIT_USAGE, "a motive has at most %d features", MOTIVE_MAX_FEATURES);
            }
            options->target.extras.feature_texts[options->target.extras.feature_count++] = argv[++i];
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--meter") == 0 || strcmp(argument, "--key") == 0 ||
                   strcmp(argument, "--format") == 0 || strcmp(argument, "--form") == 0 ||
                   strcmp(argument, "--pickup") == 0 || strcmp(argument, "--target-format") == 0 ||
                   strcmp(argument, "--target-pickup") == 0 || strcmp(argument, "--target-harmony") == 0 ||
                   strcmp(argument, "--depth") == 0) {
            const char **slot = strcmp(argument, "--meter") == 0            ? &options->source.meter_text
                                : strcmp(argument, "--key") == 0            ? &options->source.key_text
                                : strcmp(argument, "--format") == 0         ? &options->source.format_text
                                : strcmp(argument, "--form") == 0           ? &options->source.form_text
                                : strcmp(argument, "--pickup") == 0         ? &options->source.pickup_text
                                : strcmp(argument, "--target-format") == 0  ? &options->target_format_text
                                : strcmp(argument, "--target-pickup") == 0  ? &options->target_pickup_text
                                : strcmp(argument, "--target-harmony") == 0 ? &options->target_harmony_text
                                                                            : &options->depth_text;
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
            }
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
            }
            *slot = argv[++i];
        } else if (argument[0] == '-' && argument[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s'", argument);
        } else if (!options->have_source) {
            options->source.path = argument;
            options->have_source = true;
        } else if (!options->have_target) {
            options->target.path = argument;
            options->have_target = true;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; recognize takes a source and a target", argument);
        }
    }
    return 0;
}

/* The chain as the variants JSON writes it: {"steps": [{"op", "params"}]}, from a replay of the chain. NULL on failure. */
static cJSON *chain_object(const Motive *source, const VariantChain *chain) {
    Motive replay;
    VariantRecord record;
    if (variant_apply_chain(source, chain, 0, &replay, &record) != VARIANT_OK) {
        return NULL;
    }
    char *text = variant_record_to_json(&record);
    cJSON *document = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *steps = document ? cJSON_DetachItemFromObjectCaseSensitive(
                                  cJSON_GetObjectItemCaseSensitive(document, "chain"), "steps")
                            : NULL;
    cJSON_Delete(document);
    if (!steps) {
        return NULL;
    }
    cJSON *step = NULL;
    cJSON_ArrayForEach(step, steps) {
        cJSON_DeleteItemFromObjectCaseSensitive(step, "open");
    }
    cJSON *chain_json = cJSON_CreateObject();
    if (!chain_json) {
        cJSON_Delete(steps);
        return NULL;
    }
    cJSON_AddItemToObject(chain_json, "steps", steps);
    return chain_json;
}

static double distance_value(const VariantRecognition *recognition) {
    return (double)recognition->distance_num / (double)recognition->distance_den;
}

static cJSON *names_array(const char *const *names, size_t count) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateString(names[i]));
    }
    return array;
}

/* Prints cJSON's output with 2-space indent and one space after each colon, as the other commands do. */
static void print_pretty(const char *printed) {
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
}

static double fraction_value(unsigned long long numerator, unsigned long long denominator) {
    return (double)numerator / (double)denominator;
}

static bool print_json(const Motive *source, const VariantChain *chain, const VariantRecognition *recognition,
                       const VariantRecord *remoteness) {
    cJSON *document = cJSON_CreateObject();
    if (!document) {
        return false;
    }
    cJSON *chain_json = recognition->found ? chain_object(source, chain) : NULL;
    if (!chain_json) {
        chain_json = cJSON_CreateObject();
        if (chain_json) {
            cJSON_AddItemToObject(chain_json, "steps", cJSON_CreateArray());
        }
    }
    cJSON_AddBoolToObject(document, "found", recognition->found);
    cJSON_AddItemToObject(document, "chain", chain_json);
    /* Four decimals, as the record's recognizability. */
    cJSON_AddNumberToObject(document, "distance", (double)(long)(distance_value(recognition) * 10000.0 + 0.5) / 10000.0);
    cJSON_AddNumberToObject(document, "remoteness",
                            (double)(long)(fraction_value(remoteness->remoteness_num, remoteness->remoteness_den) * 10000.0 +
                                           0.5) / 10000.0);
    cJSON_AddItemToObject(document, "kept", names_array(recognition->kept, recognition->kept_count));
    cJSON_AddItemToObject(document, "changed", names_array(recognition->changed, recognition->changed_count));
    cJSON_AddStringToObject(document, "derivation", recognition->derivation);
    char *printed = cJSON_Print(document);
    cJSON_Delete(document);
    if (!printed) {
        return false;
    }
    print_pretty(printed);
    cJSON_free(printed);
    return true;
}

static void print_text(const VariantChain *chain, const VariantRecognition *recognition, const VariantRecord *remoteness) {
    if (recognition->found) {
        printf("Chain:");
        for (size_t s = 0; s < chain->step_count; s++) {
            printf("%s %s", s ? "," : "", chain->steps[s].op);
        }
        printf("\n");
    } else {
        printf("No chain found\n");
    }
    printf("Distance: %llu/%llu\n", recognition->distance_num, recognition->distance_den);
    printf("Remoteness: %llu/%llu\n", remoteness->remoteness_num, remoteness->remoteness_den);
    printf("%s\n", recognition->derivation);
}

int run_recognize(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
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
    unsigned long depth = VARIANT_CHAIN_MAX;
    if (options.depth_text &&
        (!cli_parse_whole_number(options.depth_text, VARIANT_CHAIN_MAX, &depth) || depth < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--depth must be a whole number from 1 to %d, not '%s'", VARIANT_CHAIN_MAX,
                        options.depth_text);
    }

    /* The target is read as the source is: its form, meter, key and role; its own format, pickup and chords. */
    options.target.meter_text = options.source.meter_text;
    options.target.key_text = options.source.key_text;
    options.target.form_text = options.source.form_text;
    options.target.format_text = options.target_format_text;
    options.target.pickup_text = options.target_pickup_text;
    options.target.extras.harmony_text = options.target_harmony_text;
    options.target.extras.role_text = options.source.extras.role_text;

    Motive source;
    Motive target;
    if (motive_input_load(&options.source, &source) != 0 || motive_input_load(&options.target, &target) != 0) {
        return CLI_EXIT_USAGE;
    }
    VariantChain chain;
    VariantRecognition recognition;
    VariantResult result = variant_recognize_depth(&source, &target, depth, &chain, &recognition);
    if (result != VARIANT_OK) {
        return cli_fail(CLI_EXIT_USAGE, "cannot recognize: %s", variant_result_name(result));
    }
    /* Remoteness is the distance from the basic motive; with no --basic the source is the basic motive. */
    VariantRecord remoteness;
    memset(&remoteness, 0, sizeof(remoteness));
    if (motive_input_basic_given(&options.basic)) {
        Motive basic;
        if (motive_input_load_basic(&options.basic, &options.source, &basic) != 0) {
            return CLI_EXIT_USAGE;
        }
        result = variant_set_remoteness(&basic, &target, depth, &remoteness);
        if (result != VARIANT_OK) {
            return cli_fail(CLI_EXIT_USAGE, "cannot recognize: %s", variant_result_name(result));
        }
    } else {
        remoteness.remoteness_num = recognition.distance_num;
        remoteness.remoteness_den = recognition.distance_den;
    }
    if (options.json) {
        if (!print_json(&source, &chain, &recognition, &remoteness)) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        print_text(&chain, &recognition, &remoteness);
    }
    return 0;
}
