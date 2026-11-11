#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

#include "api/api_piece.h"
#include "support/test_support.h"

/*
 * The plans every form template makes (R-417): each template at each size in C major 4/4 and A minor 3/4, with an
 * invented motive, once without modulate and with modulate and seeds 1 to 5, serialized with plan_to_json (plan
 * metadata is empty here, so the text is stable) and compared by a 64-bit FNV-1a hash with
 * tests/fixtures/forms/template-plans.txt, one line each: form, bars, count, key, meter, modulate, seed, hash.
 *
 *   test_form_plans                 compares every plan with the file
 *   test_form_plans record          writes the file
 *   test_form_plans pieces          the same for the loop plans of the piece requests in tests/fixtures/piece
 *   test_form_plans record-pieces   writes tests/fixtures/forms/piece-plans.txt
 *
 * The pieces mode reads each request with api_piece_read_request and hashes the loop plan it builds, one line each:
 * the fixture's name and the hash. The ending plan needs a composed loop, so the snapshots (piece-loop-and-ending.*)
 * pin it. Refactors of the form code keep this passing; only a change meant to move a plan records it again.
 */

#define SEEDS 5

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C-major", 0, KEY_STRUCTURE_MODE_MAJOR, 4},
    {"A-minor", 9, KEY_STRUCTURE_MODE_MINOR, 3},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

/* A size of a template as the request gives it: bars, or the count of variations or layers. */
typedef struct {
    const char *kind;
    size_t bars;
    size_t count;
} Size;

static size_t sizes_of(Size *out) {
    size_t n = 0;
    static const size_t BINARY[] = {8, 16};
    static const size_t ROUNDED[] = {16, 24};
    static const size_t TERNARY[] = {12, 24};
    static const size_t LOOP[] = {8, 16};
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){"binary", BINARY[i], 0};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){"rounded-binary", ROUNDED[i], 0};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){"ternary", TERNARY[i], 0};
    }
    for (size_t count = 1; count <= 8; count++) {
        out[n++] = (Size){"variations", 0, count};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){"loop", LOOP[i], 0};
    }
    for (size_t count = 2; count <= 4; count++) {
        out[n++] = (Size){"layered", 0, count};
    }
    return n;
}
#define MAX_SIZES 24

static uint64_t fnv1a(const char *text) {
    uint64_t hash = 14695981039346656037ULL;
    for (; *text; text++) {
        hash = (hash ^ (unsigned char)*text) * 1099511628211ULL;
    }
    return hash;
}

/* The plan text records the pinned Ursatz commit (metadata.ursatz_commit); a pin bump must not move a plan, so the
 * 40 hex digits are overwritten with zeros before the hash. */
static uint64_t fnv1a_plan(char *json) {
    char *field = strstr(json, "ursatz_commit");
    if (field) {
        char *quote = strchr(field + strlen("ursatz_commit") + 1, '"');
        for (char *c = quote ? quote + 1 : NULL; c && *c && *c != '"'; c++) {
            *c = '0';
        }
    }
    return fnv1a(json);
}

/* The hash of the plan the request makes. */
static uint64_t plan_hash(const Size *size, const Scenario *scenario, bool modulate, unsigned long seed) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = size->kind;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = 4;
    request.bars = size->bars;
    request.count = size->count;
    request.modulate = modulate;
    request.seed = seed;
    PiecePlan *plan = NULL;
    PieceResult built = piece_build_plan(&request, &plan);
    if (built != PIECE_OK || !plan) {
        fprintf(stderr, "%s %zu %zu %s modulate %d seed %lu: %s\n", size->kind, size->bars,
                size->count, scenario->name, modulate, seed, piece_result_name(built));
        exit(1);
    }
    PlanJsonError error;
    char *json = plan_to_json(plan, &error);
    if (!json) {
        fprintf(stderr, "plan_to_json: %s\n", error.message);
        exit(1);
    }
    uint64_t hash = fnv1a_plan(json);
    free(json);
    plan_destroy(plan);
    return hash;
}

static void line(char *out, size_t cap, const Size *size, const Scenario *scenario, bool modulate, unsigned long seed,
                 uint64_t hash) {
    snprintf(out, cap, "%s %zu %zu %s %d/4 %d %lu %016llx", size->kind, size->bars, size->count,
             scenario->name, scenario->beats, modulate ? 1 : 0, seed, (unsigned long long)hash);
}

/* Calls each(line) for every plan in file order; stops at the first nonzero. */
typedef int (*Each)(const char *text, void *state);

static int every_plan(Each each, void *state) {
    Size sizes[MAX_SIZES];
    size_t count = sizes_of(sizes);
    for (size_t z = 0; z < count; z++) {
        for (size_t s = 0; s < SCENARIO_COUNT; s++) {
            for (unsigned long mode = 0; mode <= SEEDS; mode++) {
                bool modulate = mode != 0;
                unsigned long seed = mode;
                char text[256];
                line(text, sizeof(text), &sizes[z], &SCENARIOS[s], modulate, seed,
                     plan_hash(&sizes[z], &SCENARIOS[s], modulate, seed));
                int stop = each(text, state);
                if (stop) {
                    return stop;
                }
            }
        }
    }
    return 0;
}

static int write_line(const char *text, void *state) {
    fprintf((FILE *)state, "%s\n", text);
    return 0;
}

typedef struct {
    FILE *file;
    size_t index;
    size_t mismatches;
} Compare;

static int compare_line(const char *text, void *state) {
    Compare *compare = state;
    char expected[256];
    compare->index++;
    if (!fgets(expected, sizeof(expected), compare->file)) {
        fprintf(stderr, "plan %zu has no line in the file: %s\n", compare->index, text);
        compare->mismatches++;
        return 0;
    }
    expected[strcspn(expected, "\n")] = '\0';
    if (strcmp(expected, text) != 0) {
        fprintf(stderr, "plan %zu moved\n  file: %s\n  now:  %s\n", compare->index, expected, text);
        compare->mismatches++;
    }
    return 0;
}

/* --- the loop plans of the piece requests --------------------------------------------------------- */

static const char *const PIECE_FIXTURES[] = {"loop-and-ending.json", "progression-fill.json", "structure-only.json",
                                              "theme-fill.json"};
#define PIECE_FIXTURE_COUNT (sizeof(PIECE_FIXTURES) / sizeof(PIECE_FIXTURES[0]))
/* The style is read from the fixtures directory (the test changes to it), so the plan holds a path that is the same on
   every machine. */
#define STYLE_FILE "style-fast-minor.json"

static char *replace(char *text, const char *placeholder, const char *value) {
    char *at = strstr(text, placeholder);
    if (!at) {
        return text;
    }
    size_t length = strlen(text) - strlen(placeholder) + strlen(value) + 1;
    char *out = malloc(length);
    assert(out);
    snprintf(out, length, "%.*s%s%s", (int)(at - text), text, value, at + strlen(placeholder));
    free(text);
    return out;
}

/* The plan of the one-motive theme at seed 1: what a theme fill is given. */
static char *theme_plan(void) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddNumberToObject(request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    char *plan = strdup(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    return plan;
}

/* The hash of the loop plan one piece request fixture makes. */
static uint64_t piece_hash(const char *name, const char *theme) {
    char path[512];
    snprintf(path, sizeof(path), "%s/piece/%s", COMPOSER_FIXTURE_DIR, name);
    char *text = replace(test_read_file(path, NULL), "@STYLE@", STYLE_FILE);
    cJSON *root = cJSON_Parse(text);
    free(text);
    assert(root);
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(root, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
        if (cJSON_IsString(kind) && strcmp(kind->valuestring, "theme") == 0) {
            cJSON *inline_plan = cJSON_Parse(theme);
            assert(inline_plan && cJSON_ReplaceItemInObject(fill, "plan", inline_plan));
            assert(cJSON_AddStringToObject(fill, "source", "theme.plan.json"));
        }
    }
    PieceSpec *spec = calloc(1, sizeof(*spec));
    assert(spec);
    ComposerError error;
    memset(&error, 0, sizeof(error));
    if (api_piece_read_request(spec, root, "", NULL, false, NULL, &error) != COMPOSER_OK) {
        fprintf(stderr, "%s: %s (%s)\n", name, error.message, error.path);
        exit(1);
    }
    PiecePlan *plan = NULL;
    PieceResult built = piece_build_plan(&spec->request, &plan);
    if (built != PIECE_OK || !plan) {
        fprintf(stderr, "%s: %s\n", name, piece_result_name(built));
        exit(1);
    }
    PlanJsonError json_error;
    char *json = plan_to_json(plan, &json_error);
    if (!json) {
        fprintf(stderr, "plan_to_json: %s\n", json_error.message);
        exit(1);
    }
    uint64_t hash = fnv1a_plan(json);
    free(json);
    plan_destroy(plan);
    api_piece_request_destroy(spec);
    free(spec);
    cJSON_Delete(root);
    return hash;
}

/* Calls each(line) for every piece request fixture in order; stops at the first nonzero. */
static int every_piece(Each each, void *state) {
    if (chdir(COMPOSER_FIXTURE_DIR) != 0) {
        fprintf(stderr, "cannot enter %s\n", COMPOSER_FIXTURE_DIR);
        exit(2);
    }
    char *theme = theme_plan();
    int stop = 0;
    for (size_t i = 0; !stop && i < PIECE_FIXTURE_COUNT; i++) {
        char text[256];
        snprintf(text, sizeof(text), "%s %016llx", PIECE_FIXTURES[i], (unsigned long long)piece_hash(PIECE_FIXTURES[i], theme));
        stop = each(text, state);
    }
    free(theme);
    return stop;
}

/* Records or compares one file of lines made by `every`. */
static int run_file(const char *file, int (*every)(Each, void *), bool record, const char *what) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/%s", COMPOSER_FIXTURE_DIR, file);
    if (record) {
        FILE *out = fopen(path, "w");
        if (!out) {
            fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        every(write_line, out);
        fclose(out);
        printf("recorded %s\n", path);
        return 0;
    }
    Compare compare = {fopen(path, "r"), 0, 0};
    if (!compare.file) {
        fprintf(stderr, "cannot read %s (record it first)\n", path);
        return 2;
    }
    every(compare_line, &compare);
    char extra[256];
    if (fgets(extra, sizeof(extra), compare.file)) {
        fprintf(stderr, "the file has lines beyond the %zu plans\n", compare.index);
        compare.mismatches++;
    }
    fclose(compare.file);
    if (compare.mismatches) {
        fprintf(stderr, "%zu of %zu plans differ from %s\n", compare.mismatches, compare.index, path);
        return 1;
    }
    printf("%zu %s match\n", compare.index, what);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "pieces") == 0 || strcmp(mode, "record-pieces") == 0) {
        return run_file("piece-plans.txt", every_piece, mode[0] == 'r', "piece plans");
    }
    return run_file("template-plans.txt", every_plan, strcmp(mode, "record") == 0, "plans");
}
