#ifndef URSATZ_ANALYZER_LINKS_INTERNAL_H
#define URSATZ_ANALYZER_LINKS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/motive_links.h>

/* One motive family of a piece: what linking needs from its document. */
typedef struct {
    size_t piece;  /* index into the scope's pieces */
    size_t family; /* the family's index in its piece */
    int first_measure;
    int last_measure;
    double beat;
    char *degrees; /* the prototype's, joined by spaces */
    /* The sketch the family is linked as: its prototype's features as a melodic sketch (motive_features_to_sketch)
       moved to the key of C, keeping its mode, by the shortest way that puts its tonic on C. The library's relations
       keep a sketch's scale, so motives of different keys could never be made equal without this; the chain of a link
       applies to these sketches (motive_relation_apply). */
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

/* One compared pair in the cache. a and b are sketch fingerprints with a <= b; reversed and steps are relative to
   that order: a chain that runs from b to a is reversed. */
typedef struct {
    unsigned long long a;
    unsigned long long b;
    bool gated;
    bool linked;
    bool reversed;
    size_t step_count;
    MotiveRelationStep steps[MOTIVE_LINK_MAX_STEPS];
    unsigned long long distance_numerator;
    unsigned long long distance_denominator;
    double similarity[MOTIVE_DIMENSION_COUNT];
} MotiveLinkCachePair;

struct MotiveLinkCache {
    bool keyed; /* the three fields below were read from a file or set by a run */
    char *producer_key;
    char *analyzer_version;
    MotiveLinkOptions options;
    MotiveLinkCachePair *pairs; /* sorted by (a, b) */
    size_t count;
    size_t capacity;
};

/* The cached pair of two fingerprints (in either order), or NULL. */
const MotiveLinkCachePair *motive_link_cache_find(const MotiveLinkCache *cache, unsigned long long a,
                                                  unsigned long long b);

/* Adds pair (a <= b) in order; replaces one with the same fingerprints. False when out of memory. */
bool motive_link_cache_insert(MotiveLinkCache *cache, const MotiveLinkCachePair *pair);

/*
 * Makes the cache usable for a run with these options: when it was keyed for another producer key, analyzer version
 * or set of link options it is emptied and *reset is true. Either way it is keyed for this run afterwards. False when
 * out of memory.
 */
bool motive_link_cache_prepare(MotiveLinkCache *cache, const MotiveLinkOptions *options, bool *reset);

#endif
