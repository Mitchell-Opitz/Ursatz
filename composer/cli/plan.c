#include "plan.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>

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
} GivenOption;

typedef struct {
    bool help;
    const char *form_text;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *bars_text;
    const char *count_text;
    bool modulate;
    const char *motive_path;
    const char *from_corpus;
    const char *like_path;
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

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", PLAN_USAGE_LINE + 7);
    fprintf(out, "Writes the plan of a whole short piece from a form template: its sections, their keys and cadences, the\n");
    fprintf(out, "sections that return an earlier one, and the motive. Nothing is composed yet; 'ursatz-composer fill'\n");
    fprintf(out, "realizes the plan.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --form <name>        binary, rounded-binary, ternary, variations, loop or layered; with --given-* material\n");
    fprintf(out, "                       it may be left out, and the form is a binary form of 16 bars\n");
    fprintf(out, "  --key <K>            the tonic (\"D\", \"F#\", \"Bb\"); or tonic and mode in one argument (\"D minor\")\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8\n");
    fprintf(out, "  --out <plan.json>    where the plan is written\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --mode <major|minor> the mode when --key is a tonic only (default: major)\n");
    fprintf(out, "  --bars <n>           the size of the piece in bars; the sizes of each form are listed when one is refused\n");
    fprintf(out, "  --count <n>          the variations (variations) or the layers (layered), instead of --bars\n");
    fprintf(out, "  --modulate           plan the keys and cadences across the sections from the seed (otherwise every section\n");
    fprintf(out, "                       is in the home key and the cadences are the form's)\n");
    fprintf(out, "  --motive <file>      the motive: a MIDI file, a MusicXML file, a note list or a motive JSON file; without\n");
    fprintf(out, "                       it one is invented when the plan is filled\n");
    fprintf(out, "  --from-corpus <dir>  the output directory of ursatz-analyzer --corpus, for --like and --like-progression; the\n");
    fprintf(out, "                       passage used, its measures and its score are printed and kept in the plan (SOURCE). Exit 2\n");
    fprintf(out, "                       when no passage scores 1/2 or more\n");
    fprintf(out, "  --like <file>        the motive: the first bar of a passage of the corpus similar to this MIDI or MusicXML\n");
    fprintf(out, "                       passage (picked by the seed among the best), in --key; instead of --motive\n");
    fprintf(out, "  --like-progression \"<I IV V I>\"\n");
    fprintf(out, "                       a progression to place like --given-progression, but the chords are those of the passage of\n");
    fprintf(out, "                       the corpus that best matches these numerals; goes with --at (and --cells)\n");
    fprintf(out, "  --accompaniment <p>  block, arpeggiated, alberti (not in 6/8), oom-pah or any (a layered form has one in any\n");
    fprintf(out, "                       case)\n");
    fprintf(out, "  --texture <name>     the texture the accompaniment is to be heard as (see grow --help); implies\n");
    fprintf(out, "                       --accompaniment any\n");
    fprintf(out, "  --given-melody <f>   a phrase to place in the piece: a MIDI file, a MusicXML file, a note list or a motive JSON\n");
    fprintf(out, "                       file of any length; fill never changes it (repeatable)\n");
    fprintf(out, "  --given-progression \"<I IV V I>\"\n");
    fprintf(out, "                       Roman numerals, one chord each, to place in the piece (repeatable)\n");
    fprintf(out, "  --given-bass <f>     a bass line to place in the piece, the same file formats, no rests (repeatable)\n");
    fprintf(out, "  --at <L>[:<bar>]     where the --given-* before it goes: the section's letter (\"B\", \"A'\") and, from 1, the bar\n");
    fprintf(out, "                       of it the material starts in (default 1); without --form, the default is B:1\n");
    fprintf(out, "  --cells <n>          the cells each chord of the --given-progression before it lasts (default: a bar)\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same plan. Default: from the clock\n");
    fprintf(out, "  --style <file>       a style model file from the Ursatz Analyzer (--style-profile): fill prefers the harmony\n");
    fprintf(out, "                       and melody closest to it. The path is kept in the plan as given, so fill finds the file\n");
    fprintf(out, "                       from where it is run\n");
    fprintf(out, "  --style-weight <n/d> how strongly fill prefers the style (default 1/1; needs --style)\n");
    fprintf(out, "  --blend <file>       a second style model file, blended with --style (needs --style); kept in the plan\n");
    fprintf(out, "  --blend-weight <n/d> the second style's share in every dimension, 0 to 1 (default 1/2; needs --blend)\n");
    fprintf(out, "  --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>\n");
    fprintf(out, "                       the share per dimension; one not named takes --blend-weight\n");
    fprintf(out, "  --format <text|json> how to print the plan on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument (an unknown form, a size the form does not have, a bad\n");
    fprintf(out, "key or meter); 2 the request cannot be satisfied (an unreadable motive, a motive too long for a bar);\n");
    fprintf(out, "3 the plan could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer plan --form ternary --key \"A\" --mode minor --meter 3/4 --out piece.json\n");
    fprintf(out, "  ursatz-composer plan --form binary --key \"D minor\" --meter 4/4 --bars 8 --motive m.txt --out piece.json\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--modulate") == 0) {
            options->modulate = true;
        } else if (strcmp(arg, "--form") == 0) {
            target = &options->form_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count_text;
        } else if (strcmp(arg, "--motive") == 0) {
            target = &options->motive_path;
        } else if (strcmp(arg, "--from-corpus") == 0) {
            target = &options->from_corpus;
        } else if (strcmp(arg, "--like") == 0) {
            target = &options->like_path;
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->accompaniment_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->texture_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--style") == 0) {
            target = &options->style_path;
        } else if (strcmp(arg, "--style-without") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            int added = cli_style_without_add(&options->without, argv[++i]);
            if (added != 0) {
                return added;
            }
        } else if (strcmp(arg, "--style-weight") == 0) {
            target = &options->style_weight_text;
        } else if (strcmp(arg, "--blend") == 0) {
            target = &options->blend_path;
        } else if (strcmp(arg, "--blend-weight") == 0) {
            target = &options->blend_weight_text;
        } else if (strcmp(arg, "--blend-weights") == 0) {
            target = &options->blend_weights_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (strcmp(arg, "--given-melody") == 0 || strcmp(arg, "--given-progression") == 0 ||
                   strcmp(arg, "--given-bass") == 0 || strcmp(arg, "--like-progression") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (options->given_count == MAX_GIVEN) {
                return cli_fail(CLI_EXIT_USAGE, "plan takes %d given phrases, progressions and bass lines at most", MAX_GIVEN);
            }
            GivenOption *given = &options->given[options->given_count++];
            given->from_corpus = arg[2] == 'l';
            given->kind = given->from_corpus ? GIVEN_PROGRESSION
                          : arg[8] == 'm'    ? GIVEN_MELODY
                          : arg[8] == 'p'    ? GIVEN_PROGRESSION
                                             : GIVEN_BASS;
            given->source = argv[++i];
        } else if (strcmp(arg, "--at") == 0 || strcmp(arg, "--cells") == 0) {
            bool at = arg[2] == 'a';
            if (options->given_count == 0) {
                return cli_fail(CLI_EXIT_USAGE, "%s goes after a --given-melody, --given-progression or --given-bass", arg);
            }
            GivenOption *given = &options->given[options->given_count - 1];
            const char **slot = at ? &given->at_text : &given->cells_text;
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice for the same --given-*", arg);
            }
            *slot = argv[++i];
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer plan --help)", arg);
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; plan takes options only", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    return 0;
}

static int check_options(Options *options) {
    if ((!options->form_text && options->given_count == 0) || !options->key_text || !options->meter_text || !options->out_path) {
        return cli_fail(CLI_EXIT_USAGE, "plan needs --form, --key, --meter and --out (see --help)\n%s", PLAN_USAGE_LINE);
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
 * composer_api_motive_from_midi and arrives as the motive it made. *held says why a file could not be read.
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
        snprintf(key, sizeof key, minor_key ? "%s" : "%s %s", options->key_text, options->mode_text ? options->mode_text : "major");
        bool ok = midi_request && cJSON_AddStringToObject(midi_request, "meter", options->meter_text) &&
                  cJSON_AddStringToObject(midi_request, "key", key);
        char *printed = ok ? cJSON_PrintUnformatted(midi_request) : NULL;
        cJSON_Delete(midi_request);
        ComposerResponse response;
        ComposerStatus status = printed ? composer_api_motive_from_midi((const uint8_t *)bytes, size, printed, NULL, &response)
                                        : COMPOSER_ERR_ALLOC;
        cJSON_free(printed);
        if (status != COMPOSER_OK) {
            held->failed = true;
            held->status = status == COMPOSER_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE;
            snprintf(held->message, sizeof held->message, "cannot load '%s': %s", path, response.error.message);
            composer_response_destroy(&response);
            text = "";
        } else {
            cJSON *document = cJSON_Parse(response.json);
            motive_text = document ? cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "motive")) : NULL;
            cJSON_Delete(document);
            composer_response_destroy(&response);
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
              add_text(request, "key", options->key_text) && add_text(request, "mode", options->mode_text) &&
              add_text(request, "meter", options->meter_text) && add_text(request, "bars", options->bars_text) &&
              add_text(request, "count", options->count_text) && (!options->modulate || cJSON_AddTrueToObject(request, "modulate")) &&
              add_text(request, "corpus", options->from_corpus) && add_text(request, "like", options->like_path) &&
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

int run_plan(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }
    Held held;
    memset(&held, 0, sizeof held);
    cJSON *request = build_request(&options, &held);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    ComposerResponse response;
    ComposerStatus called = cli_call(composer_api_plan, request, &response);
    bool load_failure = called != COMPOSER_OK && strncmp(response.error.message, "cannot load the input", 21) == 0;
    if (called != COMPOSER_OK && !(held.failed && load_failure)) {
        status = cli_fail_api(called, &response.error, input_name(&options, response.error.path));
    } else if (held.failed) {
        status = cli_fail(held.status, "%s", held.message);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        cJSON *plan = cJSON_Parse(response.plan_json);
        CliFile file = {options.out_path, CLI_FILE_PLAN, plan, NULL, 0, false};
        status = document && plan ? cli_write_files(&file, 1) : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            cli_print_notices(document);
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
    composer_response_destroy(&response);
    return status;
}
