#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

/*
 * The plans every form template makes (R-417): each template at each size in C major 4/4 and A minor 3/4, with an
 * invented motive, once without modulate and with modulate and seeds 1 to 5, serialized with plan_to_json (plan
 * metadata is empty here, so the text is stable) and compared by a 64-bit FNV-1a hash with
 * tests/fixtures/forms/template-plans.txt, one line each: form, bars, count, key, meter, modulate, seed, hash.
 *
 *   test_form_plans          compares every plan with the file
 *   test_form_plans record   writes the file
 *
 * Refactors of the form code keep this passing; only a change meant to move a plan records it again.
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
    FormTemplateKind kind;
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
        out[n++] = (Size){FORM_TEMPLATE_BINARY, BINARY[i], 0};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){FORM_TEMPLATE_ROUNDED_BINARY, ROUNDED[i], 0};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){FORM_TEMPLATE_TERNARY, TERNARY[i], 0};
    }
    for (size_t count = 1; count <= 8; count++) {
        out[n++] = (Size){FORM_TEMPLATE_VARIATIONS, 0, count};
    }
    for (size_t i = 0; i < 2; i++) {
        out[n++] = (Size){FORM_TEMPLATE_LOOP, LOOP[i], 0};
    }
    for (size_t count = 2; count <= 4; count++) {
        out[n++] = (Size){FORM_TEMPLATE_LAYERED, 0, count};
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

/* The hash of the plan the request makes. */
static uint64_t plan_hash(const Size *size, const Scenario *scenario, bool modulate, unsigned long seed) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form_template_name(size->kind);
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
        fprintf(stderr, "%s %zu %zu %s modulate %d seed %lu: %s\n", form_template_name(size->kind), size->bars,
                size->count, scenario->name, modulate, seed, piece_result_name(built));
        exit(1);
    }
    PlanJsonError error;
    char *json = plan_to_json(plan, &error);
    if (!json) {
        fprintf(stderr, "plan_to_json: %s\n", error.message);
        exit(1);
    }
    uint64_t hash = fnv1a(json);
    free(json);
    plan_destroy(plan);
    return hash;
}

static void line(char *out, size_t cap, const Size *size, const Scenario *scenario, bool modulate, unsigned long seed,
                 uint64_t hash) {
    snprintf(out, cap, "%s %zu %zu %s %d/4 %d %lu %016llx", form_template_name(size->kind), size->bars, size->count,
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

int main(int argc, char **argv) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/template-plans.txt", COMPOSER_FIXTURE_DIR);
    if (argc > 1 && strcmp(argv[1], "record") == 0) {
        FILE *file = fopen(path, "w");
        if (!file) {
            fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        every_plan(write_line, file);
        fclose(file);
        printf("recorded %s\n", path);
        return 0;
    }
    Compare compare = {fopen(path, "r"), 0, 0};
    if (!compare.file) {
        fprintf(stderr, "cannot read %s (run test_form_plans record)\n", path);
        return 2;
    }
    every_plan(compare_line, &compare);
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
    printf("%zu plans match\n", compare.index);
    return 0;
}
