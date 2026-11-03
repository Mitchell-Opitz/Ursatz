#ifndef URSATZ_COMPOSER_RENDER_H
#define URSATZ_COMPOSER_RENDER_H

#include <stddef.h>

#include <events/note_event.h>
#include <pitch/pitch_numeric_value.h>
#include <time/meter_map.h>
#include <time/musical_time.h>

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
 * A span owned by a section with a MATERIAL constraint, or by a node under one, is skipped with
 * RENDER_SKIP_MATERIAL, whatever its lane or state: source material is not part of the piece. The piece ends
 * where the last section that is not material ends (piece_end).
 * The same holds for the seam section of a piece structure (a SEAM constraint, R-171), the Given copy of the body's
 * first bar after the loop's end that the loop's end is filled against: its spans are skipped with RENDER_SKIP_SEAM, it
 * has no section marker, and the piece ends where the loop does.
 * A span in a section with a LAYERS constraint (a layer of a layered form) whose lane the mask leaves out is skipped
 * with RENDER_SKIP_LAYER: the section is grown in full, and the mask decides only what sounds.
 * Only Given and Realized spans are rendered. A Constrained span is skipped,
 * whatever its lane, and so is every harmony span, which makes no sound until
 * accompaniment exists. Every skipped span is in the report.
 *
 * Pitch is a MIDI note number under Ursatz's "midi-note-number" tuning. The
 * written pitch is spelled from the key in force at the note's start: flats in
 * a flat key, sharps otherwise (and when no key applies). MIDI 60 is C4. A key
 * signature is the one with the fewest accidentals, sharps on a tie.
 *
 * Sections: every section of the form that is not material, at its start, labelled with its SECTION_LETTER constraint
 * or else its node name (a range render has those that start inside the range).
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

/* A section's start: where an export puts a marker. label is the form letter ("A", "B", "A'") or the node name. */
typedef struct {
    char *label;           /* owned */
    MusicalTime *position; /* owned; from the start of the render */
} RenderedSection;

typedef enum {
    RENDER_SKIP_NOT_REALIZED,   /* the span is Constrained */
    RENDER_SKIP_LANE_NOT_RENDERED, /* the lane makes no sound yet (harmony) */
    RENDER_SKIP_MATERIAL,          /* the span belongs to a section marked as source material */
    RENDER_SKIP_LAYER,             /* the span's lane is not in the LAYERS mask of the section it is in */
    RENDER_SKIP_SEAM               /* the span belongs to the seam section, which is not part of the piece */
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
    LaneSpanId *spans;                /* owned; parallel to events: the lane span each note was written in */
    RenderedPart *parts;              /* owned; only parts that have notes */
    size_t part_count;
    MeterMap *meter_map;              /* owned; NULL when no meter span was rendered */
    RenderedKey *keys;                /* owned; in time order */
    size_t key_count;
    RenderSkip *skipped;              /* owned; in lane span id order */
    size_t skipped_count;
    PlanTime piece_end;               /* the end of the last section that is not material; 0 when the plan has none */
    RenderedSection *sections;        /* owned; the sections that are not material, in time order */
    size_t section_count;
    char *form_name;                  /* owned; the form node's name, NULL when the plan has no form or it has no name */
} RenderedPlan;

/* Renders plan into *out, which the caller destroys. On failure *out is empty and safe to destroy. */
PlanRenderResult plan_render(const PiecePlan *plan, RenderedPlan *out);

/*
 * Renders only the part of the plan inside range, measured from the range's start: a note span that starts in the
 * range, and the key and meter spans that overlap it (the first of each starting at 0). piece_end is the range's
 * length. The same rules as plan_render otherwise.
 */
PlanRenderResult plan_render_range(const PiecePlan *plan, PlanSpan range, RenderedPlan *out);

/* Frees everything and zeroes the struct. Safe on a zeroed struct. */
void rendered_plan_destroy(RenderedPlan *rendered);

/* "melody", "bass" or "inner_voice". */
const char *rendered_part_lane_name(const RenderedPart *part);

/* The part's name in an export: "melody", "bass", "inner_voice_2". Written to out, which holds size bytes. */
void rendered_part_name(const RenderedPart *part, char *out, size_t size);

/* The id value of the voice reference every NoteEvent of the part carries (entity_id_value of its target). */
void rendered_part_voice_id(const RenderedPart *part, char *out, size_t size);

#endif
