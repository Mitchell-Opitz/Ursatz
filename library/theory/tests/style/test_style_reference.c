#include <stdbool.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "theory/style/style_model_build.h"
#include "theory/style/style_reference.h"

#include "ursatz_test.h"

/*
 * The style reference (style_reference.h): prepared once from a model with some categories dropped, then any number of fragments are
 * measured against it. Refusals leave *out NULL; a fragment made of the model's own statistics is at distance 0; a dropped category
 * appears in no distance.
 */

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

static const StyleStatistic STATS[] = {{"cadence-type", "half", 3, 4},
                                       {"cadence-type", "perfect-authentic", 1, 4},
                                       {"contour", "down", 2, 5},
                                       {"contour", "up", 3, 5}};

static void test_refusals(void) {
    StyleModel *model = build_model("reference-refusals", STATS, 4);
    CHECKF(model, "cannot build the model");
    if (!model) {
        return;
    }
    StyleReference *reference = (StyleReference *)model; /* a value to see overwritten */
    CHECK(style_reference_create(NULL, NULL, 0, "test", &reference) == STYLE_REFERENCE_ERR_ARGUMENT && reference == NULL);
    CHECK(style_reference_create(model, NULL, 0, "test", NULL) == STYLE_REFERENCE_ERR_ARGUMENT);
    const char *unknown[] = {"no-such-category"};
    reference = (StyleReference *)model;
    CHECK(style_reference_create(model, unknown, 1, "test", &reference) == STYLE_REFERENCE_ERR_ARGUMENT && reference == NULL);
    CHECK(style_reference_create(model, NULL, 1, "test", &reference) == STYLE_REFERENCE_ERR_ARGUMENT && reference == NULL);

    StyleComparison comparison;
    StyleStatistic fragment[4];
    memcpy(fragment, STATS, sizeof(fragment));
    CHECK(style_reference_distance(NULL, fragment, 4, &comparison) == STYLE_REFERENCE_ERR_ARGUMENT);
    style_reference_destroy(NULL); /* a no-op */
    style_model_destroy(model);
}

static void test_distance_from_its_own_statistics(void) {
    StyleModel *model = build_model("reference-own", STATS, 4);
    CHECKF(model, "cannot build the model");
    if (!model) {
        return;
    }
    StyleReference *reference = NULL;
    CHECK(style_reference_create(model, NULL, 0, "test", &reference) == STYLE_REFERENCE_OK && reference != NULL);
    style_model_destroy(model); /* the reference copies what it needs */
    if (reference) {
        StyleStatistic fragment[4];
        StyleComparison comparison;
        memcpy(fragment, STATS, sizeof(fragment));
        CHECK(style_reference_distance(reference, fragment, 4, &comparison) == STYLE_REFERENCE_OK);
        CHECKF(comparison.category_count == 2 && comparison.overall_present && comparison.overall == 0.0,
               "a fragment of the model's own statistics is at distance 0 (%zu categories)", comparison.category_count);
        style_comparison_destroy(&comparison);
        style_reference_destroy(reference);
    }
}

static void test_a_dropped_category(void) {
    StyleModel *model = build_model("reference-dropped", STATS, 4);
    CHECKF(model, "cannot build the model");
    if (!model) {
        return;
    }
    const char *without[] = {"contour"};
    StyleReference *reference = NULL;
    CHECK(style_reference_create(model, without, 1, "test", &reference) == STYLE_REFERENCE_OK && reference != NULL);
    style_model_destroy(model);
    if (reference) {
        StyleStatistic fragment[4];
        StyleComparison comparison;
        memcpy(fragment, STATS, sizeof(fragment));
        CHECK(style_reference_distance(reference, fragment, 4, &comparison) == STYLE_REFERENCE_OK);
        CHECKF(comparison.category_count == 1 && strcmp(comparison.categories[0].category, "cadence-type") == 0,
               "the dropped category is in no distance (%zu categories)", comparison.category_count);
        style_comparison_destroy(&comparison);
        style_reference_destroy(reference);
    }
}

int main(void) {
    test_refusals();
    test_distance_from_its_own_statistics();
    test_a_dropped_category();
    return test_finish("theory_style_reference");
}
