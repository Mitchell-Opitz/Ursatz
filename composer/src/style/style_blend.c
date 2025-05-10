#include <ursatz_composer/style.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "theory/style_model_blend.h"
#include "theory/style_model_build.h"

__extension__ typedef unsigned __int128 Wide;

/* A category's statistics are brought to one total: the least common multiple of its labels' totals when that is below
   this, else this. */
#define COMMON_TOTAL_LIMIT ((Wide)1 << 40)
#define COMMON_TOTAL_ROUNDED ((Wide)1 << 30)

ComposerStyleWeights composer_style_weights_all(ComposerRatio weight) {
    ComposerStyleWeights weights;
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        weights.numerator[d] = weight.numerator;
        weights.denominator[d] = weight.denominator;
    }
    return weights;
}

static Wide gcd_wide(Wide a, Wide b) {
    while (b) {
        Wide rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

static ComposerStyleResult from_blend(StyleBlendResult result) {
    switch (result) {
    case STYLE_BLEND_OK:
        return COMPOSER_STYLE_OK;
    case STYLE_BLEND_ERR_NULL_ARGUMENT:
    case STYLE_BLEND_ERR_WEIGHT_ZERO_DENOMINATOR:
    case STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE:
        return COMPOSER_STYLE_ERR_ARGUMENT;
    case STYLE_BLEND_ERR_ALLOC:
        return COMPOSER_STYLE_ERR_ALLOC;
    default:
        return COMPOSER_STYLE_ERR_FORMAT;
    }
}

static uint64_t fnv1a(uint64_t hash, const char *text) {
    for (; *text; text++) {
        hash = (hash ^ (unsigned char)*text) * 1099511628211ULL;
    }
    return hash;
}

/* The statistics of one dimension out of a model's, which are sorted by category: borrowed strings, in one array. */
static StyleStatistic *dimension_statistics(const StyleStatistic *all, size_t count, StyleDimension dimension,
                                            size_t *out_count) {
    StyleStatistic *picked = malloc((count + 1) * sizeof(*picked));
    size_t n = 0;
    if (!picked) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        StyleDimension found;
        if (style_category_dimension(all[i].category, &found) && found == dimension) {
            picked[n++] = all[i];
        }
    }
    *out_count = n;
    return picked;
}

/* A model of the picked statistics, named for the part it is. */
static ComposerStyleResult part_model(const StyleModel *from, StyleDimension dimension, const char *side,
                                      const StyleStatistic *all, size_t all_count, StyleModel **out) {
    size_t count = 0;
    StyleStatistic *picked = dimension_statistics(all, all_count, dimension, &count);
    if (!picked) {
        return COMPOSER_STYLE_ERR_ALLOC;
    }
    char text[64];
    snprintf(text, sizeof text, "blend-part-%s-%s", side, style_dimension_name(dimension));
    EntityID *id = NULL;
    ComposerStyleResult result = COMPOSER_STYLE_ERR_ALLOC;
    if (entity_id_create(text, &id) == ENTITY_ID_OK) {
        StyleModelBuildResult built = style_model_from_statistics(id, style_model_style_name(from),
                                                                  style_model_training_corpus(from),
                                                                  style_model_model_version(from), picked, count, out);
        if (built == STYLE_MODEL_BUILD_OK) {
            result = COMPOSER_STYLE_OK;
        } else {
            entity_id_destroy(id);
            result = built == STYLE_MODEL_BUILD_ERR_ALLOC ? COMPOSER_STYLE_ERR_ALLOC : COMPOSER_STYLE_ERR_FORMAT;
        }
    }
    free(picked);
    return result;
}

/*
 * The statistics [start, end) of one category brought to one total, appended to out. The blended frequencies come as
 * reduced fractions with their own denominators; the Analyzer compares categories whose labels share a total, so they
 * are put over the least common multiple, or over 2^30 (counts rounded, a label that rounds to nothing left out) when
 * that is too large.
 */
static bool normalize_category(const StyleStatistic *items, size_t start, size_t end, StyleStatistic *out,
                               size_t *out_count) {
    Wide common = 1;
    for (size_t i = start; i < end; i++) {
        Wide total = items[i].total;
        common = common / gcd_wide(common, total) * total;
        if (common > COMMON_TOTAL_LIMIT) {
            common = COMMON_TOTAL_ROUNDED;
            break;
        }
    }
    bool exact = common != COMMON_TOTAL_ROUNDED;
    for (size_t i = start; exact && i < end; i++) {
        exact = common % items[i].total == 0;
    }
    for (size_t i = start; i < end; i++) {
        Wide scaled = (Wide)items[i].count * common;
        Wide count = exact ? scaled / items[i].total : (scaled + items[i].total / 2) / items[i].total;
        if (count == 0) {
            continue;
        }
        out[*out_count] = items[i];
        out[*out_count].count = (unsigned long)count;
        out[*out_count].total = (unsigned long)common;
        (*out_count)++;
    }
    return true;
}

ComposerStyleResult composer_style_blend(const ComposerStyle *a, const ComposerStyle *b,
                                         const ComposerStyleWeights *weights, ComposerStyle **out) {
    if (!out) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!a || !b || !weights) {
        return COMPOSER_STYLE_ERR_ARGUMENT;
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (weights->denominator[d] == 0 || weights->numerator[d] > weights->denominator[d]) {
            return COMPOSER_STYLE_ERR_ARGUMENT;
        }
    }
    const StyleModel *model_a = composer_style_model(a);
    const StyleModel *model_b = composer_style_model(b);
    StyleStatistic *stats_a = NULL, *stats_b = NULL;
    size_t count_a = 0, count_b = 0;
    StyleStatistic *decoded[STYLE_DIMENSION_COUNT] = {NULL};
    size_t decoded_count[STYLE_DIMENSION_COUNT] = {0};
    StyleStatistic *merged = NULL;
    size_t merged_count = 0;
    size_t merged_capacity = 0;
    char *name = NULL, *corpus = NULL;
    StyleModel *final_model = NULL;
    ComposerStyleResult result = COMPOSER_STYLE_ERR_ALLOC;

    if (style_model_statistics(model_a, &stats_a, &count_a) != STYLE_MODEL_FILE_OK ||
        style_model_statistics(model_b, &stats_b, &count_b) != STYLE_MODEL_FILE_OK) {
        result = COMPOSER_STYLE_ERR_FORMAT;
        goto done;
    }
    for (StyleDimension d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        StyleModel *part_a = NULL, *part_b = NULL, *blended = NULL;
        result = part_model(model_a, d, "a", stats_a, count_a, &part_a);
        if (result == COMPOSER_STYLE_OK) {
            result = part_model(model_b, d, "b", stats_b, count_b, &part_b);
        }
        if (result == COMPOSER_STYLE_OK) {
            char text[64];
            EntityID *id = NULL;
            snprintf(text, sizeof text, "blend-%s", style_dimension_name(d));
            if (entity_id_create(text, &id) != ENTITY_ID_OK) {
                result = COMPOSER_STYLE_ERR_ALLOC;
            } else {
                StyleBlendResult blend = style_model_blend(id, style_model_style_name(model_a),
                                                           style_model_training_corpus(model_a),
                                                           style_model_model_version(model_a), part_a, part_b,
                                                           weights->numerator[d], weights->denominator[d], &blended);
                result = from_blend(blend);
                if (blend != STYLE_BLEND_OK) {
                    entity_id_destroy(id);
                }
            }
        }
        if (result == COMPOSER_STYLE_OK &&
            style_model_statistics(blended, &decoded[d], &decoded_count[d]) != STYLE_MODEL_FILE_OK) {
            result = COMPOSER_STYLE_ERR_FORMAT;
        }
        style_model_destroy(part_a);
        style_model_destroy(part_b);
        style_model_destroy(blended);
        if (result != COMPOSER_STYLE_OK) {
            goto done;
        }
        merged_capacity += decoded_count[d];
    }

    merged = malloc((merged_capacity + 1) * sizeof(*merged));
    if (!merged) {
        result = COMPOSER_STYLE_ERR_ALLOC;
        goto done;
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        size_t i = 0;
        while (i < decoded_count[d]) {
            size_t end = i + 1;
            while (end < decoded_count[d] && strcmp(decoded[d][end].category, decoded[d][i].category) == 0) {
                end++;
            }
            normalize_category(decoded[d], i, end, merged, &merged_count);
            i = end;
        }
    }

    const char *name_a = style_model_style_name(model_a);
    const char *name_b = style_model_style_name(model_b);
    const char *corpus_a = style_model_training_corpus(model_a);
    const char *corpus_b = style_model_training_corpus(model_b);
    name = malloc(strlen(name_a) + strlen(name_b) + 4);
    corpus = malloc(strlen(corpus_a) + strlen(corpus_b) + 2);
    if (!name || !corpus) {
        result = COMPOSER_STYLE_ERR_ALLOC;
        goto done;
    }
    sprintf(name, "%s + %s", name_a, name_b);
    sprintf(corpus, "%s;%s", corpus_a, corpus_b);

    uint64_t hash = fnv1a(1469598103934665603ULL, name);
    hash = fnv1a(hash, corpus);
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        char weight[64];
        snprintf(weight, sizeof weight, "|%lu/%lu", weights->numerator[d], weights->denominator[d]);
        hash = fnv1a(hash, weight);
    }
    char id_text[64];
    snprintf(id_text, sizeof id_text, "style-model-blend-%016llx", (unsigned long long)hash);
    EntityID *id = NULL;
    if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
        result = COMPOSER_STYLE_ERR_ALLOC;
        goto done;
    }
    StyleModelBuildResult built = style_model_from_statistics(id, name, corpus, style_model_model_version(model_a),
                                                              merged, merged_count, &final_model);
    if (built != STYLE_MODEL_BUILD_OK) {
        entity_id_destroy(id);
        result = built == STYLE_MODEL_BUILD_ERR_ALLOC ? COMPOSER_STYLE_ERR_ALLOC : COMPOSER_STYLE_ERR_FORMAT;
        goto done;
    }
    result = composer_style_from_model(final_model, composer_style_pieces(a) + composer_style_pieces(b), out);
    if (result != COMPOSER_STYLE_OK) {
        style_model_destroy(final_model);
    }

done:
    free(merged);
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        style_statistics_free(decoded[d], decoded_count[d]);
    }
    style_statistics_free(stats_a, count_a);
    style_statistics_free(stats_b, count_b);
    free(name);
    free(corpus);
    return result;
}
