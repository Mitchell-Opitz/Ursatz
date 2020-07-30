#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_REPEAT_NAVIGATION_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_REPEAT_NAVIGATION_H

#include <stdbool.h>
#include <stddef.h>

#include "structural_rules.h"

/*
 * Repeat navigation for standard-markings-v1: given the structural
 * markers of a work, the order in which its measures are played.
 *
 * Works on plain measure indices (0 .. measure_count - 1), not on
 * Measure/TemporalAnchor objects. Mapping an anchor to a measure belongs
 * to Measure/MeterMap territory (containers/measure.h notes MeterMap does
 * not exist yet), so the caller supplies indices. Markers may be given in
 * any order.
 *
 * Marker positions:
 *   repeat-start   at the START of its measure
 *   repeat-end     at the END of its measure
 *   ending         at the start of its measure; parameter = its number
 *   segno, coda    at the start of their measure
 *   to-coda, fine  at the end of their measure
 *   dc-/ds-al-*    at the end of their measure
 *
 * Rules implemented:
 *  - A repeat-end sends play back to the nearest earlier repeat-start (or
 *    the beginning of the work if there is none), once.
 *  - The nth ending is played the nth time through; the others are
 *    skipped. An ending runs up to the next ending (number + 1); the last
 *    ending runs to the end of the work, or to the next repeat-start or
 *    ending numbered 1 if a new repeated section follows.
 *  - D.C. goes back to the beginning, D.S. to the segno. "al fine" plays
 *    until the fine; "al coda" plays until the to-coda sign and then
 *    continues at the coda. Fine and to-coda are ignored on the first
 *    time through.
 *  - Convention on the jump pass: repeat signs are NOT taken again, and
 *    each ending group plays its last ending. This is the standard
 *    reading; options.take_repeats_after_jump = true replays repeats and
 *    endings by the ordinary rules instead.
 *
 * One- and two-measure repeat signs are not playback jumps: they say
 * that a measure repeats the content of an earlier one. Resolving that
 * is standard_markings_measure_content_source; the playback order names
 * measures as written, and the caller maps each through it.
 */

typedef struct {
    size_t measure;
    StructuralKind kind;
    unsigned int parameter; /* ending number; ignored by other kinds */
} NavigationMarker;

typedef struct {
    bool take_repeats_after_jump;
} NavigationOptions;

typedef enum {
    NAVIGATION_OK = 0,
    NAVIGATION_ERR_NULL_ARGUMENT,
    NAVIGATION_ERR_MARKER_OUT_OF_RANGE,
    NAVIGATION_ERR_MALFORMED_MARKER,
    NAVIGATION_ERR_MISSING_SEGNO,
    NAVIGATION_ERR_MISSING_CODA,
    NAVIGATION_ERR_BUFFER_TOO_SMALL,
    NAVIGATION_ERR_RUNAWAY,
    NAVIGATION_ERR_ALLOC
} NavigationResult;

/* The standard reading: repeats are not retaken after a D.C./D.S. jump. */
NavigationOptions standard_markings_default_navigation_options(void);

/*
 * Writes the sequence of measure indices in performance order to
 * out_order (capacity out_capacity) and the count to *out_length. options
 * may be NULL for the defaults. measure_count of 0 yields an empty order.
 * markers may be NULL only when marker_count is 0.
 */
NavigationResult standard_markings_playback_order(const NavigationMarker *markers, size_t marker_count,
                                                  size_t measure_count, const NavigationOptions *options,
                                                  size_t *out_order, size_t out_capacity, size_t *out_length);

/*
 * The measure whose content `measure` plays. A measure carrying a
 * one-measure-repeat marker plays the content of the measure before it;
 * under a two-measure-repeat marker at measure m, m plays m - 2 and m + 1
 * plays m - 1. Chains resolve through to written content. A measure with
 * no repeat sign is its own source. Returns false when the sign points
 * before the first measure.
 */
bool standard_markings_measure_content_source(const NavigationMarker *markers, size_t marker_count,
                                              size_t measure, size_t *out_source);

#endif
