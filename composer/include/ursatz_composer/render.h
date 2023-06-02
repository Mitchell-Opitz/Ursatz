#ifndef URSATZ_COMPOSER_RENDER_H
#define URSATZ_COMPOSER_RENDER_H

#include <stddef.h>

#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Rendering: a plan's realized lanes as Ursatz NoteEvents, ready for Ursatz's
 * MIDI renderer and MusicXML exporter (see export.h).
 *
 * What is rendered:
 *   - Notes of the melody, bass and inner voice lanes. A note sounds at its
 *     span's start plus its offset. Each lane voice is one part: melody, then
 *     bass, then inner voices by voice number. A NoteEvent carries a voice
 *     reference naming its part.
 *   - Meter spans, as a MeterMap: the time signature is beats_per_bar over
 *     beat_unit, from the start of each span.
 *   - Key spans, as key signatures from each span's start.
 * Only Given and Realized spans are rendered. A Constrained span is skipped,
 * whatever its lane, and so is every harmony span, which makes no sound until
 * accompaniment exists. Every skipped span is in the report.
 *
 * Pitch is a MIDI note number under Ursatz's "midi-note-number" tuning. The
 * written pitch is spelled from the key in force at the note's start: flats in
 * a flat key, sharps otherwise (and when no key applies). MIDI 60 is C4. A key
 * signature is the one with the fewest accidentals, sharps on a tie.
 *
 * The same plan always gives the same events, in the same order: by time, then
 * part, then lane span, then position in the span.
 */

typedef enum {
    PLAN_RENDER_OK = 0,
    PLAN_RENDER_ERR_NULL,
    PLAN_RENDER_ERR_ALLOC,
    PLAN_RENDER_ERR_NOT_REPRESENTABLE
} PlanRenderResult;

/* One part: a lane voice. voice is 0 except for inner voices. */
typedef struct {
    PlanLaneKind lane;
    int voice;
} RenderedPart;

/* A key signature from position on. sharps_flats is -7 to 7. */
typedef struct {
    int sharps_flats;
    PlanKeyMode mode;
    MusicalTime *position; /* owned */
} RenderedKey;

typedef enum {
    RENDER_SKIP_NOT_REALIZED,   /* the span is Constrained */
    RENDER_SKIP_LANE_NOT_RENDERED /* the lane makes no sound yet (harmony) */
} RenderSkipReason;

typedef struct {
    LaneSpanId span;
    PlanLaneKind lane;
    RenderSkipReason reason;
} RenderSkip;

typedef struct {
    NoteEvent **events;               /* owned; count entries, in time order */
    PitchNumericValue **pitch_values; /* owned; parallel to events */
    size_t count;
    size_t *part_index;               /* owned; parallel to events: an index into parts */
    RenderedPart *parts;              /* owned; only parts that have notes */
    size_t part_count;
    MeterMap *meter_map;              /* owned; NULL when no meter span was rendered */
    RenderedKey *keys;                /* owned; in time order */
    size_t key_count;
    RenderSkip *skipped;              /* owned; in lane span id order */
    size_t skipped_count;
} RenderedPlan;

/* Renders plan into *out, which the caller destroys. On failure *out is empty and safe to destroy. */
PlanRenderResult plan_render(const PiecePlan *plan, RenderedPlan *out);

/* Frees everything and zeroes the struct. Safe on a zeroed struct. */
void rendered_plan_destroy(RenderedPlan *rendered);

/* "melody", "bass" or "inner_voice". */
const char *rendered_part_lane_name(const RenderedPart *part);

#endif
