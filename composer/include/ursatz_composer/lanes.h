#ifndef URSATZ_COMPOSER_LANES_H
#define URSATZ_COMPOSER_LANES_H

#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * Lanes: time-aligned layers of the plan. Each lane span belongs to one node
 * and lies inside that node's span. Spans in the same lane (and, for inner
 * voices, the same voice) never overlap; spans are half-open, so a span that
 * starts where another ends is fine.
 */

typedef size_t LaneSpanId;
#define PLAN_LANE_SPAN_NONE ((LaneSpanId)0)

typedef enum {
    PLAN_LANE_METER,
    PLAN_LANE_KEY,
    PLAN_LANE_HARMONY,
    PLAN_LANE_BASS,
    PLAN_LANE_MELODY,
    PLAN_LANE_INNER_VOICE
} PlanLaneKind;

typedef enum {
    PLAN_MODE_MAJOR,
    PLAN_MODE_MINOR
} PlanKeyMode;

typedef enum {
    PLAN_FUNCTION_TONIC,
    PLAN_FUNCTION_PREDOMINANT,
    PLAN_FUNCTION_DOMINANT,
    /* For chords read from an analysis, which records no function. */
    PLAN_FUNCTION_UNKNOWN
} PlanHarmonicFunction;

/* beat_unit is the note value of one beat (4 for a quarter); bars_per_hyperbar is 0 when there is none. */
typedef struct {
    int beats_per_bar;
    int beat_unit;
    int bars_per_hyperbar;
} PlanMeter;

/* tonic_pitch_class is 0 to 11, C = 0. */
typedef struct {
    int tonic_pitch_class;
    PlanKeyMode mode;
} PlanKey;

#define PLAN_ROMAN_NUMERAL_MAX 16

typedef struct {
    char roman_numeral[PLAN_ROMAN_NUMERAL_MAX];
    PlanHarmonicFunction function;
} PlanHarmony;

/* offset is measured from the start of the owning lane span. */
typedef struct {
    int midi_pitch;
    PlanTime offset;
    PlanTime duration;
} PlanNote;

/* Notes are copied in; a pointer read back stays valid until the plan is destroyed. */
typedef struct {
    const PlanNote *notes;
    size_t note_count;
} PlanNotes;

/* Which member is used follows the lane: meter, key, harmony, or notes for bass, melody and inner voice. */
typedef union {
    PlanMeter meter;
    PlanKey key;
    PlanHarmony harmony;
    PlanNotes notes;
} PlanLanePayload;

/*
 * Adds a lane span for owner. voice is 0 except for PLAN_LANE_INNER_VOICE,
 * where it numbers the voice. Fails with PLAN_ERR_NOT_FOUND for an unknown
 * owner, PLAN_ERR_BAD_SPAN when the span is not inside the owner's,
 * PLAN_ERR_LANE_OVERLAP when it overlaps a span in the same lane and voice,
 * and PLAN_ERR_BAD_PAYLOAD for a payload the lane cannot hold.
 */
PlanResult plan_add_lane_span(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span,
                              const PlanLanePayload *payload, LaneSpanId *out_id);

bool plan_has_lane_span(const PiecePlan *plan, LaneSpanId id);
size_t plan_lane_span_count(const PiecePlan *plan);

/* The accessors below return a zero value or NULL for an unknown id. */
PlanLaneKind plan_lane_span_lane(const PiecePlan *plan, LaneSpanId id);
int plan_lane_span_voice(const PiecePlan *plan, LaneSpanId id);
NodeId plan_lane_span_owner(const PiecePlan *plan, LaneSpanId id);
PlanSpan plan_lane_span_span(const PiecePlan *plan, LaneSpanId id);
const PlanLanePayload *plan_lane_span_payload(const PiecePlan *plan, LaneSpanId id);

/*
 * Finds the spans of one lane and voice that overlap range, ordered by start.
 * Writes up to capacity ids to out_ids (which may be NULL when capacity is 0)
 * and returns how many matched in all.
 */
size_t plan_lane_find(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan range, LaneSpanId *out_ids,
                      size_t capacity);

/* The spans owned by a node, in the order they were added. Same out_ids rules as plan_lane_find. */
size_t plan_node_lane_spans(const PiecePlan *plan, NodeId owner, LaneSpanId *out_ids, size_t capacity);

#endif
