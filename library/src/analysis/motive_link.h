#ifndef MUSIC_ANALYSIS_MOTIVE_LINK_H
#define MUSIC_ANALYSIS_MOTIVE_LINK_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_sketch.h"
#include "analysis/motive_variant_recognition.h"

/*
 * MotiveLink (L8 analysis): whether two motives, usually from different pieces, are variants of one another.
 * A link claims that one motive is a chain of motive relations (analysis/motive_relation.h) from the other,
 * at most max_depth steps long, found by the variant recognizer (analysis/motive_variant_recognition.h) with
 * every relation allowed.
 *
 * Recognizing costs: a depth-2 miss takes about 770 ms in the default build (docs/modules/motive-link.md), so
 * a pair goes through a gate first. The gate is the nearness of the two sketches (motive_link_nearness, the
 * mean of the four dimension similarities); a pair below min_nearness is not searched. A caller that has many
 * motives to compare ranks the candidates of each by motive_link_nearness, takes the nearest few, and calls
 * motive_link_pair on those.
 *
 * A chain is directional: a fragment of a motive is reached from the whole by a fragment step, but the whole
 * is not reached from the fragment at depth 2. So a pair is recognized from a to b, and when that finds
 * nothing, from b to a. reversed says the chain found runs from b to a.
 *
 * The sketches are compared as given (melodic sketches, no chords, in the key region of the motive's first
 * note). Nothing is allocated beyond the recognizer's beam, which lives for the length of one call.
 */

typedef struct {
    size_t max_depth;    /* 1 to MOTIVE_RECOGNIZE_MAX_DEPTH, default 2 */
    size_t beam;         /* 1 to MOTIVE_RECOGNIZE_MAX_BEAM, default 64 */
    double min_nearness; /* 0 to 1, default 0.5: the gate */
    bool contour;        /* recognize contour forms, default false */
} MotiveLinkOptions;

void motive_link_options_default(MotiveLinkOptions *options);

/* motive_variant_nearness of motive_variant_compare(a, b): 0 to 1, symmetric. */
double motive_link_nearness(const MotiveSketch *a, const MotiveSketch *b);

typedef struct {
    bool gated;    /* nearness below min_nearness: nothing was searched, linked is false */
    bool linked;   /* a chain of at most max_depth steps was found in either direction */
    bool reversed; /* the chain runs from b to a (a to b found nothing) */
    double nearness;
    MotiveRecognition recognition; /* the chain found, or the a to b miss; zeroed when gated */
} MotiveLink;

typedef enum {
    MOTIVE_LINK_OK = 0,
    MOTIVE_LINK_INVALID,  /* a sketch fails motive_sketch_validate, an option is out of range, or out is NULL */
    MOTIVE_LINK_NO_MEMORY /* the recognizer's beam could not be allocated */
} MotiveLinkResult;

/*
 * Compares a and b. A NULL options takes the defaults. *out is written on MOTIVE_LINK_OK only.
 */
MotiveLinkResult motive_link_pair(const MotiveSketch *a, const MotiveSketch *b, const MotiveLinkOptions *options,
                                  MotiveLink *out);

/*
 * Choosing and grouping. A caller with many motives picks the pairs worth comparing, runs motive_link_pair on
 * them, and groups the motives the links join.
 */

/* An unordered pair of sketches by index, low first (low < high). */
typedef struct {
    size_t low;
    size_t high;
} MotiveLinkPair;

/*
 * For each sketch, its per_sketch nearest other sketches in a different group (groups[i], the piece the sketch
 * belongs to) whose motive_link_nearness is at least min_nearness, nearest first, equal nearness by index. The
 * pairs are unordered, sorted by low then high, and unique. per_sketch is 1 or more and min_nearness 0 to 1,
 * else MOTIVE_LINK_INVALID (as for a NULL argument with a count). On OK, *out is owned by the caller (free) and
 * NULL with *out_count 0 when there is no pair; otherwise *out and *out_count are untouched.
 */
MotiveLinkResult motive_link_candidates(const MotiveSketch *sketches, const size_t *groups, size_t count,
                                        size_t per_sketch, double min_nearness, MotiveLinkPair **out,
                                        size_t *out_count);

/*
 * The connected components of the linked pairs: pairs[p] joins its two sketches when linked[p]. Groups are
 * numbered from 0 in the order of their lowest member. (*group_of)[i] is the number of the group of sketch i,
 * or (size_t)-1 when no linked pair touches it (a singleton, left out). *group_of is owned by the caller (free;
 * NULL when count is 0). A pair with low >= high or high >= count is MOTIVE_LINK_INVALID. *group_of and
 * *group_count are written on OK only.
 */
MotiveLinkResult motive_link_groups(size_t count, const MotiveLinkPair *pairs, const bool *linked, size_t pair_count,
                                    size_t **group_of, size_t *group_count);

#endif
