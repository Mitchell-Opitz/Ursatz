#include "theory/style/style_compare.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_features.h"
#include "theory/style/style_model_file.h"

/* One category of one model: statistics [first, first + count) of a sorted list. */
typedef struct {
    const StyleStatistic *items;
    size_t count;
} Slice;

/* Total variation distance between two slices (either may be empty only if both categories exist): half the sum over the
   labels of |count_a/total_a - count_b/total_b|, exact. False when a step does not fit a Rational. */
static bool slice_distance(Slice a, Slice b, Rational *out) {
    if (a.items[0].total > (unsigned long)LLONG_MAX || b.items[0].total > (unsigned long)LLONG_MAX) {
        return false;
    }
    Rational sum = {0, 1};
    size_t i = 0, j = 0;
    while (i < a.count || j < b.count) {
        int order = i == a.count ? 1 : (j == b.count ? -1 : strcmp(a.items[i].label, b.items[j].label));
        Rational frequency_a = {0, 1}, frequency_b = {0, 1}, difference;
        if (order <= 0 && rational_make((long long)a.items[i].count, (long long)a.items[i].total, &frequency_a) != RATIONAL_OK) {
            return false;
        }
        if (order >= 0 && rational_make((long long)b.items[j].count, (long long)b.items[j].total, &frequency_b) != RATIONAL_OK) {
            return false;
        }
        if (rational_sub(frequency_a, frequency_b, &difference) != RATIONAL_OK ||
            (rational_is_negative(difference) && rational_neg(difference, &difference) != RATIONAL_OK) ||
            rational_add(sum, difference, &sum) != RATIONAL_OK) {
            return false;
        }
        if (order <= 0) {
            i++;
        }
        if (order >= 0) {
            j++;
        }
    }
    return rational_div_int(sum, 2, out) == RATIONAL_OK;
}

static size_t category_end(const StyleStatistic *items, size_t start, size_t count) {
    size_t end = start;
    while (end < count && strcmp(items[end].category, items[start].category) == 0) {
        end++;
    }
    return end;
}

/* The three most frequent labels of a slice, by count (high first), then label (byte order). */
static bool top_labels(Slice slice, char *out[3]) {
    out[0] = out[1] = out[2] = NULL;
    const StyleStatistic *previous = NULL;
    for (int rank = 0; rank < 3; rank++) {
        const StyleStatistic *best = NULL;
        for (size_t i = 0; i < slice.count; i++) {
            const StyleStatistic *item = &slice.items[i];
            bool after_previous = !previous || item->count < previous->count ||
                                  (item->count == previous->count && strcmp(item->label, previous->label) > 0);
            if (after_previous && (!best || item->count > best->count)) {
                best = item;
            }
        }
        if (!best) {
            break;
        }
        out[rank] = strdup(best->label);
        if (!out[rank]) {
            return false;
        }
        previous = best;
    }
    return true;
}

void style_comparison_destroy(StyleComparison *comparison) {
    if (!comparison) {
        return;
    }
    for (size_t i = 0; i < comparison->category_count; i++) {
        free(comparison->categories[i].category);
        for (int k = 0; k < 3; k++) {
            free(comparison->categories[i].top_a[k]);
            free(comparison->categories[i].top_b[k]);
        }
    }
    free(comparison->categories);
    memset(comparison, 0, sizeof *comparison);
}

StyleCompareResult style_profile_compare(const StyleModel *model_a, const StyleModel *model_b, StyleComparison *out) {
    if (!model_a || !model_b || !out) {
        return STYLE_COMPARE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    StyleStatistic *a = NULL, *b = NULL;
    size_t count_a = 0, count_b = 0;
    StyleCompareResult result = STYLE_COMPARE_ERR_MODEL;
    if (style_model_statistics(model_a, &a, &count_a) != STYLE_MODEL_FILE_OK ||
        style_model_statistics(model_b, &b, &count_b) != STYLE_MODEL_FILE_OK) {
        goto done;
    }
    out->categories = calloc(count_a + count_b + 1, sizeof *out->categories);
    if (!out->categories) {
        result = STYLE_COMPARE_ERR_ALLOC;
        goto done;
    }
    size_t dimension_categories[STYLE_DIMENSION_COUNT] = {0};
    double dimension_total[STYLE_DIMENSION_COUNT] = {0};
    size_t i = 0, j = 0;
    while (i < count_a || j < count_b) {
        size_t end_a = i < count_a ? category_end(a, i, count_a) : i;
        size_t end_b = j < count_b ? category_end(b, j, count_b) : j;
        int order = i == count_a ? 1 : (j == count_b ? -1 : strcmp(a[i].category, b[j].category));
        Slice slice_a = {a + i, order <= 0 ? end_a - i : 0};
        Slice slice_b = {b + j, order >= 0 ? end_b - j : 0};
        const char *category = order <= 0 ? a[i].category : b[j].category;
        StyleCategoryDistance *entry = &out->categories[out->category_count];
        if (!style_category_dimension(category, &entry->dimension)) {
            goto done;
        }
        entry->category = strdup(category);
        if (!entry->category) {
            result = STYLE_COMPARE_ERR_ALLOC;
            goto done;
        }
        out->category_count++;
        entry->in_a = slice_a.count > 0;
        entry->in_b = slice_b.count > 0;
        if (entry->in_a && entry->in_b) {
            if (!slice_distance(slice_a, slice_b, &entry->distance)) {
                result = STYLE_COMPARE_ERR_OVERFLOW;
                goto done;
            }
        } else {
            entry->distance = rational_ratio(1, 1);
        }
        if (!top_labels(slice_a, entry->top_a) || !top_labels(slice_b, entry->top_b)) {
            result = STYLE_COMPARE_ERR_ALLOC;
            goto done;
        }
        dimension_total[entry->dimension] += rational_to_double(entry->distance);
        dimension_categories[entry->dimension]++;
        if (order <= 0) {
            i = end_a;
        }
        if (order >= 0) {
            j = end_b;
        }
    }
    double overall_total = 0;
    size_t dimensions = 0;
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (dimension_categories[d] == 0) {
            continue;
        }
        out->dimension_present[d] = true;
        out->dimension[d] = dimension_total[d] / (double)dimension_categories[d];
        overall_total += out->dimension[d];
        dimensions++;
    }
    if (dimensions > 0) {
        out->overall = overall_total / (double)dimensions;
        out->overall_present = true;
    }
    result = STYLE_COMPARE_OK;
done:
    style_statistics_free(a, count_a);
    style_statistics_free(b, count_b);
    if (result != STYLE_COMPARE_OK) {
        style_comparison_destroy(out);
    }
    return result;
}
