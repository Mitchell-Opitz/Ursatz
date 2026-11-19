#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/motive_links.h>
#include <ursatz_analyzer/version.h>

#include "links_internal.h"
#include <common/json.h>
#include <common/files.h>
#include "util/misc_util.h"

#define CACHE_SCHEMA "ursatz-motive-link-cache"
#define CACHE_SCHEMA_VERSION 2

static const char *const dimension_keys[MOTIVE_DIMENSION_COUNT] = {"rhythm", "intervals", "contour", "harmony"};

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
    if (cache->keyed && cache->producer_key && cache->analyzer_version &&
        strcmp(cache->producer_key, ursatz_analyzer_producer_key()) == 0 &&
        strcmp(cache->analyzer_version, URSATZ_ANALYZER_VERSION) == 0 && options_equal(&cache->options, options)) {
        return true;
    }
    char *commit = text_copy(ursatz_analyzer_producer_key());
    char *version = text_copy(URSATZ_ANALYZER_VERSION);
    if (!commit || !version) {
        free(commit);
        free(version);
        return false;
    }
    if (cache->keyed) {
        cache->count = 0;
        *reset = true;
    }
    free(cache->producer_key);
    free(cache->analyzer_version);
    cache->producer_key = commit;
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
    free(cache->producer_key);
    free(cache->analyzer_version);
    free(cache);
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
    long long n, d;
    if (!common_json_rational_read(item, COMMON_RATIONAL_TEXT, &n, &d) || n < 0) {
        return false;
    }
    *numerator = (unsigned long long)n;
    *denominator = (unsigned long long)d;
    return true;
}

static bool read_pair(const cJSON *entry, MotiveLinkCachePair *pair) {
    memset(pair, 0, sizeof *pair);
    const cJSON *gated = common_json_member(entry, "gated");
    const cJSON *linked = common_json_member(entry, "linked");
    const cJSON *reversed = common_json_member(entry, "reversed");
    const cJSON *steps = common_json_member(entry, "steps");
    const cJSON *similarity = common_json_member(entry, "similarity");
    if (!fingerprint_value(common_json_member(entry, "a"), &pair->a) || !fingerprint_value(common_json_member(entry, "b"), &pair->b) ||
        pair->a > pair->b || !cJSON_IsBool(gated) || !cJSON_IsBool(linked) || !cJSON_IsBool(reversed) ||
        !cJSON_IsArray(steps) || cJSON_GetArraySize(steps) > MOTIVE_LINK_MAX_STEPS ||
        !distance_value(common_json_member(entry, "distance"), &pair->distance_numerator, &pair->distance_denominator)) {
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
        const cJSON *value = common_json_member(similarity, dimension_keys[d]);
        if (!cJSON_IsNumber(value)) {
            return false;
        }
        pair->similarity[d] = value->valuedouble;
    }
    return true;
}

static UrsatzStatus read_cache(const cJSON *root, MotiveLinkCache *cache) {
    const cJSON *schema = common_json_member(root, "schema");
    const cJSON *version = common_json_member(root, "schema_version");
    const cJSON *commit = common_json_member(root, "producer_key");
    const cJSON *analyzer = common_json_member(root, "analyzer_version");
    const cJSON *options = common_json_member(root, "options");
    const cJSON *pairs = common_json_member(root, "pairs");
    const cJSON *contour = common_json_member(options, "contour");
    const cJSON *nearness = common_json_member(options, "min_nearness");
    if (cJSON_IsString(schema) && strcmp(schema->valuestring, CACHE_SCHEMA) == 0 && cJSON_IsNumber(version) &&
        version->valuedouble < CACHE_SCHEMA_VERSION) {
        /* A cache of an older schema version (it keys itself by ursatz_commit) reads as an empty cache keyed with
           nothing, so the next run starts it again and reports cache_reset. */
        cache->producer_key = text_copy("");
        cache->analyzer_version = text_copy("");
        motive_link_options_default(&cache->options);
        if (!cache->producer_key || !cache->analyzer_version) {
            return URSATZ_ERR_ALLOC;
        }
        cache->keyed = true;
        return URSATZ_OK;
    }
    if (!cJSON_IsString(schema) || strcmp(schema->valuestring, CACHE_SCHEMA) != 0 || !cJSON_IsNumber(version) ||
        version->valuedouble != CACHE_SCHEMA_VERSION || !cJSON_IsString(commit) || !cJSON_IsString(analyzer) ||
        !whole_number(common_json_member(options, "max_depth"), &cache->options.max_depth) ||
        !whole_number(common_json_member(options, "beam"), &cache->options.beam) || !cJSON_IsNumber(nearness) ||
        !cJSON_IsBool(contour) || !cJSON_IsArray(pairs)) {
        return URSATZ_ERR_FORMAT;
    }
    cache->options.min_nearness = nearness->valuedouble;
    cache->options.contour = cJSON_IsTrue(contour);
    cache->producer_key = text_copy(commit->valuestring);
    cache->analyzer_version = text_copy(analyzer->valuestring);
    if (!cache->producer_key || !cache->analyzer_version) {
        return URSATZ_ERR_ALLOC;
    }
    cache->keyed = true;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, pairs) {
        MotiveLinkCachePair pair;
        if (!read_pair(entry, &pair)) {
            return URSATZ_ERR_FORMAT;
        }
        if (!motive_link_cache_insert(cache, &pair)) {
            return URSATZ_ERR_ALLOC;
        }
    }
    return URSATZ_OK;
}

UrsatzStatus motive_link_cache_load(const char *path, MotiveLinkCache **out) {
    if (!path || !out) {
        return URSATZ_ERR_REQUEST;
    }
    *out = NULL;
    MotiveLinkCache *cache = calloc(1, sizeof *cache);
    if (!cache) {
        return URSATZ_ERR_ALLOC;
    }
    FILE *probe = fopen(path, "rb");
    if (!probe) {
        if (errno == ENOENT) {
            *out = cache;
            return URSATZ_OK;
        }
        free(cache);
        return URSATZ_ERR_IO;
    }
    fclose(probe);
    char *text = NULL;
    common_file_read(path, &text, NULL, NULL);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    UrsatzStatus result = !text ? URSATZ_ERR_IO : (root ? read_cache(root, cache) : URSATZ_ERR_FORMAT);
    free(text);
    cJSON_Delete(root);
    if (result != URSATZ_OK) {
        motive_link_cache_destroy(cache);
        return result;
    }
    *out = cache;
    return URSATZ_OK;
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

UrsatzStatus motive_link_cache_save(const MotiveLinkCache *cache, const char *path) {
    if (!cache || !path) {
        return URSATZ_ERR_REQUEST;
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
        return URSATZ_ERR_ALLOC;
    }
    bool ok = cJSON_AddStringToObject(root, "schema", CACHE_SCHEMA) &&
              cJSON_AddNumberToObject(root, "schema_version", CACHE_SCHEMA_VERSION) &&
              cJSON_AddStringToObject(root, "producer_key",
                                      cache->keyed ? cache->producer_key : ursatz_analyzer_producer_key()) &&
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
        return URSATZ_ERR_ALLOC;
    }
    size_t text_length = strlen(text);
    char *terminated = malloc(text_length + 2);
    if (!terminated) {
        cJSON_free(text);
        return URSATZ_ERR_ALLOC;
    }
    memcpy(terminated, text, text_length);
    memcpy(terminated + text_length, "\n", 2);
    bool wrote = (common_file_write_atomic(path, terminated, strlen(terminated), NULL) == URSATZ_OK);
    free(terminated);
    cJSON_free(text);
    return wrote ? URSATZ_OK : URSATZ_ERR_IO;
}
