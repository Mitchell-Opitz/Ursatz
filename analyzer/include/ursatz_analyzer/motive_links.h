#ifndef URSATZ_ANALYZER_MOTIVE_LINKS_H
#define URSATZ_ANALYZER_MOTIVE_LINKS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_link.h"
#include "analysis/motive_relation.h"
#include "analysis/motive_variant_recognition.h"

/*
 * Motive links: which motive families of different pieces are variants of one another (R-154). A family's prototype
 * is read from the "features" of its analysis document, as a melodic sketch (no chords) in the key region of its first
 * note, moved to the key of C (motive_link_scope_sketch); two families are linked when the library's variant recognizer finds a chain of at most max_depth relations
 * from one to the other (analysis/motive_link.h). Like the style profile, it reads only analysis documents (R-079).
 *
 * A scope is a set of pieces in the order they were added. For each family the nearest families of other pieces are
 * its candidates: those at nearness at least min_nearness, nearest first (ties by piece order, then family index), at
 * most `candidates`. The union of these as unordered pairs is compared once each, the earlier family in scope order
 * as the source, unless the cache has the pair. Linked families are grouped by union.
 *
 * Incremental: adding one piece to a scope and running again compares only pairs with a family of that piece. A new
 * candidate can only push an old pair out of a family's top list, never into it, so every other pair is in the cache.
 */

typedef struct MotiveLinkScope MotiveLinkScope;
typedef struct MotiveLinkCache MotiveLinkCache;

typedef enum {
    MOTIVE_LINKS_OK = 0,
    MOTIVE_LINKS_ERR_ARGUMENT,    /* a NULL argument, an option out of range, or a piece id added twice */
    MOTIVE_LINKS_ERR_IO,          /* a file could not be read */
    MOTIVE_LINKS_ERR_FORMAT,      /* not an analysis document of schema version 9, a corpus index or a cache file */
    MOTIVE_LINKS_ERR_NO_FEATURES, /* a motive family without features: analyze the piece again */
    MOTIVE_LINKS_ERR_ALLOC
} MotiveLinksResult;

MotiveLinkScope *motive_link_scope_create(void);
void motive_link_scope_destroy(MotiveLinkScope *scope);

/*
 * Adds the piece whose analysis document is json: every motive family's features become a sketch (the same one the
 * Analyzer fingerprints, motive_features_to_sketch). A piece without families is added with none. On any result other
 * than MOTIVE_LINKS_OK the scope is unchanged.
 */
MotiveLinksResult motive_link_scope_add_document(MotiveLinkScope *scope, const char *piece_id, const char *json);

/*
 * Adds every analyzed piece of <output_dir>/corpus-index.json, in index order, with the index path as its piece id.
 * Stops at the first piece that fails and leaves the pieces before it in the scope.
 */
MotiveLinksResult motive_link_scope_add_corpus(MotiveLinkScope *scope, const char *output_dir);

/*
 * The id of the piece the last failed motive_link_scope_add_document (or the piece add_corpus stopped at) was for, or
 * NULL when none failed. Valid until the next failure or the scope is destroyed.
 */
const char *motive_link_scope_failed_piece(const MotiveLinkScope *scope);

/*
 * The sketch a family is linked as: its prototype's features as a melodic sketch (motive_features_to_sketch) moved to the
 * key of C, keeping its mode, by the shortest way that puts its tonic on C. The library's relations keep a sketch's
 * scale, so motives of different keys could never be made equal without this; the chain of a link applies to these
 * sketches (motive_relation_apply). False when the piece or family is not in the scope.
 */
bool motive_link_scope_sketch(const MotiveLinkScope *scope, const char *piece_id, size_t family, MotiveSketch *out);

size_t motive_link_scope_piece_count(const MotiveLinkScope *scope);
size_t motive_link_scope_family_count(const MotiveLinkScope *scope);

typedef struct {
    MotiveLinkOptions link;
    size_t candidates; /* per family, default 4 */
    /* Optional: called with (0, total) when the pairs are known and after each pair, compared or cached. */
    void (*progress)(size_t done, size_t total, void *context);
    void *progress_context;
} MotiveLinksOptions;

/* link as motive_link_options_default, 4 candidates, no progress callback. */
void motive_links_options_default(MotiveLinksOptions *options);

/* One end of a link: a family of a piece. degrees is the prototype's, joined by spaces. */
typedef struct {
    char *piece;
    size_t family;
    int first_measure;
    int last_measure;
    double beat;
    char *degrees;
} MotiveLinkEnd;

#define MOTIVE_LINK_MAX_STEPS MOTIVE_RECOGNIZE_MAX_DEPTH

/*
 * A linked pair. a is the earlier family in scope order. reversed says the chain runs from b to a, else from a to b;
 * steps are in the order they apply. distance is the recognizer's, exact; similarity is indexed by MotiveDimension.
 */
typedef struct {
    MotiveLinkEnd a;
    MotiveLinkEnd b;
    bool reversed;
    MotiveRelationStep steps[MOTIVE_LINK_MAX_STEPS];
    char *step_text[MOTIVE_LINK_MAX_STEPS];
    size_t step_count;
    unsigned long long distance_numerator;
    unsigned long long distance_denominator;
    double similarity[MOTIVE_DIMENSION_COUNT];
} MotiveLinkFound;

typedef struct {
    char *piece;
    size_t family;
} MotiveLinkMember;

/* Families joined by links, at least two, in scope order. */
typedef struct {
    MotiveLinkMember *members;
    size_t member_count;
} MotiveLinkGroup;

typedef struct {
    MotiveLinkFound *links; /* in scope order of a, then of b */
    size_t link_count;
    MotiveLinkGroup *groups; /* largest first, then by first member in scope order */
    size_t group_count;
    size_t pieces;
    size_t pairs;    /* considered */
    size_t compared; /* run through the recognizer now */
    size_t cached;   /* taken from the cache */
    bool cache_reset; /* the cache was for another Ursatz commit, analyzer version or set of link options and was emptied */
} MotiveLinks;

/*
 * The cache of compared pairs, kept by the two sketches' fingerprints so one cache serves every scope (R-156): a pair
 * compared for one scope is not compared again for another. It is valid only for the Ursatz commit, analyzer version and
 * link options it was made with; motive_links_run empties one that differs and says so (MotiveLinks.cache_reset).
 *
 * Load: a file that does not exist is an empty cache. Save: written to <path>.tmp and renamed over path. The file
 * follows schema/motive-link-cache.v1.schema.json.
 */
MotiveLinksResult motive_link_cache_load(const char *path, MotiveLinkCache **out);
MotiveLinksResult motive_link_cache_save(const MotiveLinkCache *cache, const char *path);
void motive_link_cache_destroy(MotiveLinkCache *cache);
size_t motive_link_cache_pair_count(const MotiveLinkCache *cache);

/*
 * Links the scope's families. cache may be NULL (nothing is kept between runs); otherwise it is read and new results are
 * added to it, in memory: the caller saves it. *out is written on MOTIVE_LINKS_OK only.
 */
MotiveLinksResult motive_links_run(const MotiveLinkScope *scope, MotiveLinkCache *cache,
                                   const MotiveLinksOptions *options, MotiveLinks *out);

void motive_links_destroy(MotiveLinks *links);

/*
 * The links as JSON text (schema/motive-links.v1.schema.json), formatted, with a final newline, newly allocated
 * and freed with free(). The same links give the same bytes.
 */
MotiveLinksResult motive_links_render(const MotiveLinks *links, char **out);

#endif
