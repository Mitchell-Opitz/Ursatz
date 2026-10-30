#include <ursatz_analyzer/motive_links.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>

#include "links_internal.h"
#include "pipeline/motive_features.h"
#include "util/json_util.h"
#include "pipeline/corpus_walk.h"
#include "util/misc_util.h"


/* ---- Scope ---- */

/* One motive family of a piece: what linking needs from its document. */
typedef struct {
    size_t piece;  /* index into the scope's pieces */
    size_t family; /* the family's index in its piece */
    int first_measure;
    int last_measure;
    double beat;
    char *degrees; /* the prototype's, joined by spaces */
    MotiveSketch sketch;
    unsigned long long fingerprint;
} Family;

typedef struct {
    char *id;
} Piece;

struct MotiveLinkScope {
    Piece *pieces;
    size_t piece_count;
    size_t piece_capacity;
    Family *families; /* in scope order: piece order, then family index */
    size_t family_count;
    size_t family_capacity;
    char *failed; /* the piece id of the last add that failed */
};

static bool number_value(const cJSON *item, double *out) {
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

static bool int_member(const cJSON *object, const char *name, int *out) {
    double value;
    if (!number_value(json_member(object, name), &value) || value < -2147483647.0 || value > 2147483647.0 ||
        value != (double)(int)value) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* "n/d" with d above zero. */
static bool fraction_value(const cJSON *item, unsigned long long *numerator, unsigned long long *denominator) {
    if (!cJSON_IsString(item)) {
        return false;
    }
    unsigned long long n;
    unsigned long long d;
    char tail;
    if (sscanf(item->valuestring, "%llu/%llu%c", &n, &d, &tail) != 2 || d == 0) {
        return false;
    }
    *numerator = n;
    *denominator = d;
    return true;
}

static bool fraction_member(const cJSON *object, const char *name, unsigned long long *numerator,
                            unsigned long long *denominator) {
    return fraction_value(json_member(object, name), numerator, denominator);
}

/* The prototype's sketch from a family's "features": the document side of motive_features_sketch. */
static MotiveLinksResult features_sketch(const cJSON *features, MotiveSketch *sketch) {
    const cJSON *notes = json_member(features, "notes");
    int count = cJSON_IsArray(notes) ? cJSON_GetArraySize(notes) : 0;
    if (count <= 0 || count > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    AnalysisMotiveFeatures read;
    memset(&read, 0, sizeof read);
    read.notes = calloc((size_t)count, sizeof *read.notes);
    if (!read.notes) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    read.note_count = (size_t)count;
    bool ok = true;
    for (int i = 0; ok && i < count; i++) {
        const cJSON *note = cJSON_GetArrayItem(notes, i);
        AnalysisMotiveNote *out = &read.notes[i];
        ok = int_member(note, "pitch", &out->pitch) &&
             fraction_member(note, "onset", &out->onset_numerator, &out->onset_denominator) &&
             fraction_member(note, "duration", &out->duration_numerator, &out->duration_denominator);
    }
    int tonic;
    ok = ok && int_member(features, "tonic_pc", &tonic) && tonic >= 0 && tonic < 12 &&
         cJSON_IsBool(json_member(features, "minor")) &&
         fraction_member(features, "bar", &read.bar_numerator, &read.bar_denominator) &&
         fraction_member(features, "beat", &read.beat_numerator, &read.beat_denominator) &&
         fraction_member(features, "pickup", &read.pickup_numerator, &read.pickup_denominator);
    if (ok) {
        read.tonic_pc = tonic;
        read.minor = cJSON_IsTrue(json_member(features, "minor"));
    }
    ok = ok && motive_features_sketch(&read, sketch);
    free(read.notes);
    return ok ? MOTIVE_LINKS_OK : MOTIVE_LINKS_ERR_FORMAT;
}

/*
 * Moves a sketch to the key of C, keeping its mode: every pitch by the shortest way that puts the tonic on C. The
 * library's relations keep a sketch's scale, so two motives in different keys could never be made equal; in one key
 * they differ only by the transpositions and the other relations the recognizer knows. False when a pitch would leave
 * the MIDI range.
 */
static bool normalize_key(MotiveSketch *sketch) {
    int shift = sketch->tonic_pc <= 6 ? -sketch->tonic_pc : 12 - sketch->tonic_pc;
    for (size_t i = 0; i < sketch->note_count; i++) {
        int pitch = sketch->notes[i].pitch + shift;
        if (pitch < 0 || pitch > 127) {
            return false;
        }
        sketch->notes[i].pitch = pitch;
    }
    sketch->tonic_pc = 0;
    return motive_sketch_validate(sketch);
}

/* The degrees array joined by spaces, as a new string. */
static char *join_degrees(const cJSON *degrees) {
    size_t length = 1;
    const cJSON *item;
    cJSON_ArrayForEach(item, degrees) {
        if (!cJSON_IsString(item)) {
            return NULL;
        }
        length += strlen(item->valuestring) + 1;
    }
    char *text = malloc(length);
    if (!text) {
        return NULL;
    }
    char *at = text;
    cJSON_ArrayForEach(item, degrees) {
        size_t size = strlen(item->valuestring);
        if (at != text) {
            *at++ = ' ';
        }
        memcpy(at, item->valuestring, size);
        at += size;
    }
    *at = '\0';
    return text;
}

static void family_clear(Family *family) {
    free(family->degrees);
    family->degrees = NULL;
}

/* The family of one motive claim: where its prototype sits and its sketch. */
static MotiveLinksResult read_family(const cJSON *motive, size_t piece, Family *family) {
    memset(family, 0, sizeof *family);
    const cJSON *features = json_member(motive, "features");
    if (!cJSON_IsObject(features)) {
        return MOTIVE_LINKS_ERR_NO_FEATURES;
    }
    family->piece = piece;
    const cJSON *index = json_member(motive, "index");
    const cJSON *occurrences = json_member(motive, "occurrences");
    const cJSON *prototype = cJSON_IsArray(occurrences) ? cJSON_GetArrayItem(occurrences, 0) : NULL;
    const cJSON *measures = json_member(prototype, "measures");
    double beat;
    if (!cJSON_IsNumber(index) || index->valuedouble < 0 || !int_member(measures, "first", &family->first_measure) ||
        !int_member(measures, "last", &family->last_measure) || !number_value(json_member(prototype, "beat"), &beat)) {
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    family->family = (size_t)index->valuedouble;
    family->beat = beat;
    MotiveLinksResult result = features_sketch(features, &family->sketch);
    if (result != MOTIVE_LINKS_OK) {
        return result;
    }
    if (!normalize_key(&family->sketch)) {
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    family->degrees = join_degrees(json_member(features, "degrees"));
    if (!family->degrees) {
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    family->fingerprint = motive_sketch_fingerprint(&family->sketch);
    return MOTIVE_LINKS_OK;
}

MotiveLinkScope *motive_link_scope_create(void) {
    return calloc(1, sizeof(MotiveLinkScope));
}

void motive_link_scope_destroy(MotiveLinkScope *scope) {
    if (!scope) {
        return;
    }
    for (size_t i = 0; i < scope->piece_count; i++) {
        free(scope->pieces[i].id);
    }
    for (size_t i = 0; i < scope->family_count; i++) {
        family_clear(&scope->families[i]);
    }
    free(scope->pieces);
    free(scope->families);
    free(scope->failed);
    free(scope);
}

bool motive_link_scope_sketch(const MotiveLinkScope *scope, const char *piece_id, size_t family, MotiveSketch *out) {
    if (!scope || !piece_id || !out) {
        return false;
    }
    for (size_t i = 0; i < scope->family_count; i++) {
        if (scope->families[i].family == family && strcmp(scope->pieces[scope->families[i].piece].id, piece_id) == 0) {
            *out = scope->families[i].sketch;
            return true;
        }
    }
    return false;
}

size_t motive_link_scope_piece_count(const MotiveLinkScope *scope) {
    return scope ? scope->piece_count : 0;
}

size_t motive_link_scope_family_count(const MotiveLinkScope *scope) {
    return scope ? scope->family_count : 0;
}

static bool grow(void **items, size_t *capacity, size_t needed, size_t size) {
    if (needed <= *capacity) {
        return true;
    }
    size_t grown = *capacity ? *capacity * 2 : 16;
    while (grown < needed) {
        grown *= 2;
    }
    void *next = realloc(*items, grown * size);
    if (!next) {
        return false;
    }
    *items = next;
    *capacity = grown;
    return true;
}

/* Remembers the id of a piece that was not added; a failure to remember it is not worth failing the add for. */
static MotiveLinksResult add_failed(MotiveLinkScope *scope, const char *piece_id, MotiveLinksResult result) {
    char *copy = text_copy(piece_id);
    if (copy) {
        free(scope->failed);
        scope->failed = copy;
    }
    return result;
}

static MotiveLinksResult add_document(MotiveLinkScope *scope, const char *piece_id, const char *json) {
    if (!scope || !piece_id || !piece_id[0] || !json) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < scope->piece_count; i++) {
        if (strcmp(scope->pieces[i].id, piece_id) == 0) {
            return MOTIVE_LINKS_ERR_ARGUMENT;
        }
    }
    cJSON *document = cJSON_Parse(json);
    const cJSON *version = json_member(document, "schema_version");
    const cJSON *motives = json_member(document, "motives");
    if (!cJSON_IsNumber(version) || version->valuedouble != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION || !cJSON_IsArray(motives)) {
        cJSON_Delete(document);
        return MOTIVE_LINKS_ERR_FORMAT;
    }
    size_t count = (size_t)cJSON_GetArraySize(motives);
    Family *read = count ? calloc(count, sizeof *read) : NULL;
    MotiveLinksResult result = count && !read ? MOTIVE_LINKS_ERR_ALLOC : MOTIVE_LINKS_OK;
    size_t made = 0;
    for (; result == MOTIVE_LINKS_OK && made < count; made++) {
        result = read_family(cJSON_GetArrayItem(motives, (int)made), scope->piece_count, &read[made]);
        if (result != MOTIVE_LINKS_OK) {
            family_clear(&read[made]);
            break;
        }
    }
    cJSON_Delete(document);
    char *id = NULL;
    if (result == MOTIVE_LINKS_OK) {
        id = text_copy(piece_id);
        if (!id || !grow((void **)&scope->pieces, &scope->piece_capacity, scope->piece_count + 1, sizeof(Piece)) ||
            !grow((void **)&scope->families, &scope->family_capacity, scope->family_count + count, sizeof(Family))) {
            result = MOTIVE_LINKS_ERR_ALLOC;
        }
    }
    if (result != MOTIVE_LINKS_OK) {
        for (size_t i = 0; i < made; i++) {
            family_clear(&read[i]);
        }
        free(read);
        free(id);
        return result;
    }
    scope->pieces[scope->piece_count++].id = id;
    if (count) {
        memcpy(&scope->families[scope->family_count], read, count * sizeof *read);
        scope->family_count += count;
    }
    free(read);
    return MOTIVE_LINKS_OK;
}

MotiveLinksResult motive_link_scope_add_document(MotiveLinkScope *scope, const char *piece_id, const char *json) {
    MotiveLinksResult result = add_document(scope, piece_id, json);
    if (scope && piece_id && result != MOTIVE_LINKS_OK && result != MOTIVE_LINKS_ERR_ARGUMENT) {
        return add_failed(scope, piece_id, result);
    }
    return result;
}

const char *motive_link_scope_failed_piece(const MotiveLinkScope *scope) {
    return scope ? scope->failed : NULL;
}

typedef struct {
    MotiveLinkScope *scope;
    MotiveLinksResult result;
} ScopeAdd;

static bool add_scope_document(const char *piece_path, const char *document, const char *json, void *context) {
    (void)document;
    ScopeAdd *add = context;
    add->result = motive_link_scope_add_document(add->scope, piece_path, json);
    return add->result == MOTIVE_LINKS_OK;
}

MotiveLinksResult motive_link_scope_add_corpus(MotiveLinkScope *scope, const char *output_dir) {
    if (!scope || !output_dir) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    size_t directory = strlen(output_dir);
    char *index_path = malloc(directory + 24);
    if (!index_path) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    sprintf(index_path, "%s/corpus-index.json", output_dir);
    ScopeAdd add = {scope, MOTIVE_LINKS_OK};
    CorpusWalkResult walked = corpus_walk_documents(index_path, add_scope_document, &add, NULL);
    free(index_path);
    switch (walked) {
    case CORPUS_WALK_OK:
        return MOTIVE_LINKS_OK;
    case CORPUS_WALK_ERR_ALLOC:
        return MOTIVE_LINKS_ERR_ALLOC;
    case CORPUS_WALK_ERR_IO:
        return MOTIVE_LINKS_ERR_IO;
    case CORPUS_WALK_ERR_FORMAT:
        return MOTIVE_LINKS_ERR_FORMAT;
    case CORPUS_WALK_ERR_VISIT:
        break;
    }
    return add.result;
}

/* ---- Run ---- */

void motive_links_options_default(MotiveLinksOptions *options) {
    memset(options, 0, sizeof *options);
    motive_link_options_default(&options->link);
    options->candidates = 4;
}

/* An unordered pair of families by scope index, low first. */
typedef struct {
    size_t low;
    size_t high;
} FamilyPair;

typedef struct {
    size_t family;
    double nearness;
} Candidate;

/* Nearest first; ties by scope order, which is piece order and then family index. */
static int compare_candidates(const void *left, const void *right) {
    const Candidate *a = left;
    const Candidate *b = right;
    if (a->nearness != b->nearness) {
        return a->nearness > b->nearness ? -1 : 1;
    }
    return a->family < b->family ? -1 : (a->family > b->family ? 1 : 0);
}

static int compare_pairs(const void *left, const void *right) {
    const FamilyPair *a = left;
    const FamilyPair *b = right;
    if (a->low != b->low) {
        return a->low < b->low ? -1 : 1;
    }
    return a->high < b->high ? -1 : (a->high > b->high ? 1 : 0);
}

/* The pairs to compare: for each family its nearest candidates in other pieces, as unordered pairs, sorted and unique. */
static MotiveLinksResult collect_pairs(const MotiveLinkScope *scope, const MotiveLinksOptions *options,
                                       FamilyPair **out, size_t *count) {
    *out = NULL;
    *count = 0;
    size_t families = scope->family_count;
    if (families == 0) {
        return MOTIVE_LINKS_OK;
    }
    size_t per_family = options->candidates < families ? options->candidates : families;
    Candidate *candidates = malloc(families * sizeof *candidates);
    FamilyPair *pairs = malloc(families * per_family * sizeof *pairs);
    if (!candidates || !pairs) {
        free(candidates);
        free(pairs);
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    size_t total = 0;
    for (size_t f = 0; f < families; f++) {
        size_t found = 0;
        for (size_t g = 0; g < families; g++) {
            if (scope->families[g].piece == scope->families[f].piece) {
                continue;
            }
            double nearness = motive_link_nearness(&scope->families[f].sketch, &scope->families[g].sketch);
            if (nearness >= options->link.min_nearness) {
                candidates[found++] = (Candidate){g, nearness};
            }
        }
        qsort(candidates, found, sizeof *candidates, compare_candidates);
        if (found > per_family) {
            found = per_family;
        }
        for (size_t c = 0; c < found; c++) {
            size_t g = candidates[c].family;
            pairs[total++] = (FamilyPair){f < g ? f : g, f < g ? g : f};
        }
    }
    free(candidates);
    qsort(pairs, total, sizeof *pairs, compare_pairs);
    size_t unique = 0;
    for (size_t i = 0; i < total; i++) {
        if (unique == 0 || compare_pairs(&pairs[unique - 1], &pairs[i]) != 0) {
            pairs[unique++] = pairs[i];
        }
    }
    *out = pairs;
    *count = unique;
    return MOTIVE_LINKS_OK;
}

static bool options_valid(const MotiveLinksOptions *options) {
    const MotiveLinkOptions *link = &options->link;
    return link->max_depth >= 1 && link->max_depth <= MOTIVE_RECOGNIZE_MAX_DEPTH && link->beam >= 1 &&
           link->beam <= MOTIVE_RECOGNIZE_MAX_BEAM && link->min_nearness >= 0.0 && link->min_nearness <= 1.0 &&
           options->candidates >= 1;
}

/* The cache entry of low and high, whose fingerprints are a and b in scope order, from a fresh comparison. The cache
   keeps a pair by the order of its fingerprints, so reversed is flipped when the earlier family has the larger one. */
static MotiveLinkCachePair cache_pair_of(const Family *earlier, const Family *later, const MotiveLink *link) {
    MotiveLinkCachePair pair;
    memset(&pair, 0, sizeof pair);
    bool swap = earlier->fingerprint > later->fingerprint;
    pair.a = swap ? later->fingerprint : earlier->fingerprint;
    pair.b = swap ? earlier->fingerprint : later->fingerprint;
    pair.gated = link->gated;
    pair.linked = link->linked;
    pair.reversed = link->linked && (link->reversed != swap);
    pair.distance_numerator = 1;
    pair.distance_denominator = 1;
    if (link->linked) {
        pair.step_count = link->recognition.step_count;
        memcpy(pair.steps, link->recognition.steps, pair.step_count * sizeof pair.steps[0]);
        pair.distance_numerator = link->recognition.distance.n;
        pair.distance_denominator = link->recognition.distance.d;
        memcpy(pair.similarity, link->recognition.similarity, sizeof pair.similarity);
    }
    return pair;
}

static bool end_fill(MotiveLinkEnd *end, const MotiveLinkScope *scope, const Family *family) {
    end->piece = text_copy(scope->pieces[family->piece].id);
    end->degrees = text_copy(family->degrees);
    end->family = family->family;
    end->first_measure = family->first_measure;
    end->last_measure = family->last_measure;
    end->beat = family->beat;
    return end->piece && end->degrees;
}

static void end_clear(MotiveLinkEnd *end) {
    free(end->piece);
    free(end->degrees);
    end->piece = NULL;
    end->degrees = NULL;
}

static void found_clear(MotiveLinkFound *link) {
    end_clear(&link->a);
    end_clear(&link->b);
    for (size_t i = 0; i < MOTIVE_LINK_MAX_STEPS; i++) {
        free(link->step_text[i]);
        link->step_text[i] = NULL;
    }
}

/* The link of the earlier and later family from a cached pair; reversed is relative to the scope order. */
static bool found_fill(MotiveLinkFound *link, const MotiveLinkScope *scope, const Family *earlier, const Family *later,
                       const MotiveLinkCachePair *pair) {
    memset(link, 0, sizeof *link);
    if (!end_fill(&link->a, scope, earlier) || !end_fill(&link->b, scope, later)) {
        found_clear(link);
        return false;
    }
    link->reversed = pair->reversed != (earlier->fingerprint > later->fingerprint);
    link->step_count = pair->step_count;
    memcpy(link->steps, pair->steps, pair->step_count * sizeof pair->steps[0]);
    for (size_t i = 0; i < pair->step_count; i++) {
        char text[128];
        if (!motive_relation_step_text(&pair->steps[i], text, sizeof text) || !(link->step_text[i] = text_copy(text))) {
            found_clear(link);
            return false;
        }
    }
    link->distance_numerator = pair->distance_numerator;
    link->distance_denominator = pair->distance_denominator;
    memcpy(link->similarity, pair->similarity, sizeof link->similarity);
    return true;
}

static size_t find_root(size_t *parent, size_t node) {
    while (parent[node] != node) {
        parent[node] = parent[parent[node]];
        node = parent[node];
    }
    return node;
}

typedef struct {
    size_t first; /* the group's first member, scope index */
    size_t size;
    size_t root;
} GroupKey;

/* Largest first, then by first member in scope order. */
static int compare_groups(const void *left, const void *right) {
    const GroupKey *a = left;
    const GroupKey *b = right;
    if (a->size != b->size) {
        return a->size > b->size ? -1 : 1;
    }
    return a->first < b->first ? -1 : (a->first > b->first ? 1 : 0);
}

/* The groups of the linked families: families joined by links, found by union. */
static bool build_groups(const MotiveLinkScope *scope, const FamilyPair *pairs, const bool *linked, size_t pair_count,
                         MotiveLinks *out) {
    size_t families = scope->family_count;
    size_t *parent = malloc((families ? families : 1) * sizeof *parent);
    size_t *size = calloc(families ? families : 1, sizeof *size);
    size_t *first = malloc((families ? families : 1) * sizeof *first);
    if (!parent || !size || !first) {
        free(parent);
        free(size);
        free(first);
        return false;
    }
    for (size_t i = 0; i < families; i++) {
        parent[i] = i;
        first[i] = families;
    }
    for (size_t p = 0; p < pair_count; p++) {
        if (linked[p]) {
            size_t low = find_root(parent, pairs[p].low);
            size_t high = find_root(parent, pairs[p].high);
            if (low != high) {
                parent[high] = low;
            }
        }
    }
    /* A family is in a group when a link touches it. */
    bool *touched = calloc(families ? families : 1, sizeof *touched);
    if (!touched) {
        free(parent);
        free(size);
        free(first);
        return false;
    }
    for (size_t p = 0; p < pair_count; p++) {
        if (linked[p]) {
            touched[pairs[p].low] = true;
            touched[pairs[p].high] = true;
        }
    }
    size_t group_count = 0;
    for (size_t f = 0; f < families; f++) {
        if (!touched[f]) {
            continue;
        }
        size_t root = find_root(parent, f);
        if (size[root] == 0) {
            first[root] = f;
            group_count++;
        }
        size[root]++;
    }
    GroupKey *keys = group_count ? malloc(group_count * sizeof *keys) : NULL;
    MotiveLinkGroup *groups = group_count ? calloc(group_count, sizeof *groups) : NULL;
    bool ok = (!group_count || (keys && groups));
    size_t made = 0;
    for (size_t f = 0; ok && f < families; f++) {
        if (touched[f] && find_root(parent, f) == f) {
            keys[made++] = (GroupKey){first[f], size[f], f};
        }
    }
    if (ok && made > 1) {
        qsort(keys, made, sizeof *keys, compare_groups);
    }
    for (size_t g = 0; ok && g < made; g++) {
        groups[g].members = malloc(keys[g].size * sizeof *groups[g].members);
        ok = groups[g].members != NULL;
        for (size_t f = 0; ok && f < families; f++) {
            if (touched[f] && find_root(parent, f) == keys[g].root) {
                MotiveLinkMember *member = &groups[g].members[groups[g].member_count];
                member->piece = text_copy(scope->pieces[scope->families[f].piece].id);
                member->family = scope->families[f].family;
                groups[g].member_count++;
                ok = member->piece != NULL;
            }
        }
        if (!ok) {
            made = g + 1;
        }
    }
    free(parent);
    free(size);
    free(first);
    free(touched);
    free(keys);
    if (!ok) {
        for (size_t g = 0; groups && g < made; g++) {
            for (size_t m = 0; groups[g].members && m < groups[g].member_count; m++) {
                free(groups[g].members[m].piece);
            }
            free(groups[g].members);
        }
        free(groups);
        return false;
    }
    out->groups = groups;
    out->group_count = group_count;
    return true;
}

void motive_links_destroy(MotiveLinks *links) {
    if (!links) {
        return;
    }
    for (size_t i = 0; i < links->link_count; i++) {
        found_clear(&links->links[i]);
    }
    for (size_t g = 0; g < links->group_count; g++) {
        for (size_t m = 0; m < links->groups[g].member_count; m++) {
            free(links->groups[g].members[m].piece);
        }
        free(links->groups[g].members);
    }
    free(links->links);
    free(links->groups);
    memset(links, 0, sizeof *links);
}

MotiveLinksResult motive_links_run(const MotiveLinkScope *scope, MotiveLinkCache *cache,
                                   const MotiveLinksOptions *options, MotiveLinks *out) {
    if (!scope || !options || !out || !options_valid(options)) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    out->pieces = scope->piece_count;
    MotiveLinkCache local;
    memset(&local, 0, sizeof local);
    MotiveLinkCache *use = cache ? cache : &local;
    bool reset = false;
    if (!motive_link_cache_prepare(use, &options->link, &reset)) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    FamilyPair *pairs;
    size_t pair_count;
    MotiveLinksResult result = collect_pairs(scope, options, &pairs, &pair_count);
    if (result != MOTIVE_LINKS_OK) {
        return result;
    }
    bool *linked = calloc(pair_count ? pair_count : 1, sizeof *linked);
    MotiveLinkFound *links = pair_count ? calloc(pair_count, sizeof *links) : NULL;
    if (!linked || (pair_count && !links)) {
        free(pairs);
        free(linked);
        free(links);
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    out->pairs = pair_count;
    out->cache_reset = reset;
    if (options->progress) {
        options->progress(0, pair_count, options->progress_context);
    }
    size_t link_count = 0;
    for (size_t p = 0; result == MOTIVE_LINKS_OK && p < pair_count; p++) {
        const Family *earlier = &scope->families[pairs[p].low];
        const Family *later = &scope->families[pairs[p].high];
        const MotiveLinkCachePair *pair = motive_link_cache_find(use, earlier->fingerprint, later->fingerprint);
        MotiveLinkCachePair fresh;
        if (pair) {
            out->cached++;
        } else {
            MotiveLink link;
            MotiveLinkResult compared = motive_link_pair(&earlier->sketch, &later->sketch, &options->link, &link);
            if (compared != MOTIVE_LINK_OK) {
                result = compared == MOTIVE_LINK_NO_MEMORY ? MOTIVE_LINKS_ERR_ALLOC : MOTIVE_LINKS_ERR_FORMAT;
                break;
            }
            fresh = cache_pair_of(earlier, later, &link);
            if (!motive_link_cache_insert(use, &fresh)) {
                result = MOTIVE_LINKS_ERR_ALLOC;
                break;
            }
            pair = &fresh;
            out->compared++;
        }
        if (pair->linked) {
            if (!found_fill(&links[link_count], scope, earlier, later, pair)) {
                result = MOTIVE_LINKS_ERR_ALLOC;
                break;
            }
            linked[p] = true;
            link_count++;
        }
        if (options->progress) {
            options->progress(p + 1, pair_count, options->progress_context);
        }
    }
    if (result == MOTIVE_LINKS_OK) {
        out->links = links;
        out->link_count = link_count;
        if (!build_groups(scope, pairs, linked, pair_count, out)) {
            result = MOTIVE_LINKS_ERR_ALLOC;
        }
    }
    if (result != MOTIVE_LINKS_OK) {
        out->links = links;
        out->link_count = link_count;
        motive_links_destroy(out);
        links = NULL;
    }
    free(pairs);
    free(linked);
    if (!cache) {
        free(local.pairs);
        free(local.ursatz_commit);
        free(local.analyzer_version);
    }
    return result;
}
