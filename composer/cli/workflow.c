#include "workflow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "util/request_parse.h"
#include "util/util.h"

/* --- reading --------------------------------------------------------------------------------------- */

void cli_read_plan_held(const char *path, cJSON **plan, CliHeldFailure *held) {
    char *text = NULL;
    *plan = NULL;
    held->failed = false;
    held->message[0] = '\0';
    if (!util_read_file(path, &text, NULL)) {
        held->failed = true;
        snprintf(held->message, sizeof held->message, "cannot read plan '%s': could not open '%s'", path, path);
        return;
    }
    *plan = cJSON_Parse(text);
    free(text);
    if (!*plan) {
        held->failed = true;
        snprintf(held->message, sizeof held->message, "cannot read plan '%s': not valid JSON", path);
    }
}

int cli_read_plan_file(const char *path, cJSON **plan) {
    CliHeldFailure held;
    cli_read_plan_held(path, plan, &held);
    return held.failed ? cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", held.message) : 0;
}

int cli_fail_plan_api(ComposerStatus status, const ComposerError *error, const char *plan_file, const CliHeldFailure *held) {
    if (held && held->failed && strcmp(error->path, "/plan") == 0) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", held->message);
    }
    static const char PREFIX[] = "cannot read plan: ";
    if (strncmp(error->message, PREFIX, sizeof PREFIX - 1) == 0) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", plan_file, error->message + sizeof PREFIX - 1);
    }
    return cli_fail_api(status, error, plan_file);
}

ComposerStatus cli_call(CliEntry entry, cJSON *request, ComposerResponse *out) {
    char *text = request ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!text) {
        memset(out, 0, sizeof *out);
        snprintf(out->error.message, sizeof out->error.message, "out of memory");
        return COMPOSER_ERR_ALLOC;
    }
    ComposerStatus status = entry(text, NULL, out);
    cJSON_free(text);
    return status;
}

/* --- the node a command works on ------------------------------------------------------------------- */

int cli_check_target(const CliTargetOptions *options, const char *command, const char *usage) {
    unsigned long node;
    if (!options->node_text == !options->section_text) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs a node: give --node <id> or --section <letter>, not both\n%s", command, usage);
    }
    if (options->node_text && !request_parse_whole_number(options->node_text, 1000000UL, &node)) {
        return cli_fail(CLI_EXIT_USAGE, "--node must be a whole number (the id in the plan file), not '%s'", options->node_text);
    }
    if (options->lane_text &&
        !(request_equals_ignore_case(options->lane_text, "harmony") || request_equals_ignore_case(options->lane_text, "bass") ||
          request_equals_ignore_case(options->lane_text, "melody") || request_equals_ignore_case(options->lane_text, "inner"))) {
        return cli_fail(CLI_EXIT_USAGE, "--lane must be harmony, bass, melody or inner, not '%s'", options->lane_text);
    }
    return 0;
}

bool cli_add_target(cJSON *request, const CliTargetOptions *target) {
    unsigned long node;
    bool ok = true;
    if (target->node_text && request_parse_whole_number(target->node_text, 1000000UL, &node)) {
        ok = cJSON_AddNumberToObject(request, "node", (double)node) != NULL;
    } else if (target->section_text) {
        ok = cJSON_AddStringToObject(request, "section", target->section_text) != NULL;
    }
    return ok && (!target->lane_text || cJSON_AddStringToObject(request, "lane", target->lane_text));
}

/* --- writing --------------------------------------------------------------------------------------- */

#define PATH_SIZE 1024

int cli_write_files(const CliFile *files, size_t count) {
    char (*names)[PATH_SIZE + 8] = calloc(count ? count : 1, PATH_SIZE + 8);
    if (!names) {
        return cli_fail(CLI_EXIT_WRITE, "out of memory");
    }
    size_t pending = 0;
    int status = 0;
    for (size_t i = 0; i < count && status == 0; i++) {
        const CliFile *file = &files[i];
        if (strlen(file->path) >= PATH_SIZE || snprintf(names[i], PATH_SIZE + 8, "%s.tmp", file->path) >= PATH_SIZE + 8) {
            status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", file->path);
            break;
        }
        pending++;
        if (file->kind == CLI_FILE_PLAN) {
            status = cli_write_plan_object(file->plan, names[i], file->path);
        } else if (!cli_write_bytes(names[i], file->data, file->size)) {
            status = file->detailed ? cli_fail(CLI_EXIT_WRITE, "cannot write '%s': could not write '%s'", file->path, names[i])
                                    : cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", file->path);
        }
    }
    for (size_t i = 0; status == 0 && i < count; i++) {
        if (rename(names[i], files[i].path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", files[i].path);
        }
    }
    if (status != 0) {
        for (size_t i = 0; i < pending; i++) {
            remove(names[i]);
        }
    }
    free(names);
    return status;
}

int cli_export_midi(const cJSON *plan, const char *tempo_text, bool plain_midi, ComposerResponse *out) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, true)) &&
              cJSON_AddBoolToObject(request, "midi", true) && cJSON_AddBoolToObject(request, "plain_midi", plain_midi);
    if (ok && tempo_text) {
        ok = cJSON_AddStringToObject(request, "tempo", tempo_text) != NULL;
    }
    if (!ok) {
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
    }
    ComposerStatus status = cli_call(composer_api_export, request, out);
    return status == COMPOSER_OK ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
}

/* --- printing -------------------------------------------------------------------------------------- */

const char *cli_json_string(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

double cli_json_number(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : 0;
}

void cli_print_notices(cJSON *document) {
    const cJSON *notice = NULL;
    cJSON_ArrayForEach(notice, cJSON_GetObjectItemCaseSensitive(document, "notices")) {
        if (cJSON_IsString(notice)) {
            fprintf(stderr, "Warning: %s\n", notice->valuestring);
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");
}

/* What was heard in a section, in words: the "heard" text, or "kept" for a section outside the scope. */
static const char *heard_of(const cJSON *section) {
    const cJSON *heard = cJSON_GetObjectItemCaseSensitive(section, "heard");
    return cJSON_IsString(heard) ? heard->valuestring : "kept";
}

static void print_cadences(const cJSON *section) {
    const cJSON *cadences = cJSON_GetObjectItemCaseSensitive(section, "cadences");
    if (cJSON_GetArraySize(cadences) > 0) {
        printf(", cadences");
        size_t at = 0;
        const cJSON *cadence = NULL;
        cJSON_ArrayForEach(cadence, cadences) {
            printf(at++ ? ", %s" : " %s", cadence->valuestring);
        }
    }
}

void cli_print_heard_line(const cJSON *sections) {
    size_t at = 0;
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, sections) {
        printf("%s%s %s", at++ ? "; " : "", cli_json_string(section, "letter"), heard_of(section));
        print_cadences(section);
    }
}

void cli_print_candidates_text(const cJSON *document) {
    const cJSON *candidates = cJSON_GetObjectItemCaseSensitive(document, "candidates");
    if (cJSON_GetArraySize(candidates) == 0) {
        return;
    }
    printf("\ncandidates (%zu of %.0f completed, best first)\n", (size_t)cJSON_GetArraySize(candidates),
           cli_json_number(document, "candidates_tried"));
    const cJSON *candidate = NULL;
    cJSON_ArrayForEach(candidate, candidates) {
        printf("  seed %.0f  agreement %.0f/%.0f  score %s%s\n", cli_json_number(candidate, "seed"),
               cli_json_number(candidate, "agreed"), cli_json_number(candidate, "planned"), cli_json_string(candidate, "score"),
               cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(candidate, "written")) ? "  written" : "");
    }
    printf("\n");
}

static void print_style_distance(const cJSON *distance) {
    printf("\nstyle distance (the Analyzer's, 0 is the style, 1 is nothing alike)\n");
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, distance) {
        printf("  %-8s %.4f\n", item->string, item->valuedouble);
    }
}

static void print_given_text(const cJSON *given) {
    printf("\ngiven material, %s\n", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(given, "unchanged")) ? "unchanged" : "CHANGED");
    const cJSON *run = NULL;
    cJSON_ArrayForEach(run, cJSON_GetObjectItemCaseSensitive(given, "runs")) {
        printf("  %s in %s from bar %.0f, ", cli_json_string(run, "kind"), cli_json_string(run, "section"), cli_json_number(run, "bar"));
        if (cJSON_GetObjectItemCaseSensitive(run, "chords")) {
            double chords = cli_json_number(run, "chords");
            printf("%.0f %s", chords, chords == 1 ? "chord" : "chords");
        } else {
            double bars = cli_json_number(run, "bars");
            printf("%.0f %s", bars, bars == 1 ? "bar" : "bars");
        }
        printf("%s\n", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run, "unchanged")) ? "" : ": changed");
    }
}

void cli_print_composed_text(const cJSON *document, const char *const *files, size_t file_count) {
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(document, "realized"))) {
        printf("nothing to fill: the plan is already realized\n");
        for (size_t i = 0; i < file_count; i++) {
            printf("wrote %s\n", files[i]);
        }
        return;
    }
    printf("seed %.0f\n", cli_json_number(document, "seed"));
    printf("\nsections (form and cadences heard)\n");
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(document, "sections")) {
        printf("  %-3s %s", cli_json_string(section, "letter"), heard_of(section));
        print_cadences(section);
        printf("\n");
    }
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(document, "analyzer");
    if (analyzer) {
        printf("\nanalyzer check: %.0f of %.0f planned cadences and keys heard as planned\n", cli_json_number(analyzer, "agreed"),
               cli_json_number(analyzer, "planned"));
    }
    const cJSON *distance = cJSON_GetObjectItemCaseSensitive(document, "style_distance");
    if (distance) {
        print_style_distance(distance);
    }
    const cJSON *returns = cJSON_GetObjectItemCaseSensitive(document, "returns");
    printf("\nreturns heard %.0f of %.0f planned\n", cli_json_number(returns, "heard"), cli_json_number(returns, "planned"));
    const cJSON *pair = NULL;
    cJSON_ArrayForEach(pair, cJSON_GetObjectItemCaseSensitive(returns, "pairs")) {
        printf("  %s resembles %s (similarity %.2f)\n", cli_json_string(pair, "later"), cli_json_string(pair, "earlier"),
               cli_json_number(pair, "similarity"));
    }
    const cJSON *given = cJSON_GetObjectItemCaseSensitive(document, "given");
    if (given) {
        print_given_text(given);
    }
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

void cli_print_composed_json(cJSON *document, const char *const *files, size_t file_count) {
    cJSON *list = cJSON_CreateArray();
    cJSON_DeleteItemFromObjectCaseSensitive(document, "candidates_tried");
    for (size_t i = 0; list && i < file_count; i++) {
        cJSON_AddItemToArray(list, cJSON_CreateString(files[i]));
    }
    if (list) {
        cJSON_AddItemToObject(document, "files", list);
    }
    char *text = cJSON_PrintUnformatted(document);
    printf("%s\n", text ? text : "{}");
    cJSON_free(text);
}

int cli_report_stuck(const ComposerError *error, const ComposerResponse *response) {
    fprintf(stderr, "Error: %s\n", error->message);
    cJSON *document = response && response->json ? cJSON_Parse(response->json) : NULL;
    const cJSON *stuck = cJSON_GetObjectItemCaseSensitive(document, "stuck");
    const cJSON *open = NULL;
    cJSON_ArrayForEach(open, cJSON_GetObjectItemCaseSensitive(stuck, "open")) {
        fprintf(stderr, "  open: %s: %s\n", cli_json_string(open, "what"), cli_json_string(open, "reason"));
    }
    if (cli_json_number(stuck, "more") > 0) {
        fprintf(stderr, "  and %.0f more\n", cli_json_number(stuck, "more"));
    }
    cJSON_Delete(document);
    return CLI_EXIT_UNSATISFIABLE;
}
