#include "theory/style/style_reference.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "theory/style/style_model_build.h"
#include "theory/style/style_model_file.h"

struct StyleReference {
    StyleStatistic *statistics; /* the model's, without the dropped categories */
    size_t count;
    StyleModel *base; /* a model of the statistics */
    char **without;   /* the dropped categories */
    size_t without_count;
    char *model_version;
};

/* A style model of the statistics. The caller destroys it. */
static StyleModel *model_of(const char *name, const char *model_version, const StyleStatistic *statistics, size_t count) {
    EntityID *id = NULL;
    StyleModel *model = NULL;
    if (entity_id_create(name, &id) != ENTITY_ID_OK) {
        return NULL;
    }
    if (style_model_from_statistics(id, name, "<fragment>", model_version, statistics, count, &model) !=
        STYLE_MODEL_BUILD_OK) {
        entity_id_destroy(id);
        return NULL;
    }
    return model;
}

static bool has_category(const StyleStatistic *statistics, size_t count, const char *category) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(statistics[i].category, category) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_dropped(const char *category, const char *const *dropped, size_t dropped_count) {
    for (size_t i = 0; i < dropped_count; i++) {
        if (strcmp(category, dropped[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Moves the statistics whose category is not dropped to the front of the array. Returns how many stay. */
static size_t keep_statistics(StyleStatistic *statistics, size_t count, const char *const *dropped, size_t dropped_count) {
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (!is_dropped(statistics[i].category, dropped, dropped_count)) {
            statistics[used++] = statistics[i];
        }
    }
    return used;
}

void style_reference_destroy(StyleReference *reference) {
    if (!reference) {
        return;
    }
    style_model_destroy(reference->base);
    style_statistics_free(reference->statistics, reference->count);
    for (size_t i = 0; i < reference->without_count; i++) {
        free(reference->without[i]);
    }
    free(reference->without);
    free(reference->model_version);
    free(reference);
}

StyleReferenceResult style_reference_create(const StyleModel *model, const char *const *without, size_t without_count,
                                            const char *model_version, StyleReference **out) {
    if (!out) {
        return STYLE_REFERENCE_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!model || !model_version || (without_count > 0 && !without)) {
        return STYLE_REFERENCE_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < without_count; i++) {
        StyleDimension dimension;
        if (!without[i] || !style_category_dimension(without[i], &dimension)) {
            return STYLE_REFERENCE_ERR_ARGUMENT;
        }
    }
    StyleReference *reference = calloc(1, sizeof *reference);
    if (!reference) {
        return STYLE_REFERENCE_ERR_ALLOC;
    }
    reference->without = calloc(without_count > 0 ? without_count : 1, sizeof *reference->without);
    reference->model_version = strdup(model_version);
    bool ok = reference->without != NULL && reference->model_version != NULL;
    for (size_t i = 0; ok && i < without_count; i++) {
        reference->without[i] = strdup(without[i]);
        ok = reference->without[i] != NULL;
        reference->without_count = i + 1;
    }
    ok = ok && style_model_statistics(model, &reference->statistics, &reference->count) == STYLE_MODEL_FILE_OK;
    if (ok) {
        reference->count = keep_statistics(reference->statistics, reference->count, (const char *const *)reference->without,
                                           reference->without_count);
        reference->base = model_of("fragment-style-base", reference->model_version, reference->statistics, reference->count);
        ok = reference->base != NULL;
    }
    if (!ok) {
        style_reference_destroy(reference);
        return STYLE_REFERENCE_ERR_ALLOC;
    }
    *out = reference;
    return STYLE_REFERENCE_OK;
}

StyleReferenceResult style_reference_distance(const StyleReference *reference, StyleStatistic *fragment_statistics,
                                              size_t fragment_count, StyleComparison *out) {
    if (!reference || !out || (fragment_count > 0 && !fragment_statistics)) {
        return STYLE_REFERENCE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    StyleReferenceResult status = STYLE_REFERENCE_ERR_ALLOC;
    StyleStatistic *kept = NULL;
    size_t kept_count = 0;
    StyleModel *fragment_model = NULL, *kept_model = NULL;
    StyleComparison full = {0}, means = {0};
    fragment_count = keep_statistics(fragment_statistics, fragment_count, (const char *const *)reference->without,
                                     reference->without_count);
    kept = calloc(reference->count > 0 ? reference->count : 1, sizeof *kept);
    fragment_model = model_of("fragment-style", reference->model_version, fragment_statistics, fragment_count);
    if (!kept || !fragment_model) {
        goto done;
    }
    /* The means come from the reference cut down to the categories the fragment has. */
    for (size_t i = 0; i < reference->count; i++) {
        if (has_category(fragment_statistics, fragment_count, reference->statistics[i].category)) {
            kept[kept_count++] = reference->statistics[i];
        }
    }
    kept_model = model_of("fragment-style-reference", reference->model_version, kept, kept_count);
    if (!kept_model) {
        goto done;
    }
    if (style_profile_compare(fragment_model, reference->base, &full) != STYLE_COMPARE_OK ||
        style_profile_compare(fragment_model, kept_model, &means) != STYLE_COMPARE_OK) {
        status = STYLE_REFERENCE_ERR_COMPARE;
        goto done;
    }
    /* The full comparison's categories (the missing ones too) with the means of the cut one. */
    StyleCategoryDistance *category_list = means.categories;
    size_t category_count = means.category_count;
    *out = means;
    out->categories = full.categories;
    out->category_count = full.category_count;
    full.categories = category_list;
    full.category_count = category_count;
    memset(&means, 0, sizeof means);
    status = STYLE_REFERENCE_OK;
done:
    style_comparison_destroy(&full);
    style_comparison_destroy(&means);
    style_model_destroy(kept_model);
    style_model_destroy(fragment_model);
    free(kept);
    return status;
}
