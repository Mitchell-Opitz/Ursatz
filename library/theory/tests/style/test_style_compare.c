#include <stdbool.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "theory/style/style_compare.h"
#include "theory/style/style_model_build.h"

#include "ursatz_test.h"

/* The comparison of two style models (R-617): exact category distances as Rationals, the dimension and overall means as
   doubles, a category only one model has at distance 1, and an overflow refused, never wrapped. */

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
    CHECKF(a && b, "cannot build the hand models");
    if (a && b) {
        StyleComparison comparison;
        CHECKF(style_profile_compare(a, b, &comparison) == STYLE_COMPARE_OK, "hand comparison");
        const StyleCategoryDistance *cadence = NULL, *mode = NULL;
        for (size_t i = 0; i < comparison.category_count; i++) {
            if (strcmp(comparison.categories[i].category, "cadence-type") == 0) {
                cadence = &comparison.categories[i];
            } else if (strcmp(comparison.categories[i].category, "mode") == 0) {
                mode = &comparison.categories[i];
            }
        }
        CHECKF(cadence && cadence->distance.num == 1 && cadence->distance.den == 2, "cadence distance");
        CHECKF(cadence && cadence->in_a && cadence->in_b && strcmp(cadence->top_a[0], "half") == 0 &&
                   strcmp(cadence->top_b[0], "perfect-authentic") == 0 && strcmp(cadence->top_a[1], "perfect-authentic") == 0 &&
                   cadence->top_a[2] == NULL,
               "top labels");
        CHECKF(mode && mode->distance.num == 1 && mode->distance.den == 1 && !mode->in_a && mode->in_b,
               "a category in one model only is distance 1");
        /* harmony: cadence-type only, 1/2. form: mode only, 1. Rhythm and melody have no categories. */
        CHECKF(comparison.dimension_present[STYLE_DIMENSION_HARMONY] && comparison.dimension[STYLE_DIMENSION_HARMONY] == 0.5,
               "harmony dimension");
        CHECKF(comparison.dimension_present[STYLE_DIMENSION_FORM] && comparison.dimension[STYLE_DIMENSION_FORM] == 1.0,
               "form dimension");
        CHECKF(!comparison.dimension_present[STYLE_DIMENSION_RHYTHM] && !comparison.dimension_present[STYLE_DIMENSION_MELODY],
               "empty dimensions are left out");
        CHECKF(comparison.overall_present && comparison.overall == 0.75,
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
    CHECKF(a && b, "cannot build the large models");
    if (a && b) {
        StyleComparison comparison;
        CHECKF(style_profile_compare(a, b, &comparison) == STYLE_COMPARE_ERR_OVERFLOW, "an overflowing comparison is refused");
        CHECKF(comparison.category_count == 0 && comparison.categories == NULL, "nothing is left behind after an overflow");
    }
    style_model_destroy(a);
    style_model_destroy(b);
}

/* A model compared with itself is at distance 0 in every category, dimension and overall. */
static void test_self_comparison(void) {
    const StyleStatistic stats[] = {{"cadence-type", "half", 3, 4},
                                    {"cadence-type", "perfect-authentic", 1, 4},
                                    {"contour", "down", 2, 5},
                                    {"contour", "up", 3, 5}};
    StyleModel *model = build_model("self", stats, 4);
    CHECKF(model, "cannot build the model");
    if (model) {
        StyleComparison comparison;
        CHECKF(style_profile_compare(model, model, &comparison) == STYLE_COMPARE_OK, "self comparison");
        CHECKF(comparison.category_count == 2 && comparison.overall_present && comparison.overall == 0.0,
               "a model is at distance 0 from itself");
        for (size_t i = 0; i < comparison.category_count; i++) {
            CHECKF(comparison.categories[i].distance.num == 0 && comparison.categories[i].in_a && comparison.categories[i].in_b,
                   "category %zu", i);
        }
        style_comparison_destroy(&comparison);
    }
    style_model_destroy(model);
}

static void test_refusals(void) {
    StyleComparison comparison;
    CHECKF(style_profile_compare(NULL, NULL, &comparison) == STYLE_COMPARE_ERR_ARGUMENT, "NULL models are refused");
    style_comparison_destroy(NULL);
}

int main(void) {
    test_hand_distances();
    test_self_comparison();
    test_overflow();
    test_refusals();
    return test_finish("theory_style_compare");
}
