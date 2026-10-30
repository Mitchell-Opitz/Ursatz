#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/motive_links.h>
#include <ursatz_analyzer/version.h>

#include "links_internal.h"
#include "style/style_internal.h"
#include "util/json_util.h"

#define CACHE_SCHEMA "ursatz-motive-link-cache"
#define CACHE_SCHEMA_VERSION 1

static const char *const dimension_keys[MOTIVE_DIMENSION_COUNT] = {"rhythm", "intervals", "contour", "harmony"};

static char *copy_text(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) {
        memcpy(copy, text, length);
    }
    return copy;
}

/* ---- In memory ---- */

const MotiveLinkCachePair *motive_link_cache_find(const MotiveLinkCache *cache, unsigned long long a,
                                                  unsigned long long b) {
    if (!cache) {
        return NULL;
    }
    unsigned long long low = a < b ? a : b;
    unsigned long long high = a < b ? b : a;
    size_t first = 0;
    size_t end = cache->count;
    while (first < end) {
        size_t middle = first + (end - first) / 2;
        const MotiveLinkCachePair *pair = &cache->pairs[middle];
        if (pair->a == low && pair->b == high) {
            return pair;
        }
        if (pair->a < low || (pair->a == low && pair->b < high)) {
            first = middle + 1;
        } else {
            end = middle;
        }
    }
    return NULL;
}

bool motive_link_cache_insert(MotiveLinkCache *cache, const MotiveLinkCachePair *pair) {
    size_t first = 0;
    size_t end = cache->count;
    while (first < end) {
        size_t middle = first + (end - first) / 2;
        const MotiveLinkCachePair *at = &cache->pairs[middle];
        if (at->a < pair->a || (at->a == pair->a && at->b < pair->b)) {
            first = middle + 1;
        } else {
            end = middle;
        }
    }
    if (first < cache->count && cache->pairs[first].a == pair->a && cache->pairs[first].b == pair->b) {
        cache->pairs[first] = *pair;
        return true;
    }
    if (cache->count == cache->capacity) {
        size_t grown = cache->capacity ? cache->capacity * 2 : 64;
        MotiveLinkCachePair *next = realloc(cache->pairs, grown * sizeof *next);
        if (!next) {
            return false;
        }
        cache->pairs = next;
        cache->capacity = grown;
    }
    memmove(&cache->pairs[first + 1], &cache->pairs[first], (cache->count - first) * sizeof *cache->pairs);
    cache->pairs[first] = *pair;
    cache->count++;
    return true;
}

static bool options_equal(const MotiveLinkOptions *a, const MotiveLinkOptions *b) {
    return a->max_depth == b->max_depth && a->beam == b->beam && a->min_nearness == b->min_nearness &&
           a->contour == b->contour;
}

bool motive_link_cache_prepare(MotiveLinkCache *cache, const MotiveLinkOptions *options, bool *reset) {
    *reset = false;
    if (cache->keyed && cache->ursatz_commit && cache->analyzer_version &&
        strcmp(cache->ursatz_commit, URSATZ_ANALYZER_URSATZ_COMMIT) == 0 &&
        strcmp(cache->analyzer_version, URSATZ_ANALYZER_VERSION) == 0 && options_equal(&cache->options, options)) {
        return true;
    }
    char *commit = copy_text(URSATZ_ANALYZER_URSATZ_COMMIT);
    char *version = copy_text(URSATZ_ANALYZER_VERSION);
    if (!commit || !version) {
        free(commit);
        free(version);
        return false;
    }
    if (cache->keyed) {
        cache->count = 0;
        *reset = true;
    }
    free(cache->ursatz_commit);
    free(cache->analyzer_version);
    cache->ursatz_commit = commit;
    cache->analyzer_version = version;
    cache->options = *options;
    cache->keyed = true;
    return true;
}

void motive_link_cache_destroy(MotiveLinkCache *cache) {
    if (!cache) {
        return;
    }
    free(cache->pairs);
    free(cache->ursatz_commit);
    free(cache->analyzer_version);
    free(cache);
}

size_t motive_link_cache_pair_count(const MotiveLinkCache *cache) {
    return cache ? cache->count : 0;
}

/* ---- File ---- */

static bool whole_number(const cJSON *item, size_t *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > 4294967295.0 ||
        item->valuedouble != (double)(unsigned long)item->valuedouble) {
        return false;
    }
    *out = (size_t)item->valuedouble;
    return true;
}

static bool fingerprint_value(const cJSON *item, unsigned long long *out) {
    if (!cJSON_IsString(item) || strlen(item->valuestring) != 16) {
        return false;
    }
    char *end;
    errno = 0;
    *out = strtoull(item->valuestring, &end, 16);
    return *end == '\0' && errno == 0 && item->valuestring[0] != '-' && item->valuestring[0] != '+';
}

static bool distance_value(const cJSON *item, unsigned long long *numerator, unsigned long long *denominator) {
    char tail;
    return cJSON_IsString(item) && sscanf(item->valuestring, "%llu/%llu%c", numerator, denominator, &tail) == 2 &&
           *denominator != 0;
}

static bool read_pair(const cJSON *entry, MotiveLinkCachePair *pair) {
    memset(pair, 0, sizeof *pair);
    const cJSON *gated = json_member(entry, "gated");
    const cJSON *linked = json_member(entry, "linked");
    const cJSON *reversed = json_member(entry, "reversed");
    const cJSON *steps = json_member(entry, "steps");
    const cJSON *similarity = json_member(entry, "similarity");
    if (!fingerprint_value(json_member(entry, "a"), &pair->a) || !fingerprint_value(json_member(entry, "b"), &pair->b) ||
        pair->a > pair->b || !cJSON_IsBool(gated) || !cJSON_IsBool(linked) || !cJSON_IsBool(reversed) ||
        !cJSON_IsArray(steps) || cJSON_GetArraySize(steps) > MOTIVE_LINK_MAX_STEPS ||
        !distance_value(json_member(entry, "distance"), &pair->distance_numerator, &pair->distance_denominator)) {
        return false;
    }
    pair->gated = cJSON_IsTrue(gated);
    pair->linked = cJSON_IsTrue(linked);
    pair->reversed = cJSON_IsTrue(reversed);
    const cJSON *step;
    cJSON_ArrayForEach(step, steps) {
        if (!cJSON_IsString(step) || !motive_relation_step_parse(step->valuestring, &pair->steps[pair->step_count])) {
            return false;
        }
        pair->step_count++;
    }
    for (int d = 0; d < MOTIVE_DIMENSION_COUNT; d++) {
        const cJSON *value = json_member(similarity, dimension_keys[d]);
        if (!cJSON_IsNumber(value)) {
            return false;
        }
        pair->similarity[d] = value->valuedouble;
    }
    return true;
}

static MotiveLinksResult read_cache(const cJSON *root, MotiveLinkCache *cache) {
    const cJSON *schema = json_member(root, "schema");
    const cJSON *version = json_member(root, "schema_version");
    const cJSON *commit = json_member(root, "ursatz_commit");
    const cJSON *analyzer = json_member(root, "analyzer_version");
    const cJSON *options = json_member(root, "options");
    const cJSON *pairs = json_member(root, "pairs");
    const cJSON *contour = json_member(options, "contour");
    const cJSON *nearness = json_member(options, "min_nearness");
    if (!cJSON_IsString(schema) || strcmp(schema->valuestring, CACHE_SCHEMA) != 0 || !cJSON_IsNumber(version) ||
        version->valuedouble != CACHE_SCHEMA_VERSION || !cJSON_IsString(commit) || !cJSON_IsString(analyzer) ||
        !whole_number(json_member(options, "max_depth"), &cache->options.max_depth) ||
        !whole_number(json_member(options, "beam"), &cache->options.beam) || !cJSON_IsNumber(nearness) ||
        !cJSON_IsBool(contour) || !cJSON_IsArray(pairs)) {
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    cache->options.min_nearness = nearness->valuedouble;
    cache->options.contour = cJSON_IsTrue(contour);
    cache->ursatz_commit = copy_text(commit->valuestring);
    cache->analyzer_version = copy_text(analyzer->valuestring);
    if (!cache->ursatz_commit || !cache->analyzer_version) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    cache->keyed = true;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, pairs) {
        MotiveLinkCachePair pair;
        if (!read_pair(entry, &pair)) {
            return MOTIVE_LINKS_ERR_FORMAT;
        }
        if (!motive_link_cache_insert(cache, &pair)) {
            return MOTIVE_LINKS_ERR_ALLOC;
        }
    }
    return MOTIVE_LINKS_OK;
}

MotiveLinksResult motive_link_cache_load(const char *path, MotiveLinkCache **out) {
    if (!path || !out) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    *out = NULL;
    MotiveLinkCache *cache = calloc(1, sizeof *cache);
    if (!cache) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    FILE *probe = fopen(path, "rb");
    if (!probe) {
        if (errno == ENOENT) {
            *out = cache;
            return MOTIVE_LINKS_OK;
        }
        free(cache);
        return MOTIVE_LINKS_ERR_IO;
    }
    fclose(probe);
    char *text = style_read_file(path);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    MotiveLinksResult result = !text ? MOTIVE_LINKS_ERR_IO : (root ? read_cache(root, cache) : MOTIVE_LINKS_ERR_FORMAT);
    free(text);
    cJSON_Delete(root);
    if (result != MOTIVE_LINKS_OK) {
        motive_link_cache_destroy(cache);
        return result;
    }
    *out = cache;
    return MOTIVE_LINKS_OK;
}

static cJSON *pair_json(const MotiveLinkCachePair *pair) {
    cJSON *entry = cJSON_CreateObject();
    cJSON *steps = cJSON_CreateArray();
    cJSON *similarity = cJSON_CreateObject();
    if (!entry || !steps || !similarity) {
        cJSON_Delete(entry);
        cJSON_Delete(steps);
        cJSON_Delete(similarity);
        return NULL;
    }
    char text[128];
    snprintf(text, sizeof text, "%016llx", pair->a);
    bool ok = cJSON_AddStringToObject(entry, "a", text) != NULL;
    snprintf(text, sizeof text, "%016llx", pair->b);
    ok = ok && cJSON_AddStringToObject(entry, "b", text) && cJSON_AddBoolToObject(entry, "gated", pair->gated) &&
         cJSON_AddBoolToObject(entry, "linked", pair->linked) &&
         cJSON_AddBoolToObject(entry, "reversed", pair->reversed);
    for (size_t i = 0; ok && i < pair->step_count; i++) {
        cJSON *step = motive_relation_step_text(&pair->steps[i], text, sizeof text) ? cJSON_CreateString(text) : NULL;
        ok = step != NULL;
        if (ok && !cJSON_AddItemToArray(steps, step)) {
            cJSON_Delete(step);
            ok = false;
        }
    }
    for (int d = 0; ok && d < MOTIVE_DIMENSION_COUNT; d++) {
        ok = cJSON_AddNumberToObject(similarity, dimension_keys[d], pair->similarity[d]) != NULL;
    }
    snprintf(text, sizeof text, "%llu/%llu", pair->distance_numerator,
             pair->distance_denominator ? pair->distance_denominator : 1ULL);
    ok = ok && cJSON_AddItemToObject(entry, "steps", steps);
    if (ok) {
        steps = NULL;
    }
    ok = ok && cJSON_AddStringToObject(entry, "distance", text) && cJSON_AddItemToObject(entry, "similarity", similarity);
    if (ok) {
        similarity = NULL;
    }
    cJSON_Delete(steps);
    cJSON_Delete(similarity);
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

MotiveLinksResult motive_link_cache_save(const MotiveLinkCache *cache, const char *path) {
    if (!cache || !path) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    MotiveLinkOptions defaults;
    motive_link_options_default(&defaults);
    const MotiveLinkOptions *options = cache->keyed ? &cache->options : &defaults;
    cJSON *root = cJSON_CreateObject();
    cJSON *object = cJSON_CreateObject();
    cJSON *pairs = cJSON_CreateArray();
    if (!root || !object || !pairs) {
        cJSON_Delete(root);
        cJSON_Delete(object);
        cJSON_Delete(pairs);
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    bool ok = cJSON_AddStringToObject(root, "schema", CACHE_SCHEMA) &&
              cJSON_AddNumberToObject(root, "schema_version", CACHE_SCHEMA_VERSION) &&
              cJSON_AddStringToObject(root, "ursatz_commit",
                                      cache->keyed ? cache->ursatz_commit : URSATZ_ANALYZER_URSATZ_COMMIT) &&
              cJSON_AddStringToObject(root, "analyzer_version",
                                      cache->keyed ? cache->analyzer_version : URSATZ_ANALYZER_VERSION) &&
              cJSON_AddNumberToObject(object, "max_depth", (double)options->max_depth) &&
              cJSON_AddNumberToObject(object, "beam", (double)options->beam) &&
              cJSON_AddNumberToObject(object, "min_nearness", options->min_nearness) &&
              cJSON_AddBoolToObject(object, "contour", options->contour) && cJSON_AddItemToObject(root, "options", object);
    if (ok) {
        object = NULL;
    }
    cJSON_Delete(object);
    for (size_t i = 0; ok && i < cache->count; i++) {
        cJSON *entry = pair_json(&cache->pairs[i]);
        ok = entry != NULL;
        if (ok && !cJSON_AddItemToArray(pairs, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    ok = ok && cJSON_AddItemToObject(root, "pairs", pairs);
    if (ok) {
        pairs = NULL;
    }
    cJSON_Delete(pairs);
    char *text = ok ? cJSON_Print(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    size_t length = strlen(path);
    char *temporary = malloc(length + 5);
    if (!temporary) {
        cJSON_free(text);
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    sprintf(temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    bool wrote = file && fputs(text, file) >= 0 && fputc('\n', file) != EOF;
    wrote = (file ? fclose(file) == 0 : false) && wrote && rename(temporary, path) == 0;
    if (!wrote) {
        remove(temporary);
    }
    free(temporary);
    cJSON_free(text);
    return wrote ? MOTIVE_LINKS_OK : MOTIVE_LINKS_ERR_IO;
}
