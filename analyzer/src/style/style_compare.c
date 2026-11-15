#include <ursatz_analyzer/style_profile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef StyleRationalInt u128;

static u128 gcd64(u128 a, u128 b) {
    while (b) {
        u128 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

double style_rational_value(StyleRational value) {
    return value.denominator == 0 ? 0.0 : (double)value.numerator / (double)value.denominator;
}

static size_t format_u128(u128 value, char *digits) {
    char reversed[48];
    size_t n = 0;
    do {
        reversed[n++] = (char)('0' + (int)(value % 10));
        value /= 10;
    } while (value);
    for (size_t i = 0; i < n; i++) {
        digits[i] = reversed[n - 1 - i];
    }
    digits[n] = '\0';
    return n;
}

void style_rational_format(StyleRational value, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    char numerator[48], denominator[48];
    format_u128(value.numerator, numerator);
    format_u128(value.denominator, denominator);
    snprintf(buffer, size, "%s/%s", numerator, denominator);
}

static StyleRational reduce(u128 numerator, u128 denominator) {
    u128 divisor = gcd64(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    return (StyleRational){numerator, denominator};
}

/* x + y, exact and reduced; false on overflow. */
static bool rational_add(StyleRational x, StyleRational y, StyleRational *out) {
    u128 divisor = gcd64(x.denominator, y.denominator);
    u128 left_scale = y.denominator / divisor;
    u128 right_scale = x.denominator / divisor;
    u128 denominator, left, right, numerator;
    if (__builtin_mul_overflow(x.denominator, left_scale, &denominator) ||
        __builtin_mul_overflow(x.numerator, left_scale, &left) ||
        __builtin_mul_overflow(y.numerator, right_scale, &right) || __builtin_add_overflow(left, right, &numerator)) {
        return false;
    }
    *out = reduce(numerator, denominator);
    return true;
}

/* x / count, exact and reduced; false on overflow. */
static bool rational_divide(StyleRational x, u128 count, StyleRational *out) {
    u128 denominator;
    if (__builtin_mul_overflow(x.denominator, count, &denominator)) {
        return false;
    }
    *out = reduce(x.numerator, denominator);
    return true;
}

/* The value, between 0 and 1, as the nearest fraction over 2^64, in lowest terms. */
static StyleRational approximate(long double value) {
    const u128 scale = (u128)1 << 64;
    long double scaled = value * 18446744073709551616.0L + 0.5L;
    u128 numerator = scaled <= 0 ? 0 : (u128)scaled;
    return reduce(numerator > scale ? scale : numerator, scale);
}

static long double value_of(StyleRational fraction) {
    return (long double)fraction.numerator / (long double)fraction.denominator;
}

/* One category of one model: statistics [first, first + count) of a sorted list. */
typedef struct {
    const StyleStatistic *items;
    size_t count;
} Slice;

/* Total variation distance between two slices (either may be empty only if both categories exist). */
static bool slice_distance(Slice a, Slice b, StyleRational *out) {
    u128 total_a = a.items[0].total, total_b = b.items[0].total;
    u128 common;
    if (__builtin_mul_overflow(total_a, total_b, &common)) {
        return false;
    }
    u128 sum = 0;
    size_t i = 0, j = 0;
    while (i < a.count || j < b.count) {
        int order = i == a.count ? 1 : (j == b.count ? -1 : strcmp(a.items[i].label, b.items[j].label));
        u128 count_a = order <= 0 ? a.items[i].count : 0;
        u128 count_b = order >= 0 ? b.items[j].count : 0;
        u128 left, right, difference;
        if (__builtin_mul_overflow(count_a, total_b, &left) || __builtin_mul_overflow(count_b, total_a, &right)) {
            return false;
        }
        difference = left > right ? left - right : right - left;
        if (__builtin_add_overflow(sum, difference, &sum)) {
            return false;
        }
        if (order <= 0) {
            i++;
        }
        if (order >= 0) {
            j++;
        }
    }
    u128 denominator;
    if (__builtin_mul_overflow(common, 2, &denominator)) {
        return false;
    }
    *out = reduce(sum, denominator);
    return true;
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
    StyleRational dimension_sum[STYLE_DIMENSION_COUNT] = {{0, 1}, {0, 1}, {0, 1}, {0, 1}};
    u128 dimension_categories[STYLE_DIMENSION_COUNT] = {0};
    long double dimension_value[STYLE_DIMENSION_COUNT] = {0};
    bool sum_exact[STYLE_DIMENSION_COUNT] = {true, true, true, true};
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
            entry->distance = (StyleRational){1, 1};
        }
        if (!top_labels(slice_a, entry->top_a) || !top_labels(slice_b, entry->top_b)) {
            result = STYLE_COMPARE_ERR_ALLOC;
            goto done;
        }
        /* A category's own distance is exact or an error; the means over categories may need more bits than 128 and
           then fall back to the nearest fraction over 2^64. */
        dimension_value[entry->dimension] += value_of(entry->distance);
        if (sum_exact[entry->dimension] &&
            !rational_add(dimension_sum[entry->dimension], entry->distance, &dimension_sum[entry->dimension])) {
            sum_exact[entry->dimension] = false;
        }
        dimension_categories[entry->dimension]++;
        if (order <= 0) {
            i = end_a;
        }
        if (order >= 0) {
            j = end_b;
        }
    }
    StyleRational overall_sum = {0, 1};
    long double overall_value = 0;
    bool overall_sum_exact = true;
    u128 dimensions = 0;
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (dimension_categories[d] == 0) {
            continue;
        }
        out->dimension_present[d] = true;
        out->dimension_exact[d] = sum_exact[d] && rational_divide(dimension_sum[d], dimension_categories[d], &out->dimension[d]);
        if (!out->dimension_exact[d]) {
            out->dimension[d] = approximate(dimension_value[d] / (long double)dimension_categories[d]);
        }
        overall_value += value_of(out->dimension[d]);
        overall_sum_exact = overall_sum_exact && out->dimension_exact[d] && rational_add(overall_sum, out->dimension[d], &overall_sum);
        dimensions++;
    }
    if (dimensions > 0) {
        out->overall_exact = overall_sum_exact && rational_divide(overall_sum, dimensions, &out->overall);
        if (!out->overall_exact) {
            out->overall = approximate(overall_value / (long double)dimensions);
        }
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
