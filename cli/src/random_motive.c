#include "random_motive.h"
#include "cli_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "cli_options.h"
#include "motive_extras.h"

#define RANDOM_MOTIVE_COUNT_DEFAULT 1

typedef struct {
    const char *material_text;
    const char *meter_text;
    const char *key_text;
    const char *form_text;
    const char *strategy_text;
    const char *style_path;
    CliStyleWithout without;
    const char *count_text;
    const char *seed_text;
    MotiveExtras extras;
    bool json;
} Options;


static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = motive_extras_read(&options->extras, args, true);
    if (extras_status != 0) {
        return extras_status;
    }
    int added = cli_style_without_read(&options->without, args);
    if (added != 0) {
        return added;
    }
    options->material_text = cli_value(args, "--material");
    options->meter_text = cli_value(args, "--meter");
    options->key_text = cli_value(args, "--key");
    options->form_text = cli_value(args, "--form");
    options->strategy_text = cli_value(args, "--strategy");
    options->style_path = cli_value(args, "--style");
    options->count_text = cli_value(args, "--count");
    options->seed_text = cli_value(args, "--seed");
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

static void print_text(const cJSON *document) {
    printf("Seed: %.0f\n", member(document, "seed")->valuedouble);
    const cJSON *drawn = member(document, "drawn");
    if (drawn) {
        const cJSON *mode = member(drawn, "material");
        const cJSON *meter = member(drawn, "meter");
        char word[64];
        printf("Drawn:%s%s%s%s\n", mode ? " mode " : "", mode ? cli_material_word(mode->valuestring, word, sizeof word) : "", meter ? (mode ? ", meter " : " meter ") : "",
               meter ? meter->valuestring : "");
    }
    size_t number = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, member(document, "motives")) {
        const cJSON *analysis = member(item, "analysis");
        const cJSON *form = member(analysis, "form");
        const char *form_name = cJSON_IsString(form) ? form->valuestring : "full";
        const cJSON *text = member(analysis, strcmp(form_name, "rhythm") == 0    ? "rhythm_text"
                                             : strcmp(form_name, "contour") == 0 ? "contour_text"
                                                                                 : "notes_text");
        number++;
        printf("\nMotive %zu: %s\n", number, cJSON_IsString(text) ? text->valuestring : "");
        const cJSON *degrees = member(analysis, "degrees_text");
        if (strcmp(form_name, "full") == 0) {
            printf("  degrees: %s\n", cJSON_IsString(degrees) ? degrees->valuestring : "");
        }
    }
}

/* Adds the options to the request. */
static bool add_members(cJSON *request, const Options *options) {
    bool ok = true;
    const struct {
        const char *name;
        const char *text;
    } texts[] = {{"material", options->material_text}, {"meter", options->meter_text}, {"key", options->key_text},
                 {"form", options->form_text}, {"strategy", options->strategy_text}, {"style", options->style_path}, {"count", options->count_text},
                 {"seed", options->seed_text}, {"role", options->extras.role_text}};
    for (size_t i = 0; ok && i < sizeof texts / sizeof texts[0]; i++) {
        ok = !texts[i].text || cJSON_AddStringToObject(request, texts[i].name, texts[i].text) != NULL;
    }
    cJSON *without = ok && options->without.count > 0 ? cJSON_AddArrayToObject(request, "style_without") : NULL;
    for (size_t i = 0; ok && i < options->without.count; i++) {
        ok = without && cJSON_AddItemToArray(without, cJSON_CreateString(options->without.names[i]));
    }
    return ok;
}

int run_random_motive(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (options.without.count > 0 && !options.style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without needs --style");
    }
    if (!options.material_text || !options.meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "random-motive needs --material and --meter\n%s", cli_usage_of("random-motive"));
    }
    cJSON *request = cJSON_CreateObject();
    if (!request || !add_members(request, &options)) {
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer("random_motive", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        cli_response_destroy(&response);
        return status;
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
