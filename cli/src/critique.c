#include "critique.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_params.h"
#include "cli_tools.h"

/*
 * ursatz critique (R-721): candidate files judged by the critic. A file is a listen sample (its motive member, or the plan of its first
 * theme: listen --plans), a plan document or a motive document. The files of one call are one set, which the critic compares (one_rhythm).
 * --rated reads a rated set (ursatz-rated-set 1) and prints, for each pair, the critic's choice against her ratings and against today's
 * ranking (a theme's own score).
 */

#define MAX_FILES 16
#define MAX_RATED_FILES 512

/* ------------------------------------------------------------------------------------------------ files */

static int read_json(const char *path, const char *what, cJSON **out) {
    char *text = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s '%s'", what, path);
    }
    *out = cJSON_Parse(text);
    free(text);
    return *out ? 0 : cli_fail(CLI_EXIT_USAGE, "%s '%s' is not valid JSON", what, path);
}

static bool has_array(const cJSON *document, const char *name) {
    return cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(document, name));
}

/* The candidate a file's document is: {"motive": ...} or {"plan": ...}. NULL, with *why, when it is neither. */
static cJSON *candidate_of(const cJSON *document, const char **why) {
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(document, "motive");
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    const cJSON *kept = cJSON_IsObject(motive) ? motive : NULL;
    const char *member = "motive";
    if (!kept && cJSON_IsArray(themes)) {
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(themes, 0), "plan");
        kept = cJSON_IsObject(plan) ? plan : NULL;
        member = "plan";
        if (!kept) {
            *why = "has no plan: make the theme with listen --plans";
            return NULL;
        }
    }
    if (!kept && has_array(document, "notes") && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(document, "meter"))) {
        kept = document;
    }
    if (!kept && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(document, "meter")) && cJSON_GetObjectItemCaseSensitive(document, "schema_version")) {
        kept = document;
        member = "plan";
    }
    if (!kept) {
        *why = "is not a listen sample, a plan or a motive";
        return NULL;
    }
    cJSON *candidate = cJSON_CreateObject();
    if (!candidate || !cJSON_AddItemToObject(candidate, member, cJSON_Duplicate(kept, true))) {
        cJSON_Delete(candidate);
        *why = "could not be read (out of memory)";
        return NULL;
    }
    return candidate;
}

/* The request of these candidates (their objects are taken) and the settings document (kept by the caller). */
static cJSON *request_of(cJSON *candidates, const cJSON *settings) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddStringToObject(request, "schema", "ursatz-critique-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 1) && cJSON_AddItemToObject(request, "candidates", candidates);
    if (ok && settings) {
        ok = cJSON_AddItemToObject(request, "preferences", cJSON_Duplicate(settings, true));
    }
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* Asks the critic about the files. *answer is the response document; the status is 0 or an exit status after printing why not. */
static int ask(const char *const *files, size_t count, const cJSON *settings, cJSON **answer) {
    cJSON *candidates = cJSON_CreateArray();
    *answer = NULL;
    for (size_t i = 0; candidates && i < count; i++) {
        cJSON *document = NULL;
        const char *why = NULL;
        int status = read_json(files[i], "candidate", &document);
        if (status != 0) {
            cJSON_Delete(candidates);
            return status;
        }
        cJSON *candidate = candidate_of(document, &why);
        cJSON_Delete(document);
        if (!candidate) {
            cJSON_Delete(candidates);
            return cli_fail(CLI_EXIT_USAGE, "'%s' %s", files[i], why);
        }
        cJSON_AddItemToArray(candidates, candidate);
    }
    cJSON *request = candidates ? request_of(candidates, settings) : NULL;
    if (!request) {
        cJSON_Delete(candidates);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus status = cli_composer("critique", request, NULL, &response);
    cJSON_Delete(request);
    if (status != URSATZ_OK) {
        int exit_status = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s%s%s", response.error.message, response.error.path[0] ? " at " : "", response.error.path);
        cli_response_destroy(&response);
        return exit_status;
    }
    *answer = cJSON_Parse(response.json);
    cli_response_destroy(&response);
    return *answer ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "the critic gave no answer");
}

/* ------------------------------------------------------------------------------------------------ text */

static double ratio_value(const cJSON *ratio) {
    const cJSON *numerator = cJSON_GetObjectItemCaseSensitive(ratio, "numerator");
    const cJSON *denominator = cJSON_GetObjectItemCaseSensitive(ratio, "denominator");
    return cJSON_IsNumber(numerator) && cJSON_IsNumber(denominator) && denominator->valuedouble != 0 ? numerator->valuedouble / denominator->valuedouble : 0;
}

static void print_ratio(const cJSON *ratio) {
    const cJSON *numerator = cJSON_GetObjectItemCaseSensitive(ratio, "numerator");
    const cJSON *denominator = cJSON_GetObjectItemCaseSensitive(ratio, "denominator");
    printf("%.0f/%.0f", cJSON_IsNumber(numerator) ? numerator->valuedouble : 0, cJSON_IsNumber(denominator) ? denominator->valuedouble : 1);
}

static void print_text(const char *const *files, const cJSON *answer) {
    const cJSON *candidate;
    size_t i = 0;
    cJSON_ArrayForEach(candidate, cJSON_GetObjectItemCaseSensitive(answer, "candidates")) {
        const cJSON *observation;
        printf("%s  score ", files[i++]);
        print_ratio(cJSON_GetObjectItemCaseSensitive(candidate, "score"));
        printf("\n");
        cJSON_ArrayForEach(observation, cJSON_GetObjectItemCaseSensitive(candidate, "observations")) {
            const cJSON *bar = cJSON_GetObjectItemCaseSensitive(observation, "bar");
            const cJSON *beat = cJSON_GetObjectItemCaseSensitive(observation, "beat");
            const cJSON *rule = cJSON_GetObjectItemCaseSensitive(observation, "rule");
            const cJSON *message = cJSON_GetObjectItemCaseSensitive(observation, "message");
            printf("  bar ");
            if (cJSON_IsNumber(bar)) {
                printf("%.0f", bar->valuedouble);
            } else {
                printf("-");
            }
            printf(" beat ");
            if (cJSON_IsObject(beat)) {
                print_ratio(beat);
            } else {
                printf("-");
            }
            printf("  %s  %s\n", cJSON_IsString(rule) ? rule->valuestring : "", cJSON_IsString(message) ? message->valuestring : "");
        }
    }
}

/* ------------------------------------------------------------------------------------------------ files mode */

static int run_files(const CliArgs *args, const cJSON *settings, bool json) {
    if (args->input_count == 0) {
        return cli_needs("critique", "a candidate file or --rated", true);
    }
    cJSON *answer = NULL;
    const char *files[MAX_FILES];
    if (args->input_count > MAX_FILES) {
        return cli_fail(CLI_EXIT_USAGE, "critique takes at most %d candidate files", MAX_FILES);
    }
    for (size_t i = 0; i < args->input_count; i++) {
        files[i] = args->inputs[i];
    }
    int status = ask(files, args->input_count, settings, &answer);
    if (status != 0) {
        return status;
    }
    if (json) {
        char *text = cJSON_Print(answer);
        printf("%s\n", text ? text : "");
        cJSON_free(text);
    } else {
        print_text(files, answer);
    }
    cJSON_Delete(answer);
    return 0;
}

/* ------------------------------------------------------------------------------------------------ rated sets */

typedef struct {
    char label[64];
    char *files[MAX_FILES * 4]; /* the paths of the set, in first-appearance order */
    size_t count;
    double score[MAX_FILES * 4]; /* the critic's score of each file */
} Set;

typedef struct {
    Set *sets;
    size_t count;
} Sets;

static char *join_path(const char *base_file, const char *relative) {
    const char *slash = strrchr(base_file, '/');
    size_t directory = slash ? (size_t)(slash - base_file) + 1 : 0;
    char *path = malloc(directory + strlen(relative) + 1);
    if (path) {
        memcpy(path, base_file, directory);
        strcpy(path + directory, relative);
    }
    return path;
}

static Set *set_named(Sets *sets, const char *label) {
    for (size_t i = 0; i < sets->count; i++) {
        if (strcmp(sets->sets[i].label, label) == 0) {
            return &sets->sets[i];
        }
    }
    Set *grown = realloc(sets->sets, (sets->count + 1) * sizeof(Set));
    if (!grown) {
        return NULL;
    }
    sets->sets = grown;
    memset(&grown[sets->count], 0, sizeof(Set));
    snprintf(grown[sets->count].label, sizeof(grown[sets->count].label), "%s", label);
    return &grown[sets->count++];
}

/* The index of the file in the set, added when it is not there. -1 when the set is full. */
static int file_in_set(Set *set, const char *path) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->files[i], path) == 0) {
            return (int)i;
        }
    }
    if (set->count == sizeof(set->files) / sizeof(set->files[0])) {
        return -1;
    }
    set->files[set->count] = strdup(path);
    return set->files[set->count] ? (int)set->count++ : -1;
}

static const char *verdict_of(double a, double b, double rating_a, double rating_b) {
    if (rating_a == rating_b) {
        return "unrated";
    }
    if (a == b) {
        return "tie";
    }
    return (a > b) == (rating_a > rating_b) ? "match" : "miss";
}

/* A theme sample's own score (today's ranking), or false when the file is not a theme. */
static bool today_score(const char *path, double *score) {
    cJSON *document = NULL;
    char *text = NULL;
    bool found = false;
    if (cli_read_file(path, &text, NULL)) {
        document = cJSON_Parse(text);
        free(text);
        const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
        const cJSON *ratio = cJSON_GetObjectItemCaseSensitive(theme, "score");
        found = cJSON_IsObject(ratio);
        *score = found ? ratio_value(ratio) : 0;
    }
    cJSON_Delete(document);
    return found;
}

static void free_sets(Sets *sets) {
    for (size_t i = 0; i < sets->count; i++) {
        for (size_t f = 0; f < sets->sets[i].count; f++) {
            free(sets->sets[i].files[f]);
        }
    }
    free(sets->sets);
    memset(sets, 0, sizeof(*sets));
}

typedef struct {
    int match, miss, tie, total;
} Tally;

static void count(Tally *tally, const char *verdict) {
    if (strcmp(verdict, "unrated") == 0) {
        return;
    }
    tally->total++;
    tally->match += strcmp(verdict, "match") == 0;
    tally->miss += strcmp(verdict, "miss") == 0;
    tally->tie += strcmp(verdict, "tie") == 0;
}

static int run_rated(const char *rated_path, const cJSON *settings, bool json) {
    cJSON *rated = NULL;
    int status = read_json(rated_path, "rated set", &rated);
    if (status != 0) {
        return status;
    }
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(rated, "schema");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(rated, "schema_version");
    const cJSON *pairs = cJSON_GetObjectItemCaseSensitive(rated, "pairs");
    if (!cJSON_IsString(schema) || strcmp(schema->valuestring, "ursatz-rated-set") != 0 || !cJSON_IsNumber(version) || version->valuedouble != 1 ||
        !cJSON_IsArray(pairs)) {
        cJSON_Delete(rated);
        return cli_fail(CLI_EXIT_USAGE, "'%s' is not an ursatz-rated-set of version 1", rated_path);
    }
    Sets sets;
    memset(&sets, 0, sizeof(sets));
    const cJSON *pair;
    status = 0;
    /* the files of each set, in the order the pairs name them */
    cJSON_ArrayForEach(pair, pairs) {
        for (int side = 0; side < 2 && status == 0; side++) {
            const cJSON *file = cJSON_GetObjectItemCaseSensitive(pair, side == 0 ? "a" : "b");
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(pair, side == 0 ? "set_a" : "set_b");
            Set *set = cJSON_IsString(label) ? set_named(&sets, label->valuestring) : NULL;
            char *path = cJSON_IsString(file) ? join_path(rated_path, file->valuestring) : NULL;
            if (!set || !path || file_in_set(set, path) < 0) {
                status = cli_fail(CLI_EXIT_USAGE, "'%s': every pair needs a, b, set_a and set_b, and a set holds at most %d files", rated_path, MAX_FILES * 4);
            }
            free(path);
        }
    }
    /* the critic's score of every file, one request for each set */
    for (size_t s = 0; status == 0 && s < sets.count; s++) {
        Set *set = &sets.sets[s];
        for (size_t start = 0; status == 0 && start < set->count; start += MAX_FILES) {
            /* a set larger than one request holds is judged in runs of 16 */
            size_t run = set->count - start < MAX_FILES ? set->count - start : MAX_FILES;
            cJSON *answer = NULL;
            status = ask((const char *const *)&set->files[start], run, settings, &answer);
            size_t i = 0;
            const cJSON *candidate;
            cJSON_ArrayForEach(candidate, cJSON_GetObjectItemCaseSensitive(answer, "candidates")) {
                set->score[start + i++] = ratio_value(cJSON_GetObjectItemCaseSensitive(candidate, "score"));
            }
            cJSON_Delete(answer);
        }
    }
    Tally critic = {0, 0, 0, 0};
    Tally today = {0, 0, 0, 0};
    cJSON_ArrayForEach(pair, pairs) {
        if (status != 0) {
            break;
        }
        const char *names[2] = {NULL, NULL};
        double scores[2] = {0, 0};
        double today_scores[2] = {0, 0};
        bool theme[2] = {false, false};
        for (int side = 0; side < 2; side++) {
            const cJSON *file = cJSON_GetObjectItemCaseSensitive(pair, side == 0 ? "a" : "b");
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(pair, side == 0 ? "set_a" : "set_b");
            char *path = join_path(rated_path, file->valuestring);
            Set *set = set_named(&sets, label->valuestring);
            int at = path && set ? file_in_set(set, path) : -1;
            scores[side] = at >= 0 ? set->score[at] : 0;
            theme[side] = path && today_score(path, &today_scores[side]);
            free(path);
            names[side] = file->valuestring;
        }
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(pair, "label");
        double rating_a = ratio_value(NULL), rating_b = 0;
        const cJSON *ra = cJSON_GetObjectItemCaseSensitive(pair, "rating_a");
        const cJSON *rb = cJSON_GetObjectItemCaseSensitive(pair, "rating_b");
        rating_a = cJSON_IsNumber(ra) ? ra->valuedouble : 0;
        rating_b = cJSON_IsNumber(rb) ? rb->valuedouble : 0;
        const char *critic_verdict = verdict_of(scores[0], scores[1], rating_a, rating_b);
        const char *today_verdict = theme[0] && theme[1] ? verdict_of(today_scores[0], today_scores[1], rating_a, rating_b)
                                                         : verdict_of(0, 0, rating_a, rating_b);
        count(&critic, critic_verdict);
        count(&today, today_verdict);
        (void)names;
        if (!json) {
            printf("%-8s %.1f %.1f  critic %6.3f %6.3f %-7s today %-7s\n", cJSON_IsString(label) ? label->valuestring : "", rating_a, rating_b, scores[0],
                   scores[1], critic_verdict, today_verdict);
        }
    }
    if (status == 0) {
        printf("critic: match %d  miss %d  tie %d  of %d\n", critic.match, critic.miss, critic.tie, critic.total);
        printf("today: match %d  miss %d  tie %d  of %d\n", today.match, today.miss, today.tie, today.total);
    }
    free_sets(&sets);
    cJSON_Delete(rated);
    return status;
}

/* ------------------------------------------------------------------------------------------------ the command */

int run_critique(const CliArgs *args) {
    const char *rated = cli_value(args, "--rated");
    const char *settings_path = cli_value(args, "--settings");
    const char *format = cli_value(args, "--format");
    bool json = false;
    if (format) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
    }
    cJSON *settings = NULL;
    if (settings_path) {
        int status = read_json(settings_path, "settings file", &settings);
        if (status != 0) {
            return status;
        }
    }
    int status;
    if (rated) {
        status = args->input_count == 0 ? run_rated(rated, settings, json) : cli_fail(CLI_EXIT_USAGE, "critique --rated takes no candidate files");
    } else {
        status = run_files(args, settings, json);
    }
    cJSON_Delete(settings);
    return status;
}
