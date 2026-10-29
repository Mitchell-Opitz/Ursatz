#include "analysis/motive_link.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void motive_link_options_default(MotiveLinkOptions *options) {
    options->max_depth = 2;
    options->beam = 64;
    options->min_nearness = 0.5;
    options->contour = false;
}

double motive_link_nearness(const MotiveSketch *a, const MotiveSketch *b) {
    MotiveDimensions dimensions;
    motive_variant_compare(a, b, &dimensions);
    return motive_variant_nearness(&dimensions);
}

static MotiveLinkResult from_recognize(MotiveRecognizeResult result) {
    return result == MOTIVE_RECOGNIZE_NO_MEMORY ? MOTIVE_LINK_NO_MEMORY : MOTIVE_LINK_INVALID;
}

MotiveLinkResult motive_link_pair(const MotiveSketch *a, const MotiveSketch *b, const MotiveLinkOptions *options,
                                  MotiveLink *out) {
    MotiveLinkOptions defaults;
    motive_link_options_default(&defaults);
    if (!options) {
        options = &defaults;
    }
    if (!out || !motive_sketch_validate(a) || !motive_sketch_validate(b) || options->max_depth < 1 ||
        options->max_depth > MOTIVE_RECOGNIZE_MAX_DEPTH || options->beam < 1 ||
        options->beam > MOTIVE_RECOGNIZE_MAX_BEAM || !(options->min_nearness >= 0.0) ||
        !(options->min_nearness <= 1.0)) {
        return MOTIVE_LINK_INVALID;
    }
    MotiveLink link;
    memset(&link, 0, sizeof(link));
    link.nearness = motive_link_nearness(a, b);
    if (link.nearness < options->min_nearness) {
        link.gated = true;
        *out = link;
        return MOTIVE_LINK_OK;
    }
    MotiveRecognizeOptions search;
    motive_recognize_options_default(&search);
    search.max_depth = options->max_depth;
    search.beam = options->beam;
    search.contour = options->contour;
    MotiveRecognizeResult result = motive_variant_recognize(a, b, &search, &link.recognition);
    if (result != MOTIVE_RECOGNIZE_OK) {
        return from_recognize(result);
    }
    if (link.recognition.found) {
        link.linked = true;
        *out = link;
        return MOTIVE_LINK_OK;
    }
    MotiveRecognition back;
    result = motive_variant_recognize(b, a, &search, &back);
    if (result != MOTIVE_RECOGNIZE_OK) {
        return from_recognize(result);
    }
    if (back.found) {
        link.linked = true;
        link.reversed = true;
        link.recognition = back;
    }
    *out = link;
    return MOTIVE_LINK_OK;
}

typedef struct {
    size_t sketch;
    double nearness;
} Candidate;

/* Nearest first; equal nearness by index. */
static int compare_candidates(const void *left, const void *right) {
    const Candidate *a = left;
    const Candidate *b = right;
    if (a->nearness != b->nearness) {
        return a->nearness > b->nearness ? -1 : 1;
    }
    return a->sketch < b->sketch ? -1 : (a->sketch > b->sketch ? 1 : 0);
}

static int compare_pairs(const void *left, const void *right) {
    const MotiveLinkPair *a = left;
    const MotiveLinkPair *b = right;
    if (a->low != b->low) {
        return a->low < b->low ? -1 : 1;
    }
    return a->high < b->high ? -1 : (a->high > b->high ? 1 : 0);
}

MotiveLinkResult motive_link_candidates(const MotiveSketch *sketches, const size_t *groups, size_t count,
                                        size_t per_sketch, double min_nearness, MotiveLinkPair **out,
                                        size_t *out_count) {
    if (out == NULL || out_count == NULL || (count > 0 && (sketches == NULL || groups == NULL)) || per_sketch < 1 ||
        !(min_nearness >= 0.0) || !(min_nearness <= 1.0)) {
        return MOTIVE_LINK_INVALID;
    }
    if (count == 0) {
        *out = NULL;
        *out_count = 0;
        return MOTIVE_LINK_OK;
    }
    size_t per = per_sketch < count ? per_sketch : count;
    if (count > SIZE_MAX / per / sizeof(MotiveLinkPair)) {
        return MOTIVE_LINK_NO_MEMORY;
    }
    Candidate *candidates = malloc(count * sizeof(*candidates));
    MotiveLinkPair *pairs = malloc(count * per * sizeof(*pairs));
    if (candidates == NULL || pairs == NULL) {
        free(candidates);
        free(pairs);
        return MOTIVE_LINK_NO_MEMORY;
    }
    size_t total = 0;
    for (size_t f = 0; f < count; f++) {
        size_t found = 0;
        for (size_t g = 0; g < count; g++) {
            if (groups[g] == groups[f]) {
                continue;
            }
            double nearness = motive_link_nearness(&sketches[f], &sketches[g]);
            if (nearness >= min_nearness) {
                candidates[found].sketch = g;
                candidates[found].nearness = nearness;
                found++;
            }
        }
        qsort(candidates, found, sizeof(*candidates), compare_candidates);
        if (found > per) {
            found = per;
        }
        for (size_t c = 0; c < found; c++) {
            size_t g = candidates[c].sketch;
            pairs[total].low = f < g ? f : g;
            pairs[total].high = f < g ? g : f;
            total++;
        }
    }
    free(candidates);
    qsort(pairs, total, sizeof(*pairs), compare_pairs);
    size_t unique = 0;
    for (size_t i = 0; i < total; i++) {
        if (unique == 0 || compare_pairs(&pairs[unique - 1], &pairs[i]) != 0) {
            pairs[unique++] = pairs[i];
        }
    }
    if (unique == 0) {
        free(pairs);
        pairs = NULL;
    }
    *out = pairs;
    *out_count = unique;
    return MOTIVE_LINK_OK;
}

static size_t find_root(size_t *parent, size_t node) {
    while (parent[node] != node) {
        parent[node] = parent[parent[node]];
        node = parent[node];
    }
    return node;
}

MotiveLinkResult motive_link_groups(size_t count, const MotiveLinkPair *pairs, const bool *linked, size_t pair_count,
                                    size_t **group_of, size_t *group_count) {
    if (group_of == NULL || group_count == NULL || (pair_count > 0 && (pairs == NULL || linked == NULL))) {
        return MOTIVE_LINK_INVALID;
    }
    for (size_t p = 0; p < pair_count; p++) {
        if (pairs[p].low >= pairs[p].high || pairs[p].high >= count) {
            return MOTIVE_LINK_INVALID;
        }
    }
    if (count == 0) {
        *group_of = NULL;
        *group_count = 0;
        return MOTIVE_LINK_OK;
    }
    size_t *parent = malloc(count * sizeof(*parent));
    size_t *number = malloc(count * sizeof(*number));
    bool *touched = calloc(count, sizeof(*touched));
    size_t *result = malloc(count * sizeof(*result));
    if (parent == NULL || number == NULL || touched == NULL || result == NULL) {
        free(parent);
        free(number);
        free(touched);
        free(result);
        return MOTIVE_LINK_NO_MEMORY;
    }
    for (size_t i = 0; i < count; i++) {
        parent[i] = i;
        number[i] = (size_t)-1;
    }
    for (size_t p = 0; p < pair_count; p++) {
        if (!linked[p]) {
            continue;
        }
        size_t low = find_root(parent, pairs[p].low);
        size_t high = find_root(parent, pairs[p].high);
        if (low != high) {
            parent[high] = low;
        }
        touched[pairs[p].low] = true;
        touched[pairs[p].high] = true;
    }
    size_t groups = 0;
    for (size_t i = 0; i < count; i++) {
        result[i] = (size_t)-1;
        if (!touched[i]) {
            continue;
        }
        size_t root = find_root(parent, i);
        if (number[root] == (size_t)-1) {
            number[root] = groups++;
        }
        result[i] = number[root];
    }
    free(parent);
    free(number);
    free(touched);
    *group_of = result;
    *group_count = groups;
    return MOTIVE_LINK_OK;
}
