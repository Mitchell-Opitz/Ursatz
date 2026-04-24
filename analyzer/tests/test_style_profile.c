#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "identity/entity_id.h"
#include "theory/style_model_build.h"
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/style_labels.h>
#include <ursatz_analyzer/style_profile.h>

/*
 * Style labels and profiles over the corpora CMake builds in the build tree: "reference" (the five framework-v1
 * pieces), "chorales" (ten Bach chorales) and "sonatinas" (Clementi 36 and Mozart 545). Checks the labels on hand
 * cases, the reference profile against tests/snapshots/style-model.reference.json (training corpus masked), the file
 * round trip, the distances, and the refusals. With UPDATE_SNAPSHOTS=1 in the environment, the snapshot is rewritten
 * instead. The distance between the chorales and the sonatinas is printed; docs/style-profile.md records it.
 */

static int failures = 0;

#define CHECK(condition, ...)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL line %d: ", __LINE__);                                                               \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, "\n");                                                                                     \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *data = malloc((size_t)size + 1);
    if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        data = NULL;
    }
    fclose(file);
    if (data) {
        data[size] = '\0';
    }
    return data;
}

static bool write_file(const char *path, const char *data) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    bool ok = fputs(data, file) >= 0;
    return fclose(file) == 0 && ok;
}

static void make_dir(const char *path) {
    mkdir(path, 0777);
}

/* ---- labels ---- */

static void check_numeral(const char *input, const char *expected) {
    char *label = style_label_numeral(input);
    CHECK((label == NULL) == (expected == NULL) && (!label || strcmp(label, expected) == 0), "numeral '%s' gives '%s', expected '%s'",
          input, label ? label : "(none)", expected ? expected : "(none)");
    free(label);
}

static void check_interval(int semitones, const char *expected) {
    char label[16];
    style_label_interval(semitones, label, sizeof(label));
    CHECK(strcmp(label, expected) == 0, "interval %d gives '%s', expected '%s'", semitones, label, expected);
}

static void check_duration(unsigned long numerator, unsigned long denominator, const char *expected) {
    char label[48];
    style_label_duration(numerator, denominator, label, sizeof(label));
    CHECK(strcmp(label, expected) == 0, "duration %lu/%lu gives '%s', expected '%s'", numerator, denominator, label, expected);
}

static void test_labels(void) {
    check_numeral("V", "V");
    check_numeral("V7", "V7");
    check_numeral("V6/5", "V7");
    check_numeral("V4/3", "V7");
    check_numeral("V4/2", "V7");
    check_numeral("I6/4", "I");
    check_numeral("I6", "I");
    check_numeral("ii6/5", "ii7");
    check_numeral("bVI", "bVI");
    check_numeral("viio6", "viio");
    check_numeral("V6/5/V", "V7/V");
    check_numeral("V/V", "V/V");
    check_numeral("", NULL);
    check_numeral("hello", NULL);
    check_numeral("V/", NULL);
    check_numeral("V/x", NULL);
    check_numeral("6/4", NULL);

    check_interval(0, "0");
    check_interval(2, "+2");
    check_interval(-5, "-5");
    check_interval(12, "+12");
    check_interval(-12, "-12");
    check_interval(13, "+13+");
    check_interval(-24, "-13+");

    check_duration(1, 8, "1/8");
    check_duration(2, 16, "1/8");
    check_duration(3, 16, "3/16");
    check_duration(4, 4, "1/1");
    check_duration(1, 0, "0/1");

    CHECK(strcmp(style_label_contour(3), "up") == 0 && strcmp(style_label_contour(-1), "down") == 0 &&
              strcmp(style_label_contour(0), "same") == 0,
          "contour labels");

    const char *pair[] = {"ii", "V7", "I"};
    char *progression = style_label_progression(pair, 3);
    CHECK(progression && strcmp(progression, "ii-V7-I") == 0, "progression label '%s'", progression ? progression : "(none)");
    free(progression);
    CHECK(style_label_progression(pair, 0) == NULL, "empty progression has no label");

    StyleDimension dimension;
    CHECK(style_category_dimension("chord-progression-3", &dimension) && dimension == STYLE_DIMENSION_HARMONY, "harmony category");
    CHECK(style_category_dimension("syncopation-per-phrase", &dimension) && dimension == STYLE_DIMENSION_RHYTHM, "rhythm category");
    CHECK(style_category_dimension("phrase-range", &dimension) && dimension == STYLE_DIMENSION_MELODY, "melody category");
    CHECK(style_category_dimension("mode", &dimension) && dimension == STYLE_DIMENSION_FORM, "form category");
    CHECK(!style_category_dimension("tempo", &dimension), "unknown category");
    CHECK(style_dimension_from_name("melody", &dimension) && dimension == STYLE_DIMENSION_MELODY, "dimension from name");
    CHECK(!style_dimension_from_name("timbre", &dimension), "unknown dimension name");
}

/* ---- profiles ---- */

static char work_dir[512];

/* Analyzes <corpora>/<name> into <work>/<name> and profiles it. Returns NULL on failure. */
static StyleProfile *profile_of(const char *name, size_t expected_pieces) {
    char input[1024], output[1024];
    snprintf(input, sizeof(input), "%s/%s", URSATZ_CORPORA_DIR, name);
    snprintf(output, sizeof(output), "%s/%s", work_dir, name);
    CorpusRunOptions options = {input, output, false, false, NULL, NULL};
    CorpusRunSummary summary;
    CHECK(corpus_run(&options, &summary) == CORPUS_RUN_OK, "%s run failed", name);
    CHECK(summary.analyzed == expected_pieces && summary.failed == 0, "%s: %zu analyzed, %zu failed", name, summary.analyzed,
          summary.failed);
    StyleProfile *profile = style_profile_create();
    CHECK(profile && style_profile_add_corpus(profile, output) == STYLE_PROFILE_OK, "%s profile failed", name);
    CHECK(profile && style_profile_piece_count(profile) == expected_pieces, "%s profile counts %zu pieces", name,
          profile ? style_profile_piece_count(profile) : 0);
    return profile;
}

static StyleModel *model_of(StyleProfile *profile, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.style.json", work_dir, name);
    CHECK(style_model_file_write(profile, name, path) == STYLE_MODEL_FILE_OK, "cannot write %s", path);
    StyleModel *model = NULL;
    CHECK(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_OK, "cannot read %s", path);
    return model;
}

static const StyleStatistic *find_statistic(const StyleStatistic *list, size_t count, const char *category, const char *label) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i].category, category) == 0 && strcmp(list[i].label, label) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static void test_profile_statistics(StyleProfile *reference) {
    StyleStatistic *statistics;
    size_t count;
    CHECK(style_profile_statistics(reference, &statistics, &count) == STYLE_PROFILE_OK && count > 0, "no statistics");

    bool sorted = true;
    unsigned long sums[64] = {0};
    size_t category_index = 0, categories_seen = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            int c = strcmp(statistics[i - 1].category, statistics[i].category);
            sorted = sorted && (c < 0 || (c == 0 && strcmp(statistics[i - 1].label, statistics[i].label) < 0));
            if (c != 0) {
                category_index++;
            }
        }
        sums[category_index] += statistics[i].count;
        if (i == 0 || strcmp(statistics[i - 1].category, statistics[i].category) != 0) {
            categories_seen++;
        }
    }
    CHECK(sorted, "statistics are not sorted by category then label");
    category_index = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && strcmp(statistics[i - 1].category, statistics[i].category) != 0) {
            category_index++;
        }
        CHECK(statistics[i].total == sums[category_index], "%s total %lu is not the sum %lu", statistics[i].category,
              statistics[i].total, sums[category_index]);
    }
    CHECK(categories_seen == 15, "%zu categories, expected 15", categories_seen);

    /* Hand-checked against the pieces: the five reference pieces each have a home key and a meter. */
    const StyleStatistic *major = find_statistic(statistics, count, "mode", "major");
    const StyleStatistic *minor = find_statistic(statistics, count, "mode", "minor");
    unsigned long modes = (major ? major->count : 0) + (minor ? minor->count : 0);
    CHECK((major || minor) && modes == 5 && (major ? major->total : minor->total) == 5, "mode counts one per piece");
    CHECK(find_statistic(statistics, count, "chord-numeral", "V7") != NULL, "V7 is a chord numeral");
    CHECK(find_statistic(statistics, count, "chord-numeral", "V6/5") == NULL, "no inversion figure in a numeral label");
    CHECK(find_statistic(statistics, count, "cadence-type", "perfect-authentic") != NULL, "perfect authentic cadences");
    CHECK(find_statistic(statistics, count, "melodic-interval", "+2") != NULL, "an ascending second");
    CHECK(find_statistic(statistics, count, "contour", "up") != NULL && find_statistic(statistics, count, "contour", "down"),
          "up and down contours");
    free(statistics);
}

static char *masked_model(const char *path) {
    char *text = read_file(path);
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        return NULL;
    }
    cJSON_ReplaceItemInObjectCaseSensitive(root, "training_corpus", cJSON_CreateString("reference"));
    char *printed = cJSON_Print(root);
    cJSON_Delete(root);
    if (!printed) {
        return NULL;
    }
    char *with_newline = malloc(strlen(printed) + 2);
    sprintf(with_newline, "%s\n", printed);
    free(printed);
    return with_newline;
}

static void test_reference_snapshot(StyleProfile *reference) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/reference.style.json", work_dir);
    CHECK(style_model_file_write(reference, "reference", path) == STYLE_MODEL_FILE_OK, "cannot write the reference model");

    char *masked = masked_model(path);
    char snapshot_path[1024];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/style-model.reference.json", URSATZ_SNAPSHOT_DIR);
    const char *update = getenv("UPDATE_SNAPSHOTS");
    if (update && strcmp(update, "1") == 0) {
        CHECK(masked && write_file(snapshot_path, masked), "cannot write %s", snapshot_path);
    } else {
        char *expected = read_file(snapshot_path);
        CHECK(expected && masked && strcmp(expected, masked) == 0, "model differs from %s", snapshot_path);
        free(expected);
    }
    free(masked);

    /* The corpus label is the folder given to the run. */
    CHECK(strstr(style_profile_training_corpus(reference), "reference") != NULL, "training corpus '%s'",
          style_profile_training_corpus(reference));
}

static void test_round_trip(StyleProfile *reference) {
    char first[1024], second[1024];
    snprintf(first, sizeof(first), "%s/reference.style.json", work_dir);
    snprintf(second, sizeof(second), "%s/reference-again.style.json", work_dir);

    StyleModel *model = NULL;
    size_t pieces = 0;
    CHECK(style_model_file_read(first, &model, &pieces) == STYLE_MODEL_FILE_OK && model, "cannot read the reference model");
    CHECK(pieces == 5, "model counts %zu pieces", pieces);
    if (!model) {
        return;
    }

    StyleStatistic *from_profile, *from_model;
    size_t profile_count, model_count;
    CHECK(style_profile_statistics(reference, &from_profile, &profile_count) == STYLE_PROFILE_OK, "profile statistics");
    CHECK(style_model_statistics(model, &from_model, &model_count) == STYLE_MODEL_FILE_OK, "model statistics");
    CHECK(profile_count == model_count, "%zu statistics became %zu", profile_count, model_count);
    for (size_t i = 0; i < profile_count && i < model_count; i++) {
        CHECK(strcmp(from_profile[i].category, from_model[i].category) == 0 && strcmp(from_profile[i].label, from_model[i].label) == 0 &&
                  from_profile[i].count == from_model[i].count && from_profile[i].total == from_model[i].total,
              "statistic %zu differs after the round trip", i);
    }
    free(from_profile);
    style_statistics_free(from_model, model_count);

    /* The same profile written again gives the same bytes, and reading a file twice gives the same id. */
    CHECK(style_model_file_write(reference, "reference", second) == STYLE_MODEL_FILE_OK, "cannot write again");
    char *a = read_file(first), *b = read_file(second);
    CHECK(a && b && strcmp(a, b) == 0, "writing twice gives different bytes");
    free(a);
    free(b);
    StyleModel *again = NULL;
    CHECK(style_model_file_read(first, &again, NULL) == STYLE_MODEL_FILE_OK, "second read");
    if (again) {
        CHECK(entity_id_equals(style_model_id(model), style_model_id(again)), "the same file gave two ids");
        style_model_destroy(again);
    }
    CHECK(strcmp(style_model_style_name(model), "reference") == 0 && strcmp(style_model_model_version(model), PROJECT_VERSION_FOR_TESTS) == 0,
          "name and version");
    style_model_destroy(model);
}

static bool all_zero(const StyleComparison *comparison) {
    if (!comparison->overall_present || comparison->overall.numerator != 0) {
        return false;
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d] && comparison->dimension[d].numerator != 0) {
            return false;
        }
    }
    for (size_t i = 0; i < comparison->category_count; i++) {
        if (comparison->categories[i].distance.numerator != 0) {
            return false;
        }
    }
    return true;
}

static void test_self_comparison(StyleModel *reference) {
    StyleComparison comparison;
    CHECK(style_profile_compare(reference, reference, &comparison) == STYLE_COMPARE_OK, "self comparison failed");
    CHECK(all_zero(&comparison), "a model is not at distance 0 from itself");
    CHECK(comparison.category_count == 15, "%zu categories compared", comparison.category_count);
    style_comparison_destroy(&comparison);
}

static void test_chorales_and_sonatinas(void) {
    StyleProfile *chorale_profile = profile_of("chorales", 10);
    StyleProfile *sonatina_profile = profile_of("sonatinas", 2);
    StyleModel *chorales = chorale_profile ? model_of(chorale_profile, "chorales") : NULL;
    StyleModel *sonatinas = sonatina_profile ? model_of(sonatina_profile, "sonatinas") : NULL;
    CHECK(chorales && sonatinas, "models for the chorales and the sonatinas");
    if (chorales && sonatinas) {
        StyleComparison forward, backward;
        CHECK(style_profile_compare(chorales, sonatinas, &forward) == STYLE_COMPARE_OK, "chorales against sonatinas");
        CHECK(style_profile_compare(sonatinas, chorales, &backward) == STYLE_COMPARE_OK, "sonatinas against chorales");
        for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
            CHECK(forward.dimension_present[d] && forward.dimension[d].numerator > 0, "%s distance is 0",
                  style_dimension_name((StyleDimension)d));
            CHECK(forward.dimension[d].numerator == backward.dimension[d].numerator &&
                      forward.dimension[d].denominator == backward.dimension[d].denominator,
                  "%s distance is not symmetric", style_dimension_name((StyleDimension)d));
            printf("chorales vs sonatinas %-8s %.4f\n", style_dimension_name((StyleDimension)d),
                   style_rational_value(forward.dimension[d]));
        }
        CHECK(forward.overall_present && forward.overall.numerator > 0 && forward.overall.numerator <= forward.overall.denominator,
              "overall distance out of range");
        printf("chorales vs sonatinas overall  %.4f\n", style_rational_value(forward.overall));
        for (size_t i = 0; i < forward.category_count; i++) {
            const StyleCategoryDistance *c = &forward.categories[i];
            CHECK(c->distance.numerator <= c->distance.denominator, "%s distance above 1", c->category);
            printf("chorales vs sonatinas   %-24s %.4f\n", c->category, style_rational_value(c->distance));
        }
        style_comparison_destroy(&forward);
        style_comparison_destroy(&backward);
    }
    style_model_destroy(chorales);
    style_model_destroy(sonatinas);
    style_profile_destroy(chorale_profile);
    style_profile_destroy(sonatina_profile);
}

/* ---- hand-built models ---- */

static StyleModel *build_model(const char *name, const StyleStatistic *statistics, size_t count) {
    EntityID *id = NULL;
    StyleModel *model = NULL;
    if (entity_id_create(name, &id) != ENTITY_ID_OK) {
        return NULL;
    }
    if (style_model_from_statistics(id, name, "hand", "test", statistics, count, &model) != STYLE_MODEL_BUILD_OK) {
        entity_id_destroy(id);
        return NULL;
    }
    return model;
}

static void test_hand_distances(void) {
    const StyleStatistic a_stats[] = {{"cadence-type", "half", 3, 4}, {"cadence-type", "perfect-authentic", 1, 4}};
    const StyleStatistic b_stats[] = {{"cadence-type", "half", 1, 4},
                                      {"cadence-type", "perfect-authentic", 3, 4},
                                      {"mode", "major", 1, 1}};
    StyleModel *a = build_model("a", a_stats, 2);
    StyleModel *b = build_model("b", b_stats, 3);
    CHECK(a && b, "cannot build the hand models");
    if (a && b) {
        StyleComparison comparison;
        CHECK(style_profile_compare(a, b, &comparison) == STYLE_COMPARE_OK, "hand comparison");
        const StyleCategoryDistance *cadence = NULL, *mode = NULL;
        for (size_t i = 0; i < comparison.category_count; i++) {
            if (strcmp(comparison.categories[i].category, "cadence-type") == 0) {
                cadence = &comparison.categories[i];
            } else if (strcmp(comparison.categories[i].category, "mode") == 0) {
                mode = &comparison.categories[i];
            }
        }
        CHECK(cadence && cadence->distance.numerator == 1 && cadence->distance.denominator == 2, "cadence distance");
        CHECK(cadence && cadence->in_a && cadence->in_b && strcmp(cadence->top_a[0], "half") == 0 &&
                  strcmp(cadence->top_b[0], "perfect-authentic") == 0 && strcmp(cadence->top_a[1], "perfect-authentic") == 0 &&
                  cadence->top_a[2] == NULL,
              "top labels");
        CHECK(mode && mode->distance.numerator == 1 && mode->distance.denominator == 1 && !mode->in_a && mode->in_b,
              "a category in one model only is distance 1");
        /* harmony: cadence-type only, 1/2. form: mode only, 1. Rhythm and melody have no categories. */
        CHECK(comparison.dimension_present[STYLE_DIMENSION_HARMONY] && comparison.dimension[STYLE_DIMENSION_HARMONY].numerator == 1 &&
                  comparison.dimension[STYLE_DIMENSION_HARMONY].denominator == 2,
              "harmony dimension");
        CHECK(comparison.dimension_present[STYLE_DIMENSION_FORM] && comparison.dimension[STYLE_DIMENSION_FORM].numerator == 1 &&
                  comparison.dimension[STYLE_DIMENSION_FORM].denominator == 1,
              "form dimension");
        CHECK(!comparison.dimension_present[STYLE_DIMENSION_RHYTHM] && !comparison.dimension_present[STYLE_DIMENSION_MELODY],
              "empty dimensions are left out");
        CHECK(comparison.overall_present && comparison.overall.numerator == 3 && comparison.overall.denominator == 4,
              "overall distance");
        style_comparison_destroy(&comparison);
    }
    style_model_destroy(a);
    style_model_destroy(b);
}

static void test_overflow(void) {
    /* Totals so large that twice their product does not fit in 128 bits: an error, never a wrapped distance. */
    const unsigned long big = ~0UL;
    const StyleStatistic a_stats[] = {{"contour", "up", 1, big}};
    const StyleStatistic b_stats[] = {{"contour", "up", 1, big - 1}};
    StyleModel *a = build_model("big-a", a_stats, 1);
    StyleModel *b = build_model("big-b", b_stats, 1);
    CHECK(a && b, "cannot build the large models");
    if (a && b) {
        StyleComparison comparison;
        CHECK(style_profile_compare(a, b, &comparison) == STYLE_COMPARE_ERR_OVERFLOW, "an overflowing comparison is refused");
        CHECK(comparison.category_count == 0 && comparison.categories == NULL, "nothing is left behind after an overflow");
    }
    style_model_destroy(a);
    style_model_destroy(b);
}

/* ---- refusals ---- */

static void test_refusals(void) {
    StyleProfile *profile = style_profile_create();
    CHECK(profile != NULL, "cannot create a profile");
    CHECK(style_profile_add_document_json(profile, "{\"schema_version\": 8}") == STYLE_PROFILE_ERR_FORMAT, "version 8 is refused");
    CHECK(style_profile_add_document_json(profile, "{}") == STYLE_PROFILE_ERR_FORMAT, "a document without a version is refused");
    CHECK(style_profile_add_document_json(profile, "not json") == STYLE_PROFILE_ERR_FORMAT, "text is refused");
    CHECK(style_profile_add_document_json(profile, "{\"schema_version\": 9}") == STYLE_PROFILE_OK &&
              style_profile_piece_count(profile) == 1,
          "an empty version 9 document counts as a piece with no labels");
    CHECK(style_profile_add_corpus(profile, "/nonexistent-corpus") == STYLE_PROFILE_ERR_IO, "a missing corpus is an I/O error");
    CHECK(style_profile_add_document_json(NULL, "{}") == STYLE_PROFILE_ERR_ARGUMENT, "no profile");
    style_profile_destroy(profile);

    char path[1024];
    snprintf(path, sizeof(path), "%s/refused.style.json", work_dir);
    const char *refused[] = {
        /* an unknown category */
        "{\"schema\": \"ursatz-style-model\", \"schema_version\": 1, \"style_name\": \"x\", \"training_corpus\": \"c\", "
        "\"model_version\": \"1\", \"pieces\": 1, \"statistics\": [{\"dimension\": \"harmony\", \"category\": \"tempo\", "
        "\"label\": \"fast\", \"count\": 1, \"total\": 1}]}",
        /* an unknown dimension */
        "{\"schema\": \"ursatz-style-model\", \"schema_version\": 1, \"style_name\": \"x\", \"training_corpus\": \"c\", "
        "\"model_version\": \"1\", \"pieces\": 1, \"statistics\": [{\"dimension\": \"timbre\", \"category\": \"mode\", "
        "\"label\": \"major\", \"count\": 1, \"total\": 1}]}",
        /* a category under the wrong dimension */
        "{\"schema\": \"ursatz-style-model\", \"schema_version\": 1, \"style_name\": \"x\", \"training_corpus\": \"c\", "
        "\"model_version\": \"1\", \"pieces\": 1, \"statistics\": [{\"dimension\": \"melody\", \"category\": \"mode\", "
        "\"label\": \"major\", \"count\": 1, \"total\": 1}]}",
        /* a total that is not the sum of the counts */
        "{\"schema\": \"ursatz-style-model\", \"schema_version\": 1, \"style_name\": \"x\", \"training_corpus\": \"c\", "
        "\"model_version\": \"1\", \"pieces\": 2, \"statistics\": [{\"dimension\": \"form\", \"category\": \"mode\", "
        "\"label\": \"major\", \"count\": 1, \"total\": 2}]}",
        /* another schema */
        "{\"schema\": \"ursatz-corpus-index\", \"schema_version\": 1}",
        /* another version */
        "{\"schema\": \"ursatz-style-model\", \"schema_version\": 2, \"style_name\": \"x\", \"training_corpus\": \"c\", "
        "\"model_version\": \"1\", \"pieces\": 1, \"statistics\": []}",
        "not json",
    };
    for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
        StyleModel *model = NULL;
        CHECK(write_file(path, refused[i]), "cannot write %s", path);
        CHECK(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_ERR_FORMAT && model == NULL, "refused file %zu was read", i);
        style_model_destroy(model);
    }
    StyleModel *model = NULL;
    CHECK(style_model_file_read("/nonexistent.style.json", &model, NULL) == STYLE_MODEL_FILE_ERR_IO, "a missing file is an I/O error");

    /* A profile with nothing in it still writes and reads. */
    StyleProfile *empty = style_profile_create();
    CHECK(style_model_file_write(empty, "empty", path) == STYLE_MODEL_FILE_OK, "empty write");
    CHECK(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_OK && model, "empty read");
    style_model_destroy(model);
    CHECK(style_model_file_write(empty, "", path) == STYLE_MODEL_FILE_ERR_ARGUMENT, "an unnamed style is refused");
    style_profile_destroy(empty);
}

int main(void) {
    snprintf(work_dir, sizeof(work_dir), "%s", URSATZ_STYLE_WORK_DIR);
    char command[1100];
    snprintf(command, sizeof(command), "rm -rf '%s'", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot clear %s\n", work_dir);
    }
    make_dir(work_dir);

    test_labels();

    StyleProfile *reference = profile_of("reference", 5);
    if (reference) {
        test_profile_statistics(reference);
        test_reference_snapshot(reference);
        test_round_trip(reference);
        StyleModel *model = model_of(reference, "reference-self");
        if (model) {
            test_self_comparison(model);
            style_model_destroy(model);
        }
    }
    style_profile_destroy(reference);

    test_chorales_and_sonatinas();
    test_hand_distances();
    test_overflow();
    test_refusals();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_style_profile: ok\n");
    return 0;
}
