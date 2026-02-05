#include <ursatz_composer/style.h>

#include "style_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *label;
    unsigned long count;
} StyleLabel;

typedef struct {
    char *name;
    unsigned long total;
    unsigned long largest;
    StyleLabel *labels; /* sorted by label, byte order */
    size_t label_count;
} StyleCategory;

struct ComposerStyle {
    StyleModel *model;
    size_t pieces;
    StyleCategory *categories; /* sorted by name, byte order */
    size_t category_count;
    char **without; /* the categories the style was read without, in the order given, no repeats */
    size_t without_count;
};

const char *composer_style_result_name(ComposerStyleResult result) {
    switch (result) {
    case COMPOSER_STYLE_OK:
        return "ok";
    case COMPOSER_STYLE_ERR_ARGUMENT:
        return "bad argument";
    case COMPOSER_STYLE_ERR_IO:
        return "file not readable";
    case COMPOSER_STYLE_ERR_FORMAT:
        return "not a style model file";
    case COMPOSER_STYLE_ERR_ALLOC:
        return "out of memory";
    }
    return "unknown";
}

void composer_style_destroy(ComposerStyle *style) {
    if (!style) {
        return;
    }
    for (size_t c = 0; c < style->category_count; c++) {
        for (size_t l = 0; l < style->categories[c].label_count; l++) {
            free(style->categories[c].labels[l].label);
        }
        free(style->categories[c].labels);
        free(style->categories[c].name);
    }
    free(style->categories);
    for (size_t i = 0; i < style->without_count; i++) {
        free(style->without[i]);
    }
    free(style->without);
    style_model_destroy(style->model);
    free(style);
}

/* Adds one statistic. The statistics come sorted by category then label, so a category is the last one or a new one. */
static bool add_statistic(ComposerStyle *style, const StyleStatistic *statistic) {
    StyleCategory *category = style->category_count ? &style->categories[style->category_count - 1] : NULL;
    if (!category || strcmp(category->name, statistic->category) != 0) {
        StyleCategory *grown = realloc(style->categories, (style->category_count + 1) * sizeof(*grown));
        if (!grown) {
            return false;
        }
        style->categories = grown;
        category = &grown[style->category_count];
        memset(category, 0, sizeof(*category));
        category->name = strdup(statistic->category);
        if (!category->name) {
            return false;
        }
        category->total = statistic->total;
        style->category_count++;
    }
    StyleLabel *labels = realloc(category->labels, (category->label_count + 1) * sizeof(*labels));
    if (!labels) {
        return false;
    }
    category->labels = labels;
    labels[category->label_count].label = strdup(statistic->label);
    if (!labels[category->label_count].label) {
        return false;
    }
    labels[category->label_count].count = statistic->count;
    category->label_count++;
    if (statistic->count > category->largest) {
        category->largest = statistic->count;
    }
    return true;
}

/* Reads the statistics of style->model into its categories. */
static ComposerStyleResult read_statistics(ComposerStyle *style) {
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleModelFileResult decoded = style_model_statistics(style->model, &statistics, &count);
    bool ok = decoded == STYLE_MODEL_FILE_OK;
    for (size_t i = 0; ok && i < count; i++) {
        ok = add_statistic(style, &statistics[i]);
    }
    if (decoded == STYLE_MODEL_FILE_OK) {
        style_statistics_free(statistics, count);
    }
    if (!ok) {
        return decoded == STYLE_MODEL_FILE_OK || decoded == STYLE_MODEL_FILE_ERR_ALLOC ? COMPOSER_STYLE_ERR_ALLOC
                                                                                       : COMPOSER_STYLE_ERR_FORMAT;
    }
    return COMPOSER_STYLE_OK;
}

ComposerStyleResult composer_style_from_model(StyleModel *model, size_t pieces, ComposerStyle **out) {
    if (!out) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!model) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    ComposerStyle *style = calloc(1, sizeof(*style));
    if (!style) {
        return COMPOSER_STYLE_ERR_ALLOC;
    }
    style->model = model;
    style->pieces = pieces;
    ComposerStyleResult read = read_statistics(style);
    if (read != COMPOSER_STYLE_OK) {
        style->model = NULL; /* the caller keeps the model on failure */
        composer_style_destroy(style);
        return read;
    }
    *out = style;
    return COMPOSER_STYLE_OK;
}

ComposerStyleResult composer_style_load(const char *path, ComposerStyle **out) {
    return composer_style_load_without(path, NULL, 0, out);
}

ComposerStyleResult composer_style_load_without(const char *path, const char *const *categories, size_t count,
                                                ComposerStyle **out) {
    if (!out) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!path || (count > 0 && !categories)) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    ComposerStyle *style = calloc(1, sizeof(*style));
    if (!style) {
        return COMPOSER_STYLE_ERR_ALLOC;
    }
    StyleModelFileResult read = style_model_file_read_without(path, categories, count, &style->model, &style->pieces);
    if (read != STYLE_MODEL_FILE_OK) {
        free(style);
        switch (read) {
        case STYLE_MODEL_FILE_ERR_IO:
            return COMPOSER_STYLE_ERR_IO;
        case STYLE_MODEL_FILE_ERR_ALLOC:
            return COMPOSER_STYLE_ERR_ALLOC;
        case STYLE_MODEL_FILE_ERR_ARGUMENT:
            return COMPOSER_STYLE_ERR_ARGUMENT;
        default:
            return COMPOSER_STYLE_ERR_FORMAT;
        }
    }
    ComposerStyleResult result = read_statistics(style);
    if (result == COMPOSER_STYLE_OK) {
        result = composer_style_add_without(style, categories, count);
    }
    if (result != COMPOSER_STYLE_OK) {
        composer_style_destroy(style);
        return result;
    }
    *out = style;
    return COMPOSER_STYLE_OK;
}

ComposerStyleResult composer_style_add_without(ComposerStyle *style, const char *const *categories, size_t count) {
    if (!style || (count > 0 && !categories)) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < count; i++) {
        if (!categories[i]) {
            return COMPOSER_STYLE_ERR_ARGUMENT;
        }
        if (composer_style_without_has(style, categories[i])) {
            continue;
        }
        char **grown = realloc(style->without, (style->without_count + 1) * sizeof(*grown));
        if (!grown) {
            return COMPOSER_STYLE_ERR_ALLOC;
        }
        style->without = grown;
        grown[style->without_count] = strdup(categories[i]);
        if (!grown[style->without_count]) {
            return COMPOSER_STYLE_ERR_ALLOC;
        }
        style->without_count++;
    }
    return COMPOSER_STYLE_OK;
}

bool composer_style_without_has(const ComposerStyle *style, const char *category) {
    if (!style || !category) {
        return false;
    }
    for (size_t i = 0; i < style->without_count; i++) {
        if (strcmp(style->without[i], category) == 0) {
            return true;
        }
    }
    return false;
}

size_t composer_style_without_count(const ComposerStyle *style) {
    return style ? style->without_count : 0;
}

const char *composer_style_without_at(const ComposerStyle *style, size_t i) {
    return style && i < style->without_count ? style->without[i] : NULL;
}

const StyleModel *composer_style_model(const ComposerStyle *style) {
    return style ? style->model : NULL;
}

size_t composer_style_pieces(const ComposerStyle *style) {
    return style ? style->pieces : 0;
}

static int compare_category(const void *key, const void *item) {
    return strcmp((const char *)key, ((const StyleCategory *)item)->name);
}

static int compare_label(const void *key, const void *item) {
    return strcmp((const char *)key, ((const StyleLabel *)item)->label);
}

bool composer_style_share(const ComposerStyle *style, const char *category, const char *label, ComposerStyleShare *out) {
    if (!style || !category || !label || !out) {
        return false;
    }
    const StyleCategory *found =
        bsearch(category, style->categories, style->category_count, sizeof(StyleCategory), compare_category);
    if (!found) {
        return false;
    }
    const StyleLabel *seen = bsearch(label, found->labels, found->label_count, sizeof(StyleLabel), compare_label);
    out->count = seen ? seen->count : 0;
    out->total = found->total;
    out->largest = found->largest;
    return true;
}

static const StyleCategory *find_category(const ComposerStyle *style, const char *name) {
    if (!style) {
        return NULL;
    }
    return bsearch(name, style->categories, style->category_count, sizeof(StyleCategory), compare_category);
}

/* The label of a category with the largest count, the lowest value among ties; NULL when none reads. */
static const StyleLabel *most_common_label(const StyleCategory *category, bool (*value)(const char *, unsigned long *),
                                           unsigned long *best_value) {
    const StyleLabel *best = NULL;
    for (size_t i = 0; category && i < category->label_count; i++) {
        const StyleLabel *label = &category->labels[i];
        unsigned long v = 0;
        if (label->count == 0 || !value(label->label, &v)) {
            continue;
        }
        if (!best || label->count > best->count || (label->count == best->count && v < *best_value)) {
            best = label;
            *best_value = v;
        }
    }
    return best;
}

/* The middle of a tempo band, in quarter notes per minute: "<40" is 30, "140-159" is 150, "220+" is 230. */
static bool tempo_band_middle(const char *text, unsigned long *out) {
    if (text[0] == '<') {
        *out = 30;
        return true;
    }
    char *end = NULL;
    unsigned long low = strtoul(text, &end, 10);
    if (end == text || (*end != '-' && *end != '+')) {
        return false;
    }
    *out = low + 10;
    return true;
}

bool composer_style_tempo(const ComposerStyle *style, ComposerRatio *out) {
    unsigned long middle = 0;
    if (!out || !most_common_label(find_category(style, STYLE_CATEGORY_TEMPO), tempo_band_middle, &middle)) {
        return false;
    }
    out->numerator = middle;
    out->denominator = 1;
    return true;
}

static bool mode_value(const char *text, unsigned long *out) {
    if (strcmp(text, "major") == 0) {
        *out = 0;
        return true;
    }
    if (strcmp(text, "minor") == 0) {
        *out = 1;
        return true;
    }
    return false;
}

bool composer_style_mode(const ComposerStyle *style, bool *minor) {
    unsigned long value = 0;
    if (!minor || !most_common_label(find_category(style, STYLE_CATEGORY_MODE), mode_value, &value)) {
        return false;
    }
    *minor = value == 1;
    return true;
}

/* Attacks per beat times 2 ("1/2" is 1, "1" is 2, "3/2" is 3, "4+" is 8), so that the order is a number's. */
static bool density_halves(const char *text, unsigned long *out) {
    if (strcmp(text, "4+") == 0) {
        *out = 8;
        return true;
    }
    char *end = NULL;
    unsigned long top = strtoul(text, &end, 10);
    if (end == text) {
        return false;
    }
    if (*end == '\0') {
        *out = top * 2;
        return true;
    }
    if (strcmp(end, "/2") == 0) {
        *out = top;
        return true;
    }
    return false;
}

bool composer_style_density(const ComposerStyle *style, ComposerRatio *out) {
    unsigned long halves = 0;
    if (!out || !most_common_label(find_category(style, STYLE_CATEGORY_NOTE_DENSITY), density_halves, &halves)) {
        return false;
    }
    out->numerator = halves;
    out->denominator = 2;
    return true;
}
