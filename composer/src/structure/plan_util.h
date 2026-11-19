#ifndef URSATZ_COMPOSER_PLAN_UTIL_H
#define URSATZ_COMPOSER_PLAN_UTIL_H

#include <stdbool.h>

#include <model/time/time_signature.h>

#include <plan/plan_constraint.h>
#include <plan/plan_lanes.h>
#include <plan/plan.h>
#include <plan/plan_state.h>

#include <plan/plan_common.h>

/* Small helpers on the plan the sources share. Private: not installed, not part of the API. */

/* Whether node is root or lies below it. */
bool plan_in_subtree(const Plan *plan, NodeId node, NodeId root);

/* The first melody lane span (voice 0) of node that is Given or Realized. */
bool plan_melody_span_of(const Plan *plan, NodeId node, LaneSpanId *out);

/* The part of a larger form a section node is (its description's function, R-503); false when it has none. */
bool plan_section_function_of(const Plan *plan, NodeId node, SectionFunction *out);

/* What a piece operation gives. */
typedef enum {
    PIECE_OK = 0,
    PIECE_ERR_NULL,
    PIECE_ERR_ALLOC,
    PIECE_ERR_MOTIVE_TOO_LONG, /* the motive is longer than the basic idea it fills (2 bars of an 8-bar section, else 1) */
    PIECE_ERR_BAD_REQUEST,     /* a meter or key not supported, a motive of one note, an accompaniment that is not
                                  valid, a section grow-theme cannot build in the meter */
    PIECE_ERR_STUCK,           /* a stage left a target open */
    PIECE_ERR_BAD_SIZE         /* a size or count the template does not have */
} PieceResult;

const char *piece_result_name(PieceResult result);

/* The piece result of a plan result: OK, an allocation failure, else a bad request. */
PieceResult piece_result_of_plan(PlanResult result);

/* Adds a node under parent and sets it Given. */
bool plan_add_given_node(Plan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out);

/* The fragment position names ("any", "leading", "trailing", "middle"), indexed by the fragment step's position (0 for none). */
#define PLAN_FRAGMENT_POSITION_NAME_COUNT 4
extern const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT];
/* The name of a fragment position; "any" for a value that is not one. */
const char *plan_fragment_position_name(long position);

/* The names and pitches the plans the Composer writes share. */
#define GROW_MATERIAL_NAME "material"
#define GROW_SOURCE_NAME "m0"
/* The pitch of the note an open bass, melody or inner voice span holds until an operator writes it. */
#define BASS_PLACEHOLDER 40
#define MELODY_PLACEHOLDER 60
#define INNER_PLACEHOLDER 55

/*
 * The most motives grow-theme makes in the material, one bar each: the two segments of the fragments and a liquidation for each of
 * the two fragments and the cadential idea (apply takes a bar for every one it makes).
 */
#define GROW_MADE_NODES 5

/*
 * Whether the harmony operators can write for a meter: 2/4, 3/4, 4/4 and 6/8, where a chord may start on a
 * strong beat.
 */
bool harmony_meter_supported(const PlanMeter *meter);

/* How many cells a bar of the meter has, which is where a chord may start: one per accent beat of a simple meter, one per
   beat group of a compound one (R-590). False for a meter not supported. */
bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells);
/* The cells in a bar the harmony operators place chords on, as harmony_rhythm_cells_per_bar. */
bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells);

/* The seam section that starts at the time, or PLAN_NODE_NONE. */
NodeId seam_section_at(const Plan *plan, Rational time);

#define ACCOMP_MAX_LINE 256
/* One sounding note of the bass or the melody, clipped to the target. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
} AccompLine;

/* The notes of a lane and voice over a span that sound inside it, from origin; false if any span there is open. */
bool accomp_read_line(const Plan *plan, PlanLaneKind lane, int voice, PlanSpan span, Rational origin,
                      Rational length, AccompLine *out, size_t *count);

#endif
