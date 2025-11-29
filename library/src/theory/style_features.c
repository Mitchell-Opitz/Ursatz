#include "theory/style_features.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_separator(const char *text) {
    return strchr(text, '\t') != NULL || strchr(text, '\n') != NULL;
}

static StyleFeaturesResult validate(const StyleStatistic *statistic) {
    if (statistic->category == NULL || statistic->label == NULL) {
        return STYLE_FEATURES_ERR_NULL_ARGUMENT;
    }
    if (statistic->category[0] == '\0' || statistic->label[0] == '\0' || has_separator(statistic->category) ||
        has_separator(statistic->label) || statistic->total == 0 || statistic->count > statistic->total) {
        return STYLE_FEATURES_ERR_INVALID_STATISTIC;
    }
    return STYLE_FEATURES_OK;
}

static int compare_pointers(const void *left, const void *right) {
    const StyleStatistic *a = *(const StyleStatistic *const *)left;
    const StyleStatistic *b = *(const StyleStatistic *const *)right;
    int by_category = strcmp(a->category, b->category);
    return by_category != 0 ? by_category : strcmp(a->label, b->label);
}

StyleFeaturesResult style_features_encode(const StyleStatistic *statistics, size_t count, Value **out) {
    if (out == NULL || (count > 0 && statistics == NULL)) {
        return STYLE_FEATURES_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < count; i++) {
        StyleFeaturesResult status = validate(&statistics[i]);
        if (status != STYLE_FEATURES_OK) {
            return status;
        }
    }

    const StyleStatistic **sorted = NULL;
    if (count > 0) {
        sorted = (const StyleStatistic **)malloc(count * sizeof(*sorted));
        if (sorted == NULL) {
            return STYLE_FEATURES_ERR_ALLOC;
        }
        for (size_t i = 0; i < count; i++) {
            sorted[i] = &statistics[i];
        }
        qsort(sorted, count, sizeof(*sorted), compare_pointers);
        for (size_t i = 1; i < count; i++) {
            if (compare_pointers(&sorted[i - 1], &sorted[i]) == 0) {
                free(sorted);
                return STYLE_FEATURES_ERR_DUPLICATE;
            }
        }
    }

    /* Each line is the two names, two decimal numbers (at most 20 digits each), three tabs' worth of separators. */
    size_t capacity = 1;
    for (size_t i = 0; i < count; i++) {
        capacity += strlen(sorted[i]->category) + strlen(sorted[i]->label) + 2 * 20 + 4;
    }
    char *text = (char *)malloc(capacity);
    if (text == NULL) {
        free(sorted);
        return STYLE_FEATURES_ERR_ALLOC;
    }
    size_t length = 0;
    for (size_t i = 0; i < count; i++) {
        length += (size_t)snprintf(text + length, capacity - length, "%s\t%s\t%lu\t%lu\n", sorted[i]->category,
                                   sorted[i]->label, sorted[i]->count, sorted[i]->total);
    }
    free(sorted);

    Value *value = NULL;
    ValueResult created = value_create(STYLE_FEATURES_VALUE_TYPE, text, length, &value);
    free(text);
    if (created != VALUE_OK) {
        return STYLE_FEATURES_ERR_ALLOC;
    }
    *out = value;
    return STYLE_FEATURES_OK;
}

void style_statistics_free(StyleStatistic *statistics, size_t count) {
    if (statistics == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free((char *)statistics[i].category);
        free((char *)statistics[i].label);
    }
    free(statistics);
}

/* Parses one unsigned decimal ending at *cursor's next stop character; returns 0 on anything else. */
static int parse_number(char **cursor, char stop, unsigned long *out) {
    char *text = *cursor;
    if (*text < '0' || *text > '9') {
        return 0;
    }
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != stop) {
        return 0;
    }
    *out = value;
    *cursor = end + 1;
    return 1;
}

StyleFeaturesResult style_features_decode(const Value *features, StyleStatistic **out, size_t *out_count) {
    if (features == NULL || out == NULL || out_count == NULL) {
        return STYLE_FEATURES_ERR_NULL_ARGUMENT;
    }
    if (strcmp(value_type(features), STYLE_FEATURES_VALUE_TYPE) != 0) {
        return STYLE_FEATURES_ERR_MALFORMED;
    }
    size_t size = 0;
    const void *content = value_content(features, &size);
    char *text = (char *)malloc(size + 1);
    if (text == NULL) {
        return STYLE_FEATURES_ERR_ALLOC;
    }
    if (size > 0) {
        memcpy(text, content, size);
    }
    text[size] = '\0';

    StyleStatistic *items = NULL;
    size_t count = 0;
    StyleFeaturesResult result = STYLE_FEATURES_OK;
    char *cursor = text;
    while (*cursor != '\0' && result == STYLE_FEATURES_OK) {
        char *category = cursor;
        char *tab = strchr(category, '\t');
        char *label = tab == NULL ? NULL : tab + 1;
        char *second_tab = label == NULL ? NULL : strchr(label, '\t');
        if (second_tab == NULL) {
            result = STYLE_FEATURES_ERR_MALFORMED;
            break;
        }
        *tab = '\0';
        *second_tab = '\0';
        char *numbers = second_tab + 1;
        unsigned long statistic_count = 0, statistic_total = 0;
        if (!parse_number(&numbers, '\t', &statistic_count) || !parse_number(&numbers, '\n', &statistic_total)) {
            result = STYLE_FEATURES_ERR_MALFORMED;
            break;
        }
        cursor = numbers;

        StyleStatistic statistic = {category, label, statistic_count, statistic_total};
        if (validate(&statistic) != STYLE_FEATURES_OK) {
            result = STYLE_FEATURES_ERR_MALFORMED;
            break;
        }
        if (count > 0) {
            const StyleStatistic *previous_pointer = &items[count - 1];
            const StyleStatistic *current_pointer = &statistic;
            if (compare_pointers(&previous_pointer, &current_pointer) >= 0) {
                result = STYLE_FEATURES_ERR_MALFORMED;
                break;
            }
        }
        StyleStatistic *grown = (StyleStatistic *)realloc(items, (count + 1) * sizeof(*grown));
        if (grown == NULL) {
            result = STYLE_FEATURES_ERR_ALLOC;
            break;
        }
        items = grown;
        items[count].category = strdup(category);
        items[count].label = strdup(label);
        items[count].count = statistic_count;
        items[count].total = statistic_total;
        count++;
        if (items[count - 1].category == NULL || items[count - 1].label == NULL) {
            result = STYLE_FEATURES_ERR_ALLOC;
        }
    }
    free(text);

    if (result != STYLE_FEATURES_OK) {
        style_statistics_free(items, count);
        return result;
    }
    *out = items;
    *out_count = count;
    return STYLE_FEATURES_OK;
}
