#include "plan_cmd.h"
#include "cli_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <stdint.h>

#include "cli_options.h"
#include "workflow.h"

#define MAX_GIVEN 8

typedef enum {
    GIVEN_MELODY,
    GIVEN_PROGRESSION,
    GIVEN_BASS
} GivenKind;

/* One --given-* option and the --at and --cells that follow it. */
typedef struct {
    GivenKind kind;
    const char *source; /* a file, or the numerals of a progression */
    bool from_corpus;   /* --like-progression */
    const char *at_text;
    const char *cells_text;
    cJSON *passages; /* --like-progression: the passages like its numerals, found by the Analyzer */
} GivenOption;

typedef struct {
    const char *form_text;
    const char *key_text;
    const char *material_text;
    const char *meter_text;
    const char *bars_text;
    const char *count_text;
    bool modulate;
    const char *motive_path;
    const char *from_corpus;
    const char *like_path;
    cJSON *passages; /* the passages like --like, found by the Analyzer */
    const char *accompaniment_text;
    const char *texture_text;
    const char *seed_text;
    const char *style_path;
    const char *style_weight_text;
    const char *blend_path;
    CliStyleWithout without;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *out_path;
    const char *format_text;
    GivenOption given[MAX_GIVEN];
    size_t given_count;
    bool json;
} Options;


/* --- reading the options -------------------------------------------------- */

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->form_text = cli_value(args, "--form");
    options->key_text = cli_value(args, "--key");
    options->material_text = cli_value(args, "--material");
    options->meter_text = cli_value(args, "--meter");
    options->bars_text = cli_value(args, "--bars");
    options->count_text = cli_value(args, "--count");
    options->modulate = cli_switch(args, "--modulate");
    options->motive_path = cli_value(args, "--motive");
    options->from_corpus = cli_value(args, "--from-corpus");
    options->like_path = cli_value(args, "--like");
    options->accompaniment_text = cli_value(args, "--accompaniment");
    options->texture_text = cli_value(args, "--texture");
    options->seed_text = cli_value(args, "--seed");
    options->style_path = cli_value(args, "--style");
    options->style_weight_text = cli_value(args, "--style-weight");
    options->blend_path = cli_value(args, "--blend");
    options->blend_weight_text = cli_value(args, "--blend-weight");
    options->blend_weights_text = cli_value(args, "--blend-weights");
    options->out_path = cli_value(args, "--out");
    options->format_text = cli_value(args, "--format");
    int added = cli_style_without_read(&options->without, args);
    if (added != 0) {
        return added;
    }
    /* The given material in argv order (the repeated flags keep it); --at and --cells go to the --given-* before them. */
    for (size_t k = 0; k < args->repeat_count; k++) {
        const CliRepeat *repeat = &args->repeats[k];
        const char *name = CLI_FLAGS[repeat->flag].name;
        if (strcmp(name, "--given-melody") == 0 || strcmp(name, "--given-progression") == 0 || strcmp(name, "--given-bass") == 0 ||
            strcmp(name, "--like-progression") == 0) {
            if (options->given_count == MAX_GIVEN) {
                return cli_fail(CLI_EXIT_USAGE, "plan takes %d given phrases, progressions and bass lines at most", MAX_GIVEN);
            }
            GivenOption *given = &options->given[options->given_count++];
            given->from_corpus = strcmp(name, "--like-progression") == 0;
            given->kind = given->from_corpus                          ? GIVEN_PROGRESSION
                          : strcmp(name, "--given-melody") == 0       ? GIVEN_MELODY
                          : strcmp(name, "--given-progression") == 0  ? GIVEN_PROGRESSION
                                                                      : GIVEN_BASS;
            given->source = repeat->value;
        } else if (strcmp(name, "--at") == 0 || strcmp(name, "--cells") == 0) {
            if (options->given_count == 0) {
                return cli_fail(CLI_EXIT_USAGE, "%s goes after a --given-melody, --given-progression or --given-bass", name);
            }
            GivenOption *given = &options->given[options->given_count - 1];
            const char **slot = strcmp(name, "--at") == 0 ? &given->at_text : &given->cells_text;
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice for the same --given-*", name);
            }
            *slot = repeat->value;
        }
    }
    return 0;
}

static int check_options(Options *options) {
    if ((!options->form_text && options->given_count == 0) || !options->key_text || !options->meter_text || !options->out_path) {
        return cli_fail(CLI_EXIT_USAGE, "plan needs --form, --key, --meter and --out (see --help)\n%s", cli_usage_of("plan"));
    }
    bool likes_progression = false;
    for (size_t i = 0; i < options->given_count; i++) {
        likes_progression = likes_progression || options->given[i].from_corpus;
    }
    if ((options->like_path || likes_progression) && !options->from_corpus) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --from-corpus <dir>", options->like_path ? "--like" : "--like-progression");
    }
    if (options->from_corpus && !options->like_path && !likes_progression) {
        return cli_fail(CLI_EXIT_USAGE, "--from-corpus needs --like <file> or --like-progression \"<numerals>\"");
    }
    if (options->like_path && options->motive_path) {
        return cli_fail(CLI_EXIT_USAGE, "--like and --motive both give the motive; use one");
    }
    if (options->format_text) {
        if (cli_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!cli_equals_ignore_case(options->format_text, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    return 0;
}

/* --- the request ------------------------------------------------------------------ */

/* Why a file could not be turned into a motive request: the options are checked first, so it is held back. */
typedef struct {
    bool failed;
    int status;
    char message[1100];
} Held;

static bool is_midi(const char *bytes, size_t size) {
    return size >= 4 && memcmp(bytes, "MThd", 4) == 0;
}

/*
 * The motive request of a file: {"input": {"format", "text"}, "source": <the file>}. A MIDI file is read through
 * the composer tool's motive_from_midi and arrives as the motive it made. *held says why a file could not be read.
 */
static cJSON *motive_request(const Options *options, const char *path, Held *held) {
    char *bytes = NULL;
    size_t size = 0;
    if (!cli_read_file(path, &bytes, &size)) {
        held->failed = true;
        held->status = CLI_EXIT_UNSATISFIABLE;
        snprintf(held->message, sizeof held->message, "cannot load '%s' (io) at %s: cannot open the file", path, path);
        bytes = NULL;
        size = 0;
    }
    const char *text = bytes ? bytes : "";
    char *motive_text = NULL;
    if (bytes && is_midi(bytes, size)) {
        cJSON *midi_request = cJSON_CreateObject();
        char key[96];
        bool minor_key = strchr(options->key_text, ' ') != NULL;
        snprintf(key, sizeof key, minor_key ? "%s" : "%s %s", options->key_text, options->material_text ? options->material_text : "major");
        bool ok = midi_request && cJSON_AddStringToObject(midi_request, "meter", options->meter_text) &&
                  cJSON_AddStringToObject(midi_request, "key", key);
        CliResponse response;
        WorkshopBytes file = {(const uint8_t *)bytes, size, NULL};
        UrsatzStatus status = ok ? cli_composer("motive_from_midi", midi_request, &file, &response) : URSATZ_ERR_ALLOC;
        cJSON_Delete(midi_request);
        if (status != URSATZ_OK) {
            held->failed = true;
            held->status = status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE;
            snprintf(held->message, sizeof held->message, "cannot load '%s': %s", path, response.error.message);
            cli_response_destroy(&response);
            text = "";
        } else {
            cJSON *document = cJSON_Parse(response.json);
            motive_text = document ? cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "motive")) : NULL;
            cJSON_Delete(document);
            cli_response_destroy(&response);
            text = motive_text;
        }
    }
    cJSON *request = cJSON_CreateObject();
    cJSON *input = cJSON_CreateObject();
    bool ok = request && input && text && cJSON_AddStringToObject(input, "format", motive_text ? "json" : "auto") &&
              cJSON_AddStringToObject(input, "text", text) && cJSON_AddItemToObject(request, "input", input) &&
              cJSON_AddStringToObject(request, "source", path);
    free(bytes);
    cJSON_free(motive_text);
    if (!ok) {
        cJSON_Delete(input && !cJSON_GetObjectItem(request, "input") ? input : NULL);
        cJSON_Delete(request);
        held->failed = true;
        held->status = CLI_EXIT_UNSATISFIABLE;
        snprintf(held->message, sizeof held->message, "out of memory");
        return NULL;
    }
    return request;
}

static bool add_text(cJSON *request, const char *name, const char *text) {
    return !text || cJSON_AddStringToObject(request, name, text);
}

static cJSON *build_request(const Options *options, Held *held) {
    cJSON *request = cJSON_CreateObject();
    cJSON *given = cJSON_CreateArray();
    cJSON *without = cJSON_CreateArray();
    bool ok = request && given && without && add_text(request, "form", options->form_text) &&
              add_text(request, "key", options->key_text) && add_text(request, "material", options->material_text) &&
              add_text(request, "meter", options->meter_text) && add_text(request, "bars", options->bars_text) &&
              add_text(request, "count", options->count_text) && (!options->modulate || cJSON_AddTrueToObject(request, "modulate")) &&
              add_text(request, "accompaniment", options->accompaniment_text) && add_text(request, "texture", options->texture_text) &&
              add_text(request, "seed", options->seed_text) && add_text(request, "style", options->style_path) &&
              add_text(request, "style_weight", options->style_weight_text) && add_text(request, "blend", options->blend_path) &&
              add_text(request, "blend_weight", options->blend_weight_text) &&
              add_text(request, "blend_weights", options->blend_weights_text);
    for (size_t i = 0; ok && i < options->without.count; i++) {
        ok = cJSON_AddItemToArray(without, cJSON_CreateString(options->without.names[i]));
    }
    if (ok && options->without.count > 0) {
        ok = cJSON_AddItemToObject(request, "style_without", without);
        without = ok ? NULL : without;
    }
    if (ok && options->passages) {
        cJSON *passages = cJSON_Duplicate(options->passages, true);
        ok = passages && cJSON_AddItemToObject(request, "corpus_passages", passages);
        if (!ok) {
            cJSON_Delete(passages);
        }
    }
    if (ok && options->motive_path) {
        cJSON *motive = motive_request(options, options->motive_path, held);
        ok = motive && cJSON_AddItemToObject(request, "motive", motive);
    }
    for (size_t i = 0; ok && i < options->given_count; i++) {
        const GivenOption *option = &options->given[i];
        static const char *const KINDS[] = {"melody", "progression", "bass"};
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddStringToObject(item, "kind", option->from_corpus ? "like_progression" : KINDS[option->kind]) &&
             add_text(item, "at", option->at_text) && add_text(item, "cells", option->cells_text);
        if (ok && option->kind == GIVEN_PROGRESSION) {
            ok = cJSON_AddStringToObject(item, "numerals", option->source) != NULL;
            if (ok && option->passages) {
                cJSON *passages = cJSON_Duplicate(option->passages, true);
                ok = passages && cJSON_AddItemToObject(item, "corpus_passages", passages);
                if (!ok) {
                    cJSON_Delete(passages);
                }
            }
        } else if (ok) {
            cJSON *motive = motive_request(options, option->source, held);
            ok = motive && cJSON_AddItemToObject(item, "motive", motive);
        }
        ok = ok && cJSON_AddItemToArray(given, item);
        if (!ok) {
            cJSON_Delete(item);
        }
    }
    if (ok && options->given_count > 0) {
        ok = cJSON_AddItemToObject(request, "given", given);
        given = ok ? NULL : given;
    }
    cJSON_Delete(given);
    cJSON_Delete(without);
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* --- the result ------------------------------------------------------------------- */

static void print_seed_line(const char *what, const cJSON *seed) {
    const cJSON *start = cJSON_GetObjectItemCaseSensitive(seed, "start");
    const cJSON *end = cJSON_GetObjectItemCaseSensitive(seed, "end");
    printf("%s from %s m.%.0f b.%g - m.%.0f b.%g, score %s\n", what, cli_json_string(seed, "piece"),
           cli_json_number(start, "measure"), cli_json_number(start, "beat"), cli_json_number(end, "measure"),
           cli_json_number(end, "beat"), cli_json_string(seed, "score"));
}

static void print_section_text(const cJSON *section) {
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(section, "key");
    const cJSON *cadences = cJSON_GetObjectItemCaseSensitive(section, "cadences");
    printf("  %-3s %2.0f bars, key %s, cadence", cli_json_string(section, "letter"), cli_json_number(section, "bars"),
           cJSON_IsString(key) ? key->valuestring : "-");
    printf(" %s", cJSON_GetArraySize(cadences) > 0 ? cJSON_GetArrayItem(cadences, 0)->valuestring : "-");
    if (cJSON_GetArraySize(cadences) > 1) {
        /* An antecedent's weak cadence comes second in the plan, first in the music. */
        printf(" (antecedent %s)", cJSON_GetArrayItem(cadences, 1)->valuestring);
    }
    const cJSON *pairing = cJSON_GetObjectItemCaseSensitive(section, "pairing");
    if (cJSON_IsString(pairing)) {
        printf(", %s", pairing->valuestring);
    }
    const cJSON *returns = cJSON_GetObjectItemCaseSensitive(section, "returns");
    if (cJSON_IsObject(returns)) {
        printf(", returns %s (%s)", cli_json_string(returns, "section"), cli_json_string(returns, "variation"));
    }
    printf("\n");
}

static void print_given_text(const cJSON *document) {
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(document, "defaulted_form"))) {
        printf("no --form: a binary form of %.0f bars, the given material in B\n", cli_json_number(document, "bars"));
    }
    const cJSON *given = NULL;
    cJSON_ArrayForEach(given, cJSON_GetObjectItemCaseSensitive(document, "given")) {
        bool progression = strcmp(cli_json_string(given, "kind"), "progression") == 0;
        printf("given %s %s%s%s at %s:%.0f", cli_json_string(given, "kind"), progression ? "\"" : "", cli_json_string(given, "source"),
               progression ? "\"" : "", cli_json_string(given, "section"), cli_json_number(given, "bar"));
        if (progression) {
            double cells = cli_json_number(given, "cells");
            printf(", %.0f %s a chord", cells, cells == 1 ? "cell" : "cells");
        }
        printf("\n");
        const cJSON *from = cJSON_GetObjectItemCaseSensitive(given, "from");
        if (from) {
            print_seed_line("progression", from);
        }
    }
}

static void print_result_text(const cJSON *document, const char *out_path) {
    printf("%s, %.0f bars, key %s, meter %s, seed %.0f%s\n", cli_json_string(document, "form"), cli_json_number(document, "bars"),
           cli_json_string(document, "key"), cli_json_string(document, "meter"), cli_json_number(document, "seed"),
           cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(document, "modulate")) ? ", keys and cadences planned" : "");
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(document, "motive_source");
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(document, "motive");
    if (source) {
        print_seed_line("motive", source);
    } else if (cJSON_IsString(motive)) {
        printf("motive %s\n", motive->valuestring);
    } else {
        printf("motive invented at fill\n");
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(document, "accompaniment");
    if (cJSON_IsString(accompaniment)) {
        printf("accompaniment %s", accompaniment->valuestring);
        const cJSON *texture = cJSON_GetObjectItemCaseSensitive(document, "accompaniment_texture");
        if (cJSON_IsString(texture)) {
            printf(" (%s)", texture->valuestring);
        }
        printf("\n");
    }
    const cJSON *style = cJSON_GetObjectItemCaseSensitive(document, "style");
    if (style) {
        printf("style %s, weight %s\n", cli_json_string(style, "file"), cli_json_string(style, "weight"));
        const cJSON *blend = cJSON_GetObjectItemCaseSensitive(document, "blend");
        if (blend) {
            printf("blend %s, second style's share", cli_json_string(blend, "file"));
            size_t at = 0;
            const cJSON *weight = NULL;
            cJSON_ArrayForEach(weight, cJSON_GetObjectItemCaseSensitive(blend, "weights")) {
                printf("%s %s %s", at++ ? "," : "", weight->string, weight->valuestring);
            }
            printf("\n");
        }
    }
    print_given_text(document);
    printf("sections\n");
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(document, "sections")) {
        print_section_text(section);
    }
    printf("wrote %s\n", out_path);
}

/* The file an error about an input is about: a given material's, else the motive's. */
static const char *input_name(const Options *options, const char *path) {
    if (strncmp(path, "/given/", 7) == 0) {
        unsigned long index = strtoul(path + 7, NULL, 10);
        return index < options->given_count ? options->given[index].source : options->motive_path;
    }
    return options->motive_path;
}

static int request_plan(Options *options_in);

int run_plan(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }
    /* The passages are found here, by the Analyzer, and given to the API inline (R-619). */
    if (options.like_path) {
        options.passages = cli_corpus_passages(options.from_corpus, options.like_path, NULL, &status);
    }
    for (size_t i = 0; status == 0 && i < options.given_count; i++) {
        GivenOption *given = &options.given[i];
        if (given->from_corpus) {
            given->passages = cli_corpus_passages(options.from_corpus, NULL, given->source, &status);
        }
    }
    if (status == 0) {
        status = request_plan(&options);
    }
    cJSON_Delete(options.passages);
    for (size_t i = 0; i < options.given_count; i++) {
        cJSON_Delete(options.given[i].passages);
    }
    return status;
}

/* Builds the request, calls the API and prints the result. */
static int request_plan(Options *options_in) {
    Options options = *options_in;
    int status = 0;
    Held held;
    memset(&held, 0, sizeof held);
    cJSON *request = build_request(&options, &held);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus called = cli_call("plan", request, &response);
    bool load_failure = called != URSATZ_OK && strncmp(response.error.message, "cannot load the input", 21) == 0;
    if (called != URSATZ_OK && !(held.failed && load_failure)) {
        status = cli_fail_api(called, &response.error, input_name(&options, response.error.path));
    } else if (held.failed) {
        status = cli_fail(held.status, "%s", held.message);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        cJSON *plan = cJSON_Parse(response.plan_json);
        CliFile file = {options.out_path, CLI_FILE_PLAN, plan, NULL, 0, false};
        status = document && plan ? cli_write_files(&file, 1) : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            cli_print_corpus_notices(document, options.from_corpus);
            if (options.json) {
                cJSON_DeleteItemFromObjectCaseSensitive(document, "accompaniment_texture");
                cJSON_DeleteItemFromObjectCaseSensitive(document, "defaulted_form");
                cJSON_AddStringToObject(document, "file", options.out_path);
                char *text = cJSON_PrintUnformatted(document);
                printf("%s\n", text ? text : "{}");
                cJSON_free(text);
            } else {
                print_result_text(document, options.out_path);
            }
        }
        cJSON_Delete(document);
        cJSON_Delete(plan);
    }
    cli_response_destroy(&response);
    return status;
}
