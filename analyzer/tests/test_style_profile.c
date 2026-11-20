#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "model/identity/entity_id.h"
#include "theory/style/style_model_build.h"
#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/style_profile.h>
#include <ursatz_analyzer_internal/version.h>
#include "theory/style/style_labels.h"

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * Style labels and profiles over the corpora CMake builds in the build tree: "reference" (the five framework-v1
 * pieces), "chorales" (ten Bach chorales) and "sonatinas" (Clementi 36 and Mozart 545). Checks the labels on hand
 * cases, the reference profile against tests/snapshots/style-model.reference.json (training corpus masked), the file
 * round trip, the distances, and the refusals. With URSATZ_UPDATE_GOLDENS=1 in the environment, the snapshot is rewritten
 * instead. The distance between the chorales and the sonatinas is printed; docs/style-profile.md records it.
 */

static void make_dir(const char *path) {
    mkdir(path, 0777);
}

/* ---- profiles ---- */

static char work_dir[512];

/* Analyzes <corpora>/<name> into <work>/<name> and profiles it. Returns NULL on failure. */
static StyleProfile *profile_of(const char *name, size_t expected_pieces) {
    char input[1024], output[1024];
    snprintf(input, sizeof(input), "%s/%s", URSATZ_CORPORA_DIR, name);
    snprintf(output, sizeof(output), "%s/%s", work_dir, name);
    AnalyzerCorpusOptions options = {input, output, false, NULL, NULL, test_knowledge()};
    AnalyzerCorpusSummary summary;
    CHECKF(analyzer_corpus_run(&options, &summary, NULL) == URSATZ_OK, "%s run failed", name);
    CHECKF(summary.analyzed == expected_pieces && summary.failed == 0, "%s: %zu analyzed, %zu failed", name, summary.analyzed,
           summary.failed);
    StyleProfile *profile = style_profile_create();
    CHECKF(profile && analyzer_style_profile_add_corpus(profile, output) == STYLE_PROFILE_OK, "%s profile failed", name);
    CHECKF(profile && style_profile_piece_count(profile) == expected_pieces, "%s profile counts %zu pieces", name,
           profile ? style_profile_piece_count(profile) : 0);
    return profile;
}

static StyleModel *model_of(StyleProfile *profile, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.style.json", work_dir, name);
    CHECKF(style_model_file_write(profile, name, URSATZ_ANALYZER_VERSION, path) == STYLE_MODEL_FILE_OK, "cannot write %s", path);
    StyleModel *model = NULL;
    CHECKF(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_OK, "cannot read %s", path);
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
    CHECKF(style_profile_statistics(reference, &statistics, &count) == STYLE_PROFILE_OK && count > 0, "no statistics");

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
    CHECKF(sorted, "statistics are not sorted by category then label");
    category_index = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && strcmp(statistics[i - 1].category, statistics[i].category) != 0) {
            category_index++;
        }
        CHECKF(statistics[i].total == sums[category_index], "%s total %lu is not the sum %lu", statistics[i].category,
               statistics[i].total, sums[category_index]);
    }
    CHECKF(categories_seen == 21, "%zu categories, expected 21", categories_seen);

    /* Hand-checked against the pieces: the five reference pieces each have a home key and a meter. */
    const StyleStatistic *major = find_statistic(statistics, count, "mode", "major");
    const StyleStatistic *minor = find_statistic(statistics, count, "mode", "minor");
    unsigned long modes = (major ? major->count : 0) + (minor ? minor->count : 0);
    CHECKF((major || minor) && modes == 5 && (major ? major->total : minor->total) == 5, "mode counts one per piece");
    CHECKF(find_statistic(statistics, count, "chord-numeral", "V7") != NULL, "V7 is a chord numeral");
    CHECKF(find_statistic(statistics, count, "chord-numeral", "V6/5") == NULL, "no inversion figure in a numeral label");
    CHECKF(find_statistic(statistics, count, "cadence-type", "perfect-authentic") != NULL, "perfect authentic cadences");
    CHECKF(find_statistic(statistics, count, "melodic-interval", "+2") != NULL, "an ascending second");
    CHECKF(find_statistic(statistics, count, "contour", "up") != NULL && find_statistic(statistics, count, "contour", "down"),
           "up and down contours");
    free(statistics);
}

static char *masked_model(const char *path) {
    char *text = test_read_file(path, NULL);
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
    CHECKF(style_model_file_write(reference, "reference", URSATZ_ANALYZER_VERSION, path) == STYLE_MODEL_FILE_OK, "cannot write the reference model");

    char *masked = masked_model(path);
    char snapshot_path[1024];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/style-model.reference.json", URSATZ_SNAPSHOT_DIR);
    if (test_update_goldens()) {
        CHECKF(masked && test_write_file(snapshot_path, masked, strlen(masked)), "cannot write %s", snapshot_path);
    } else {
        char *expected = test_read_file(snapshot_path, NULL);
        CHECKF(expected && masked && strcmp(expected, masked) == 0, "model differs from %s", snapshot_path);
        free(expected);
    }
    free(masked);

    /* The corpus label is the folder given to the run. */
    CHECKF(strstr(style_profile_training_corpus(reference), "reference") != NULL, "training corpus '%s'",
           style_profile_training_corpus(reference));
}

static void test_round_trip(StyleProfile *reference) {
    char first[1024], second[1024];
    snprintf(first, sizeof(first), "%s/reference.style.json", work_dir);
    snprintf(second, sizeof(second), "%s/reference-again.style.json", work_dir);

    StyleModel *model = NULL;
    size_t pieces = 0;
    CHECKF(style_model_file_read(first, &model, &pieces) == STYLE_MODEL_FILE_OK && model, "cannot read the reference model");
    CHECKF(pieces == 5, "model counts %zu pieces", pieces);
    if (!model) {
        return;
    }

    StyleStatistic *from_profile, *from_model;
    size_t profile_count, model_count;
    CHECKF(style_profile_statistics(reference, &from_profile, &profile_count) == STYLE_PROFILE_OK, "profile statistics");
    CHECKF(style_model_statistics(model, &from_model, &model_count) == STYLE_MODEL_FILE_OK, "model statistics");
    CHECKF(profile_count == model_count, "%zu statistics became %zu", profile_count, model_count);
    for (size_t i = 0; i < profile_count && i < model_count; i++) {
        CHECKF(strcmp(from_profile[i].category, from_model[i].category) == 0 && strcmp(from_profile[i].label, from_model[i].label) == 0 &&
                   from_profile[i].count == from_model[i].count && from_profile[i].total == from_model[i].total,
               "statistic %zu differs after the round trip", i);
    }
    free(from_profile);
    style_statistics_free(from_model, model_count);

    /* The same profile written again gives the same bytes, and reading a file twice gives the same name. */
    CHECKF(style_model_file_write(reference, "reference", URSATZ_ANALYZER_VERSION, second) == STYLE_MODEL_FILE_OK, "cannot write again");
    char *a = test_read_file(first, NULL), *b = test_read_file(second, NULL);
    CHECKF(a && b && strcmp(a, b) == 0, "writing twice gives different bytes");
    free(a);
    free(b);
    StyleModel *again = NULL;
    CHECKF(style_model_file_read(first, &again, NULL) == STYLE_MODEL_FILE_OK, "second read");
    if (again) {
        CHECKF(strcmp(style_model_style_name(again), style_model_style_name(model)) == 0, "the same file gave two names");
        style_model_destroy(again);
    }
    CHECKF(strcmp(style_model_style_name(model), "reference") == 0 && strcmp(style_model_model_version(model), PROJECT_VERSION_FOR_TESTS) == 0,
           "name and version");
    style_model_destroy(model);
}

static bool all_zero(const StyleComparison *comparison) {
    if (!comparison->overall_present || comparison->overall != 0.0) {
        return false;
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d] && comparison->dimension[d] != 0.0) {
            return false;
        }
    }
    for (size_t i = 0; i < comparison->category_count; i++) {
        if (comparison->categories[i].distance.num != 0) {
            return false;
        }
    }
    return true;
}

static void test_self_comparison(StyleModel *reference) {
    StyleComparison comparison;
    CHECKF(style_profile_compare(reference, reference, &comparison) == STYLE_COMPARE_OK, "self comparison failed");
    CHECKF(all_zero(&comparison), "a model is not at distance 0 from itself");
    CHECKF(comparison.category_count == 21, "%zu categories compared", comparison.category_count);
    style_comparison_destroy(&comparison);
}

/* The sonatinas are MIDI files with a set-tempo event at tick 0 (Clementi 36 at 120 to 139, Mozart 545 at 160 to 179
   quarter notes per minute): one tempo label each. Both corpora count a note density per phrase. */
static void check_tempo_and_density(const StyleProfile *chorale_profile, const StyleProfile *sonatina_profile) {
    const StyleProfile *profiles[2] = {chorale_profile, sonatina_profile};
    for (int p = 0; p < 2; p++) {
        StyleStatistic *statistics = NULL;
        size_t count = 0;
        if (!profiles[p] || style_profile_statistics(profiles[p], &statistics, &count) != STYLE_PROFILE_OK) {
            CHECKF(false, "no statistics for profile %d", p);
            continue;
        }
        size_t density_labels = 0;
        size_t tempo_labels = 0;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(statistics[i].category, "note-density") == 0) {
                density_labels++;
                printf("profile %d note-density %s x%lu\n", p, statistics[i].label, statistics[i].count);
            } else if (strcmp(statistics[i].category, "tempo") == 0) {
                tempo_labels++;
                printf("profile %d tempo %s x%lu\n", p, statistics[i].label, statistics[i].count);
                CHECKF(strchr(statistics[i].label, '-') != NULL || strchr(statistics[i].label, '<') != NULL ||
                           strchr(statistics[i].label, '+') != NULL,
                       "tempo label '%s'", statistics[i].label);
            }
        }
        CHECKF(density_labels > 0, "profile %d has no note-density labels", p);
        if (p == 1) {
            CHECKF(tempo_labels == 2 && find_statistic(statistics, count, "tempo", "120-139") != NULL &&
                       find_statistic(statistics, count, "tempo", "160-179") != NULL,
                   "the sonatinas' MIDI tempos are not read");
        }
        free(statistics);
    }
}

static void test_chorales_and_sonatinas(void) {
    StyleProfile *chorale_profile = profile_of("chorales", 10);
    StyleProfile *sonatina_profile = profile_of("sonatinas", 2);
    StyleModel *chorales = chorale_profile ? model_of(chorale_profile, "chorales") : NULL;
    StyleModel *sonatinas = sonatina_profile ? model_of(sonatina_profile, "sonatinas") : NULL;
    CHECKF(chorales && sonatinas, "models for the chorales and the sonatinas");
    if (chorales && sonatinas) {
        StyleComparison forward, backward;
        CHECKF(style_profile_compare(chorales, sonatinas, &forward) == STYLE_COMPARE_OK, "chorales against sonatinas");
        CHECKF(style_profile_compare(sonatinas, chorales, &backward) == STYLE_COMPARE_OK, "sonatinas against chorales");
        for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
            CHECKF(forward.dimension_present[d] && forward.dimension[d] > 0.0, "%s distance is 0",
                   style_dimension_name((StyleDimension)d));
            CHECKF(forward.dimension[d] == backward.dimension[d],
                   "%s distance is not symmetric", style_dimension_name((StyleDimension)d));
            printf("chorales vs sonatinas %-8s %.4f\n", style_dimension_name((StyleDimension)d),
                   forward.dimension[d]);
        }
        CHECKF(forward.overall_present && forward.overall > 0.0 && forward.overall <= 1.0,
               "overall distance out of range");
        printf("chorales vs sonatinas overall  %.4f\n", forward.overall);
        for (size_t i = 0; i < forward.category_count; i++) {
            const StyleCategoryDistance *c = &forward.categories[i];
            CHECKF(c->distance.num <= c->distance.den, "%s distance above 1", c->category);
            printf("chorales vs sonatinas   %-24s %.4f\n", c->category, rational_to_double(c->distance));
        }
        style_comparison_destroy(&forward);
        style_comparison_destroy(&backward);
    }
    check_tempo_and_density(chorale_profile, sonatina_profile);
    style_model_destroy(chorales);
    style_model_destroy(sonatinas);
    style_profile_destroy(chorale_profile);
    style_profile_destroy(sonatina_profile);
}

/* ---- motive categories ---- */

static unsigned long label_count(const StyleStatistic *list, size_t count, const char *category, const char *label) {
    const StyleStatistic *found = find_statistic(list, count, category, label);
    return found ? found->count : 0;
}

static size_t category_size(const StyleStatistic *list, size_t count, const char *category) {
    size_t size = 0;
    for (size_t i = 0; i < count; i++) {
        size += strcmp(list[i].category, category) == 0 ? 1 : 0;
    }
    return size;
}

/* A document with features: two families with the same degrees, intervals and rhythm, one with a zero interval, and a
   family without features, which adds nothing. Note 3 is an appoggiatura in two occurrences (counted once), note 9 a
   passing tone, note 7 an unclassified tone (no figure). */
static void test_motive_categories(void) {
    const char *document =
        "{\"schema_version\": 13,"
        " \"non_chord_tones\": [{\"note_ids\": [3], \"type\": \"appoggiatura\"}, {\"note_ids\": [7], \"type\": null},"
        " {\"note_ids\": [9], \"type\": \"passing\"}],"
        " \"motives\": ["
        "{\"features\": {\"degrees\": [\"1\", \"b6\", \"5\"], \"intervals\": [8, -1], \"rhythm\": [\"1/8\", \"1/8\", \"1/4\"]},"
        " \"occurrences\": [{\"note_ids\": [2, 3, 4], \"degrees\": [\"1\", \"b6\", \"5\"]},"
        " {\"note_ids\": [8, 9, 10], \"degrees\": [\"2\", \"3\", \"4\"]}]},"
        "{\"features\": {\"degrees\": [\"1\", \"b6\", \"5\"], \"intervals\": [8, -1], \"rhythm\": [\"1/8\", \"1/8\", \"1/4\"]},"
        " \"occurrences\": [{\"note_ids\": [2, 3, 4], \"degrees\": [\"1\", \"b6\", \"5\"]}]},"
        "{\"features\": {\"degrees\": [\"1\", \"1\", \"2\"], \"intervals\": [0, 2], \"rhythm\": [\"1/4\", \"1/4\", \"1/2\"]},"
        " \"occurrences\": [{\"note_ids\": [5, 6, 7], \"degrees\": [\"1\", \"1\", \"2\"]}]},"
        "{\"occurrences\": [{\"note_ids\": [3, 4, 5], \"degrees\": [\"1\", \"2\", \"3\"]}]}"
        "]}";
    StyleProfile *profile = style_profile_create();
    CHECKF(profile && analyzer_style_add_document_json(profile, document) == STYLE_PROFILE_OK, "motive document");
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    if (!profile || style_profile_statistics(profile, &statistics, &count) != STYLE_PROFILE_OK) {
        style_profile_destroy(profile);
        return;
    }
    CHECKF(label_count(statistics, count, "motive-degrees", "1 b6 5") == 2, "motive degrees '1 b6 5' twice");
    CHECKF(label_count(statistics, count, "motive-degrees", "1 1 2") == 1, "motive degrees '1 1 2'");
    CHECKF(category_size(statistics, count, "motive-degrees") == 2, "a family without features adds no degrees");
    CHECKF(label_count(statistics, count, "motive-intervals", "+8 -1") == 2, "motive intervals '+8 -1' twice");
    CHECKF(label_count(statistics, count, "motive-intervals", "0 +2") == 1, "a zero interval is unsigned");
    CHECKF(label_count(statistics, count, "motive-rhythm", "1/8 1/8 1/4") == 2, "motive rhythm twice");
    CHECKF(label_count(statistics, count, "motive-rhythm", "1/4 1/4 1/2") == 1, "motive rhythm once");
    CHECKF(label_count(statistics, count, "motive-nct-figure", "1 b6:appoggiatura 5") == 1,
           "a note in two occurrences counts one figure");
    CHECKF(label_count(statistics, count, "motive-nct-figure", "2 3:passing 4") == 1, "a figure in the second occurrence");
    CHECKF(category_size(statistics, count, "motive-nct-figure") == 2, "an unclassified tone has no figure");
    free(statistics);
    style_profile_destroy(profile);
}

/* The figure of a tone at the ends of its occurrence has a dash for the missing neighbour. */
static void test_motive_figure_ends(void) {
    const char *document =
        "{\"schema_version\": 13,"
        " \"non_chord_tones\": [{\"note_ids\": [1], \"type\": \"neighbor\"}, {\"note_ids\": [4], \"type\": \"escape-tone\"}],"
        " \"motives\": [{\"features\": {\"degrees\": [\"1\", \"2\", \"3\"], \"intervals\": [2, 2], \"rhythm\": [\"1/4\", \"1/4\", \"1/4\"]},"
        " \"occurrences\": [{\"note_ids\": [1, 2, 3, 4], \"degrees\": [\"6\", \"1\", \"2\", \"3\"]}]}]}";
    StyleProfile *profile = style_profile_create();
    CHECKF(profile && analyzer_style_add_document_json(profile, document) == STYLE_PROFILE_OK, "figure document");
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    if (!profile || style_profile_statistics(profile, &statistics, &count) != STYLE_PROFILE_OK) {
        style_profile_destroy(profile);
        return;
    }
    CHECKF(label_count(statistics, count, "motive-nct-figure", "- 6:neighbor 1") == 1, "no previous note");
    CHECKF(label_count(statistics, count, "motive-nct-figure", "2 3:escape-tone -") == 1, "no next note");
    free(statistics);
    style_profile_destroy(profile);
}

/* style_model_file_read_without drops the named categories before the model is built. */
static void test_read_without(StyleProfile *reference) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/reference.style.json", work_dir);
    const char *motive[] = {"motive-degrees", "motive-intervals", "motive-rhythm", "motive-nct-figure"};
    StyleModel *all = NULL, *without = NULL, *none = NULL;
    CHECKF(style_model_file_read(path, &all, NULL) == STYLE_MODEL_FILE_OK, "read all");
    CHECKF(style_model_file_read_without(path, motive, 4, &without, NULL) == STYLE_MODEL_FILE_OK && without, "read without");
    CHECKF(style_model_file_read_without(path, NULL, 0, &none, NULL) == STYLE_MODEL_FILE_OK && none, "read without none");
    StyleStatistic *all_statistics = NULL, *kept = NULL, *same = NULL;
    size_t all_count = 0, kept_count = 0, same_count = 0;
    if (all && without && none && style_model_statistics(all, &all_statistics, &all_count) == STYLE_MODEL_FILE_OK &&
        style_model_statistics(without, &kept, &kept_count) == STYLE_MODEL_FILE_OK &&
        style_model_statistics(none, &same, &same_count) == STYLE_MODEL_FILE_OK) {
        size_t motive_labels = 0;
        for (size_t i = 0; i < all_count; i++) {
            motive_labels += strncmp(all_statistics[i].category, "motive-", 7) == 0 ? 1 : 0;
        }
        CHECKF(motive_labels > 0, "the reference pieces have motive labels");
        CHECKF(kept_count + motive_labels == all_count, "%zu kept and %zu dropped of %zu", kept_count, motive_labels, all_count);
        for (size_t i = 0; i < kept_count; i++) {
            CHECKF(strncmp(kept[i].category, "motive-", 7) != 0, "%s was kept", kept[i].category);
        }
        CHECKF(same_count == all_count, "dropping none keeps every statistic");
        StyleComparison comparison;
        CHECKF(style_profile_compare(all, without, &comparison) == STYLE_COMPARE_OK, "compare with and without");
        CHECKF(comparison.category_count == 21, "%zu categories compared", comparison.category_count);
        size_t one_sided = 0;
        for (size_t i = 0; i < comparison.category_count; i++) {
            one_sided += comparison.categories[i].in_a != comparison.categories[i].in_b ? 1 : 0;
        }
        CHECKF(one_sided == 4, "%zu categories in one model only", one_sided);
        style_comparison_destroy(&comparison);
    }
    free(all_statistics);
    style_statistics_free(kept, kept_count);
    style_statistics_free(same, same_count);
    style_model_destroy(all);
    style_model_destroy(without);
    style_model_destroy(none);

    StyleModel *unknown = NULL;
    const char *bad[] = {"no-such-category"};
    CHECKF(style_model_file_read_without(path, bad, 1, &unknown, NULL) == STYLE_MODEL_FILE_ERR_ARGUMENT && unknown == NULL,
           "an unknown category is refused");
    CHECKF(style_model_file_read_without(path, NULL, 1, &unknown, NULL) == STYLE_MODEL_FILE_ERR_ARGUMENT, "no list");
    (void)reference;
}

/* ---- refusals ---- */

static void test_refusals(void) {
    StyleProfile *profile = style_profile_create();
    CHECKF(profile != NULL, "cannot create a profile");
    CHECKF(analyzer_style_add_document_json(profile, "{\"schema_version\": 8}") == STYLE_PROFILE_ERR_FORMAT, "version 8 is refused");
    CHECKF(analyzer_style_add_document_json(profile, "{}") == STYLE_PROFILE_ERR_FORMAT, "a document without a version is refused");
    CHECKF(analyzer_style_add_document_json(profile, "not json") == STYLE_PROFILE_ERR_FORMAT, "text is refused");
    CHECKF(analyzer_style_add_document_json(profile, "{\"schema_version\": 13}") == STYLE_PROFILE_OK &&
               style_profile_piece_count(profile) == 1,
           "an empty version 9 document counts as a piece with no labels");
    CHECKF(analyzer_style_profile_add_corpus(profile, "/nonexistent-corpus") == STYLE_PROFILE_ERR_IO, "a missing corpus is an I/O error");
    CHECKF(analyzer_style_add_document_json(NULL, "{}") == STYLE_PROFILE_ERR_ARGUMENT, "no profile");
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
        CHECKF(test_write_file(path, refused[i], strlen(refused[i])), "cannot write %s", path);
        CHECKF(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_ERR_FORMAT && model == NULL, "refused file %zu was read", i);
        style_model_destroy(model);
    }
    StyleModel *model = NULL;
    CHECKF(style_model_file_read("/nonexistent.style.json", &model, NULL) == STYLE_MODEL_FILE_ERR_IO, "a missing file is an I/O error");

    /* A profile with nothing in it still writes and reads. */
    StyleProfile *empty = style_profile_create();
    CHECKF(style_model_file_write(empty, "empty", URSATZ_ANALYZER_VERSION, path) == STYLE_MODEL_FILE_OK, "empty write");
    CHECKF(style_model_file_read(path, &model, NULL) == STYLE_MODEL_FILE_OK && model, "empty read");
    style_model_destroy(model);
    CHECKF(style_model_file_write(empty, "", URSATZ_ANALYZER_VERSION, path) == STYLE_MODEL_FILE_ERR_ARGUMENT, "an unnamed style is refused");
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


    StyleProfile *reference = profile_of("reference", 5);
    if (reference) {
        test_profile_statistics(reference);
        test_reference_snapshot(reference);
        test_round_trip(reference);
        test_read_without(reference);
        StyleModel *model = model_of(reference, "reference-self");
        if (model) {
            test_self_comparison(model);
            style_model_destroy(model);
        }
    }
    style_profile_destroy(reference);

    test_motive_categories();
    test_motive_figure_ends();
    test_chorales_and_sonatinas();
    test_refusals();

    return test_update_goldens() ? test_goldens_updated("test_style_profile") : test_finish("test_style_profile");
}
