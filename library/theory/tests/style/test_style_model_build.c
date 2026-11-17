#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/value.h"
#include "model/identity/entity_id.h"
#include "theory/style/style_features.h"
#include "theory/style/style_model_build.h"

/*
 * A StyleModel built from corpus statistics keeps each statistic as an exact
 * count/total, in a sorted, duplicate-free list that reads back unchanged;
 * invalid statistics are rejected with an explicit result.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static void assert_statistic(const StyleStatistic *s, const char *category, const char *label, unsigned long count,
                             unsigned long total) {
    assert(strcmp(s->category, category) == 0 && strcmp(s->label, label) == 0);
    assert(s->count == count && s->total == total);
}

/* Deliberately not in sorted order, with an unreduced fraction (2/4). */
static const StyleStatistic STATISTICS[] = {
    {"chord-progression-2", "V > I", 2, 5},
    {"cadence-type", "pac", 1, 2},
    {"chord-progression-2", "I > V", 3, 5},
    {"cadence-type", "hc", 2, 4},
};

static void test_build_and_read_back(void) {
    StyleModel *model = NULL;
    assert(style_model_from_statistics(make_id("style-1"), "Test style", "reference-set", "v1", STATISTICS, 4,
                                       &model) == STYLE_MODEL_BUILD_OK);
    assert(strcmp(style_model_style_name(model), "Test style") == 0);
    assert(strcmp(style_model_training_corpus(model), "reference-set") == 0);
    assert(strcmp(style_model_model_version(model), "v1") == 0);
    assert(strcmp(value_type(style_model_style_features(model)), STYLE_FEATURES_VALUE_TYPE) == 0);

    /* The features list is sorted by category and then label, with each count/total as given. */
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    assert(style_features_decode(style_model_style_features(model), &statistics, &count) == STYLE_FEATURES_OK);
    assert(count == 4);
    assert_statistic(&statistics[0], "cadence-type", "hc", 2, 4);
    assert_statistic(&statistics[1], "cadence-type", "pac", 1, 2);
    assert_statistic(&statistics[2], "chord-progression-2", "I > V", 3, 5);
    assert_statistic(&statistics[3], "chord-progression-2", "V > I", 2, 5);
    style_statistics_free(statistics, count);

    style_model_destroy(model);
}

static void test_empty_model(void) {
    StyleModel *model = NULL;
    assert(style_model_from_statistics(make_id("style-empty"), "Empty", "none", "v0", NULL, 0, &model) ==
           STYLE_MODEL_BUILD_OK);
    size_t count = 99;
    StyleStatistic *statistics = (StyleStatistic *)0x1;
    assert(style_features_decode(style_model_style_features(model), &statistics, &count) == STYLE_FEATURES_OK);
    assert(count == 0 && statistics == NULL);
    style_statistics_free(NULL, 0);
    style_model_destroy(model);
}

/* A failed build leaves the id with the caller. */
static StyleModelBuildResult build(const StyleStatistic *statistics, size_t count) {
    EntityID *id = make_id("style-bad");
    StyleModel *model = NULL;
    StyleModelBuildResult result =
        style_model_from_statistics(id, "Bad", "corpus", "v1", statistics, count, &model);
    if (result == STYLE_MODEL_BUILD_OK) {
        style_model_destroy(model);
    } else {
        assert(model == NULL);
        entity_id_destroy(id);
    }
    return result;
}

static void test_invalid_statistics(void) {
    const StyleStatistic over[] = {{"c", "x", 3, 2}};
    const StyleStatistic zero_total[] = {{"c", "x", 0, 0}};
    const StyleStatistic empty_label[] = {{"c", "", 1, 2}};
    const StyleStatistic empty_category[] = {{"", "x", 1, 2}};
    const StyleStatistic tab_label[] = {{"c", "a\tb", 1, 2}};
    const StyleStatistic newline_category[] = {{"c\n", "x", 1, 2}};
    const StyleStatistic null_label[] = {{"c", NULL, 1, 2}};
    const StyleStatistic duplicate[] = {{"c", "x", 1, 2}, {"c", "y", 1, 2}, {"c", "x", 1, 3}};
    const StyleStatistic whole[] = {{"c", "x", 2, 2}, {"d", "x", 0, 1}};
    assert(build(over, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(zero_total, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(empty_label, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(empty_category, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(tab_label, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(newline_category, 1) == STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(build(null_label, 1) == STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT);
    assert(build(duplicate, 3) == STYLE_MODEL_BUILD_ERR_DUPLICATE);
    assert(build(NULL, 2) == STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT);
    /* A frequency of exactly 1 and one of exactly 0 are valid. */
    assert(build(whole, 2) == STYLE_MODEL_BUILD_OK);

    /* The model's own fields: name, training-corpus label and version are required. */
    StyleModel *model = NULL;
    EntityID *id = make_id("style-x");
    assert(style_model_from_statistics(id, NULL, "corpus", "v1", STATISTICS, 4, &model) == STYLE_MODEL_BUILD_ERR_MODEL);
    assert(style_model_from_statistics(id, "Name", "", "v1", STATISTICS, 4, &model) == STYLE_MODEL_BUILD_ERR_MODEL);
    assert(style_model_from_statistics(id, "Name", "corpus", NULL, STATISTICS, 4, &model) ==
           STYLE_MODEL_BUILD_ERR_MODEL);
    assert(model == NULL);
    assert(style_model_from_statistics(NULL, "Name", "corpus", "v1", STATISTICS, 4, &model) ==
           STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT);
    assert(style_model_from_statistics(id, "Name", "corpus", "v1", STATISTICS, 4, NULL) ==
           STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT);
    entity_id_destroy(id);
}

static void test_features_round_trip_and_malformed(void) {
    Value *features = NULL;
    assert(style_features_encode(STATISTICS, 4, &features) == STYLE_FEATURES_OK);
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    assert(style_features_decode(features, &statistics, &count) == STYLE_FEATURES_OK && count == 4);
    /* Encoding what was decoded gives the same bytes. */
    Value *again = NULL;
    assert(style_features_encode(statistics, count, &again) == STYLE_FEATURES_OK);
    assert(value_equals(features, again));
    style_statistics_free(statistics, count);
    value_destroy(again);
    value_destroy(features);

    /* Other types and content that is not a well formed, sorted, duplicate-free list are malformed. */
    const char *bad_contents[] = {"c\tx\t1\n",           "c\tx\t1\t2",       "c\tx\ta\t2\n",    "c\tx\t3\t2\n",
                                  "c\tx\t1\t0\n",        "\tx\t1\t2\n",      "c\t\t1\t2\n",     "c\ty\t1\t2\nc\tx\t1\t2\n",
                                  "c\tx\t1\t2\nc\tx\t1\t2\n", "c\tx\t-1\t2\n",  "c\tx\t1\t2\textra\n"};
    for (size_t i = 0; i < sizeof(bad_contents) / sizeof(bad_contents[0]); i++) {
        Value *bad = NULL;
        assert(value_create(STYLE_FEATURES_VALUE_TYPE, bad_contents[i], strlen(bad_contents[i]), &bad) == VALUE_OK);
        assert(style_features_decode(bad, &statistics, &count) == STYLE_FEATURES_ERR_MALFORMED);
        value_destroy(bad);
    }
    Value *other = NULL;
    assert(value_create("something-else", "c\tx\t1\t2\n", 8, &other) == VALUE_OK);
    assert(style_features_decode(other, &statistics, &count) == STYLE_FEATURES_ERR_MALFORMED);
    value_destroy(other);

    assert(style_features_decode(NULL, &statistics, &count) == STYLE_FEATURES_ERR_NULL_ARGUMENT);
    assert(style_features_encode(NULL, 1, &features) == STYLE_FEATURES_ERR_NULL_ARGUMENT);
    assert(style_features_encode(STATISTICS, 1, NULL) == STYLE_FEATURES_ERR_NULL_ARGUMENT);

    /* A model whose features are not statistics reads back as malformed, never as made-up statistics. */
    Value *not_features = NULL;
    assert(value_create("other-features", "x", 1, &not_features) == VALUE_OK);
    StyleModel *model = NULL;
    assert(style_model_create(make_id("style-other"), "Other", not_features, "corpus", "v1", &model) == STYLE_MODEL_OK);
    assert(style_features_decode(style_model_style_features(model), &statistics, &count) ==
           STYLE_FEATURES_ERR_MALFORMED);
    style_model_destroy(model);
}

int main(void) {
    test_build_and_read_back();
    test_empty_model();
    test_invalid_statistics();
    test_features_round_trip_and_malformed();
    printf("test_style_model_build: ok\n");
    return 0;
}
