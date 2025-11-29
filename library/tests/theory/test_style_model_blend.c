#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/style_features.h"
#include "theory/style_fraction.h"
#include "theory/style_model_blend.h"
#include "theory/style_model_build.h"

/*
 * Blending is exact: the result's frequencies are (1 - w) * a + w * b as
 * reduced fractions, weight 0 gives model A and weight 1 model B, and a bad
 * weight or an overflow is an explicit result, never a wrapped value.
 */

typedef struct {
    const char *category;
    const char *label;
    unsigned long numerator;
    unsigned long denominator;
} Expected;

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static StyleModel *make_model(const char *id, const StyleStatistic *statistics, size_t count) {
    StyleModel *model = NULL;
    assert(style_model_from_statistics(make_id(id), "model", "corpus", "v1", statistics, count, &model) ==
           STYLE_MODEL_BUILD_OK);
    return model;
}

static void assert_frequencies(const StyleModel *model, const Expected *expected, size_t expected_count) {
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    assert(style_features_decode(style_model_style_features(model), &statistics, &count) == STYLE_FEATURES_OK);
    assert(count == expected_count);
    for (size_t i = 0; i < count; i++) {
        assert(strcmp(statistics[i].category, expected[i].category) == 0);
        assert(strcmp(statistics[i].label, expected[i].label) == 0);
        assert(statistics[i].count == expected[i].numerator && statistics[i].total == expected[i].denominator);
    }
    style_statistics_free(statistics, count);
}

static StyleModel *blend(const StyleModel *a, const StyleModel *b, unsigned long numerator, unsigned long denominator) {
    StyleModel *result = NULL;
    assert(style_model_blend(make_id("blended"), "blend", "corpus-a+corpus-b", "v2", a, b, numerator, denominator,
                             &result) == STYLE_BLEND_OK);
    return result;
}

/* A: c/x 2/4, c/y 1/4, d/q 1/1.  B: c/x 1/4, c/z 3/4. */
static const StyleStatistic A_STATISTICS[] = {{"c", "x", 2, 4}, {"c", "y", 1, 4}, {"d", "q", 1, 1}};
static const StyleStatistic B_STATISTICS[] = {{"c", "x", 1, 4}, {"c", "z", 3, 4}};

static void test_weight_zero_is_a_and_weight_one_is_b(void) {
    StyleModel *a = make_model("a", A_STATISTICS, 3);
    StyleModel *b = make_model("b", B_STATISTICS, 2);

    /* Model A's frequencies, reduced: x 1/2, y 1/4, q 1/1; z (only in B) is left out. */
    const Expected as_a[] = {{"c", "x", 1, 2}, {"c", "y", 1, 4}, {"d", "q", 1, 1}};
    StyleModel *zero = blend(a, b, 0, 1);
    assert_frequencies(zero, as_a, 3);
    StyleModel *zero_unreduced = blend(a, b, 0, 7);
    assert_frequencies(zero_unreduced, as_a, 3);

    /* Model B's: x 1/4, z 3/4; y and q are left out. */
    const Expected as_b[] = {{"c", "x", 1, 4}, {"c", "z", 3, 4}};
    StyleModel *one = blend(a, b, 1, 1);
    assert_frequencies(one, as_b, 2);
    StyleModel *one_unreduced = blend(a, b, 9, 9);
    assert_frequencies(one_unreduced, as_b, 2);

    style_model_destroy(zero);
    style_model_destroy(zero_unreduced);
    style_model_destroy(one);
    style_model_destroy(one_unreduced);
    style_model_destroy(a);
    style_model_destroy(b);
}

static void test_interior_weights(void) {
    StyleModel *a = make_model("a", A_STATISTICS, 3);
    StyleModel *b = make_model("b", B_STATISTICS, 2);

    /* w = 1/4: x = 3/4 * 1/2 + 1/4 * 1/4 = 7/16, y = 3/4 * 1/4 = 3/16, z = 1/4 * 3/4 = 3/16, q = 3/4. */
    const Expected quarter[] = {{"c", "x", 7, 16}, {"c", "y", 3, 16}, {"c", "z", 3, 16}, {"d", "q", 3, 4}};
    StyleModel *blended = blend(a, b, 1, 4);
    assert_frequencies(blended, quarter, 4);
    style_model_destroy(blended);

    /* w = 2/4 is the same weight as 1/2: x = 1/4 + 1/8 = 3/8, y = 1/8, z = 3/8, q = 1/2. */
    const Expected half[] = {{"c", "x", 3, 8}, {"c", "y", 1, 8}, {"c", "z", 3, 8}, {"d", "q", 1, 2}};
    blended = blend(a, b, 2, 4);
    assert_frequencies(blended, half, 4);
    StyleModel *again = blend(a, b, 1, 2);
    assert_frequencies(again, half, 4);
    style_model_destroy(blended);
    style_model_destroy(again);

    /* A blended model blends again: 1/2 of (half blend) and B: x = 3/16 + 1/8 = 5/16, y = 1/16, z = 3/16 + 3/8 = 9/16,
       q = 1/4. */
    StyleModel *half_blend = blend(a, b, 1, 2);
    const Expected next[] = {{"c", "x", 5, 16}, {"c", "y", 1, 16}, {"c", "z", 9, 16}, {"d", "q", 1, 4}};
    blended = blend(half_blend, b, 1, 2);
    assert_frequencies(blended, next, 4);
    style_model_destroy(blended);
    style_model_destroy(half_blend);

    /* Blending a model with itself gives it back, whatever the weight. */
    const Expected a_reduced[] = {{"c", "x", 1, 2}, {"c", "y", 1, 4}, {"d", "q", 1, 1}};
    blended = blend(a, a, 3, 7);
    assert_frequencies(blended, a_reduced, 3);
    style_model_destroy(blended);

    style_model_destroy(a);
    style_model_destroy(b);
}

static void test_result_metadata_and_inputs_unchanged(void) {
    StyleModel *a = make_model("a", A_STATISTICS, 3);
    StyleModel *b = make_model("b", B_STATISTICS, 2);
    StyleModel *blended = blend(a, b, 1, 3);
    assert(strcmp(entity_id_value(style_model_id(blended)), "blended") == 0);
    assert(strcmp(style_model_style_name(blended), "blend") == 0);
    assert(strcmp(style_model_training_corpus(blended), "corpus-a+corpus-b") == 0);
    assert(strcmp(style_model_model_version(blended), "v2") == 0);

    /* The inputs are untouched. */
    const Expected a_unchanged[] = {{"c", "x", 2, 4}, {"c", "y", 1, 4}, {"d", "q", 1, 1}};
    assert_frequencies(a, a_unchanged, 3);
    const Expected b_unchanged[] = {{"c", "x", 1, 4}, {"c", "z", 3, 4}};
    assert_frequencies(b, b_unchanged, 2);
    assert(strcmp(entity_id_value(style_model_id(a)), "a") == 0);

    /* Two empty models blend to an empty one. */
    StyleModel *empty_a = make_model("ea", NULL, 0);
    StyleModel *empty_b = make_model("eb", NULL, 0);
    StyleModel *empty = blend(empty_a, empty_b, 1, 2);
    assert_frequencies(empty, NULL, 0);
    style_model_destroy(empty);
    style_model_destroy(empty_a);
    style_model_destroy(empty_b);
    style_model_destroy(blended);
    style_model_destroy(a);
    style_model_destroy(b);
}

static void test_bad_weights_and_arguments(void) {
    StyleModel *a = make_model("a", A_STATISTICS, 3);
    StyleModel *b = make_model("b", B_STATISTICS, 2);
    StyleModel *result = NULL;
    EntityID *id = make_id("blended");

    /* Each failure leaves the id with the caller and the output untouched. */
    assert(style_model_blend(id, "n", "c", "v", a, b, 1, 0, &result) == STYLE_BLEND_ERR_WEIGHT_ZERO_DENOMINATOR);
    assert(style_model_blend(id, "n", "c", "v", a, b, 0, 0, &result) == STYLE_BLEND_ERR_WEIGHT_ZERO_DENOMINATOR);
    assert(style_model_blend(id, "n", "c", "v", a, b, 2, 1, &result) == STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE);
    assert(style_model_blend(id, "n", "c", "v", a, b, 5, 4, &result) == STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE);
    assert(style_model_blend(id, "n", "c", "v", a, b, ULONG_MAX, ULONG_MAX - 1, &result) ==
           STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE);
    assert(style_model_blend(NULL, "n", "c", "v", a, b, 1, 2, &result) == STYLE_BLEND_ERR_NULL_ARGUMENT);
    assert(style_model_blend(id, "n", "c", "v", NULL, b, 1, 2, &result) == STYLE_BLEND_ERR_NULL_ARGUMENT);
    assert(style_model_blend(id, "n", "c", "v", a, NULL, 1, 2, &result) == STYLE_BLEND_ERR_NULL_ARGUMENT);
    assert(style_model_blend(id, "n", "c", "v", a, b, 1, 2, NULL) == STYLE_BLEND_ERR_NULL_ARGUMENT);
    assert(style_model_blend(id, NULL, "c", "v", a, b, 1, 2, &result) == STYLE_BLEND_ERR_MODEL);
    assert(style_model_blend(id, "n", NULL, "v", a, b, 1, 2, &result) == STYLE_BLEND_ERR_MODEL);
    assert(style_model_blend(id, "n", "c", "", a, b, 1, 2, &result) == STYLE_BLEND_ERR_MODEL);
    assert(result == NULL);

    /* A model that is not made of statistics cannot be blended. */
    Value *not_features = NULL;
    assert(value_create("other-features", "x", 1, &not_features) == VALUE_OK);
    StyleModel *other = NULL;
    assert(style_model_create(make_id("other"), "Other", not_features, "corpus", "v1", &other) == STYLE_MODEL_OK);
    assert(style_model_blend(id, "n", "c", "v", other, b, 1, 2, &result) == STYLE_BLEND_ERR_MALFORMED);
    assert(style_model_blend(id, "n", "c", "v", a, other, 1, 2, &result) == STYLE_BLEND_ERR_MALFORMED);
    style_model_destroy(other);

    /* An exact result that does not fit in unsigned long is an error, not a wrapped value: half of 1/ULONG_MAX is
       1/(2 * ULONG_MAX). */
    const StyleStatistic tiny_a[] = {{"c", "x", 1, ULONG_MAX}};
    const StyleStatistic tiny_b[] = {{"c", "x", 1, ULONG_MAX - 1}};
    StyleModel *big_a = make_model("big-a", tiny_a, 1);
    StyleModel *big_b = make_model("big-b", tiny_b, 1);
    assert(style_model_blend(id, "n", "c", "v", big_a, big_b, 1, 2, &result) == STYLE_BLEND_ERR_OVERFLOW);
    assert(result == NULL);
    /* The same models at weight 0 and 1 stay exact (the weights cancel). */
    const Expected only_a[] = {{"c", "x", 1, ULONG_MAX}};
    const Expected only_b[] = {{"c", "x", 1, ULONG_MAX - 1}};
    StyleModel *at_zero = blend(big_a, big_b, 0, 1);
    assert_frequencies(at_zero, only_a, 1);
    StyleModel *at_one = blend(big_a, big_b, 1, 1);
    assert_frequencies(at_one, only_b, 1);
    style_model_destroy(at_zero);
    style_model_destroy(at_one);
    style_model_destroy(big_a);
    style_model_destroy(big_b);

    entity_id_destroy(id);
    style_model_destroy(a);
    style_model_destroy(b);
}

static void test_fractions(void) {
    StyleFraction a = {1, 2}, b = {1, 3}, out = {9, 9};
    assert(style_fraction_add(&a, &b, &out) == STYLE_FRACTION_OK && out.numerator == 5 && out.denominator == 6);
    assert(style_fraction_multiply(&a, &b, &out) == STYLE_FRACTION_OK && out.numerator == 1 && out.denominator == 6);
    StyleFraction unreduced = {6, 8};
    assert(style_fraction_reduce(&unreduced, &out) == STYLE_FRACTION_OK && out.numerator == 3 && out.denominator == 4);
    StyleFraction zero = {0, 5};
    assert(style_fraction_reduce(&zero, &out) == STYLE_FRACTION_OK && out.numerator == 0 && out.denominator == 1);

    out.numerator = 9;
    out.denominator = 9;
    StyleFraction bad = {1, 0}, big = {ULONG_MAX, 1}, one = {1, 1}, thin = {1, ULONG_MAX}, thin2 = {1, ULONG_MAX - 1};
    assert(style_fraction_add(&bad, &a, &out) == STYLE_FRACTION_ERR_ZERO_DENOMINATOR);
    assert(style_fraction_multiply(&a, &bad, &out) == STYLE_FRACTION_ERR_ZERO_DENOMINATOR);
    assert(style_fraction_reduce(&bad, &out) == STYLE_FRACTION_ERR_ZERO_DENOMINATOR);
    assert(style_fraction_add(&big, &one, &out) == STYLE_FRACTION_ERR_OVERFLOW);
    assert(style_fraction_add(&thin, &thin2, &out) == STYLE_FRACTION_ERR_OVERFLOW);
    assert(style_fraction_multiply(&big, &big, &out) == STYLE_FRACTION_ERR_OVERFLOW);
    assert(style_fraction_multiply(&thin, &thin, &out) == STYLE_FRACTION_ERR_OVERFLOW);
    assert(out.numerator == 9 && out.denominator == 9);
    assert(style_fraction_add(NULL, &a, &out) == STYLE_FRACTION_ERR_NULL_ARGUMENT);
    assert(style_fraction_multiply(&a, &a, NULL) == STYLE_FRACTION_ERR_NULL_ARGUMENT);
    assert(style_fraction_reduce(&a, NULL) == STYLE_FRACTION_ERR_NULL_ARGUMENT);
    /* Cancelling across keeps a large product exact. */
    StyleFraction left = {ULONG_MAX, 3}, right = {3, ULONG_MAX};
    assert(style_fraction_multiply(&left, &right, &out) == STYLE_FRACTION_OK && out.numerator == 1 &&
           out.denominator == 1);
}

int main(void) {
    test_weight_zero_is_a_and_weight_one_is_b();
    test_interior_weights();
    test_result_metadata_and_inputs_unchanged();
    test_bad_weights_and_arguments();
    test_fractions();
    printf("test_style_model_blend: ok\n");
    return 0;
}
