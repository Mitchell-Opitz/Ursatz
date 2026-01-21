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

/* The motive categories of the Analyzer's style model. The Composer reads them out until P16 makes them preferences. */
const char *const composer_style_motive_categories[COMPOSER_STYLE_MOTIVE_CATEGORY_COUNT] = {
    "motive-degrees", "motive-intervals", "motive-rhythm", "motive-nct-figure"};

struct ComposerStyle {
    StyleModel *model;
    size_t pieces;
    StyleCategory *categories; /* sorted by name, byte order */
    size_t category_count;
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
    if (!out) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!path) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    ComposerStyle *style = calloc(1, sizeof(*style));
    if (!style) {
        return COMPOSER_STYLE_ERR_ALLOC;
    }
    StyleModelFileResult read = style_model_file_read_without(
        path, composer_style_motive_categories, COMPOSER_STYLE_MOTIVE_CATEGORY_COUNT, &style->model, &style->pieces);
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
    if (result != COMPOSER_STYLE_OK) {
        composer_style_destroy(style);
        return result;
    }
    *out = style;
    return COMPOSER_STYLE_OK;
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
