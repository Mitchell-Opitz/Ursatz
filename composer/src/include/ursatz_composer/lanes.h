#ifndef URSATZ_COMPOSER_LANES_H
#define URSATZ_COMPOSER_LANES_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/tonality/spelled_key.h>
#include <plan/section.h>
#include <theory/harmony/harmonic_function.h>

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

/* The lanes are the section description's (plan/section.h): the same values in the same order. */
_Static_assert((int)PLAN_LANE_METER == (int)SECTION_LANE_METER, "PlanLaneKind follows SectionLane");
_Static_assert((int)PLAN_LANE_KEY == (int)SECTION_LANE_KEY, "PlanLaneKind follows SectionLane");
_Static_assert((int)PLAN_LANE_HARMONY == (int)SECTION_LANE_HARMONY, "PlanLaneKind follows SectionLane");
_Static_assert((int)PLAN_LANE_BASS == (int)SECTION_LANE_BASS, "PlanLaneKind follows SectionLane");
_Static_assert((int)PLAN_LANE_MELODY == (int)SECTION_LANE_MELODY, "PlanLaneKind follows SectionLane");
_Static_assert((int)PLAN_LANE_INNER_VOICE == (int)SECTION_LANE_INNER_VOICE, "PlanLaneKind follows SectionLane");

/* beat_unit is the note value of one beat (4 for a quarter); bars_per_hyperbar is 0 when there is none. */
typedef struct {
    int beats_per_bar;
    int beat_unit;
    int bars_per_hyperbar;
} PlanMeter;

/*
 * A key on the plan's key lane is the library's SpelledKey (R-242): a spelled tonic, the key's pitch material and its mode
 * (set from the material), id NULL. plan_key_of_pitch_class spells the tonic of a pitch class (0 to 11, C = 0, taken mod 12) with
 * the fewest accidentals in the material's signature, sharps on a tie; spelled_key_tonic_pitch_class gives the pitch class back.
 * The material is the major or the natural minor of the call's materials (the Composer writes keys of no other); a key whose mode
 * is read is read from it (composer_key_is_minor). A NULL material gives the zero key (C, no material), which reads as C major.
 */
SpelledKey plan_key_of_pitch_class(int pitch_class, const PitchMaterial *material);

#define PLAN_ROMAN_NUMERAL_MAX 16

typedef struct {
    char roman_numeral[PLAN_ROMAN_NUMERAL_MAX];
    HarmonicFunction function; /* meaningful only when has_function */
    bool has_function;         /* false for chords read from an analysis, which records no function */
} PlanHarmony;

/*
 * A note's identity (R-601): the plan gives a note an id when it enters a lane span, and never gives the same id twice (the next id
 * only rises, so an id of a removed note is not reused). 0 is "none yet": a note the operators write has id 0 and gets its id when
 * it is written into the plan. `source` is the id of the note this one was made from and `operation` how (copy within the plan, a
 * motive carried, a section returned); a note made from nothing has source 0 and operation PLAN_NOTE_FROM_NONE.
 */
typedef size_t PlanNoteId;
#define PLAN_NOTE_NONE ((PlanNoteId)0)

typedef enum {
    PLAN_NOTE_FROM_NONE,
    PLAN_NOTE_FROM_COPY,
    PLAN_NOTE_FROM_MOTIVE,
    PLAN_NOTE_FROM_RETURN
} PlanNoteOperation;

/*
 * A note is a MIDI number and an optional spelling (R-242): the letter and accidental the note was written with. Only
 * notes read from MusicXML are spelled; the operators write unspelled notes, and render spells those in the key under
 * them. offset is measured from the start of the owning lane span.
 */
typedef struct {
    int midi;
    Rational offset;
    Rational duration;
    bool spelled;
    PitchLetter letter;  /* meaningful only when spelled */
    int accidental;      /* meaningful only when spelled */
    PlanNoteId id;       /* 0 until the plan gives one */
    PlanNoteId source;   /* the id of the note this was made from, 0 for none */
    PlanNoteOperation operation;
} PlanNote;

/* A note the operators write: a MIDI number, unspelled, with no id yet. */
static inline PlanNote plan_note(int midi, Rational offset, Rational duration) {
    PlanNote note = {midi, offset, duration, false, PITCH_LETTER_C, 0, PLAN_NOTE_NONE, PLAN_NOTE_NONE, PLAN_NOTE_FROM_NONE};
    return note;
}

/* Notes are copied in; a pointer read back stays valid until the plan is destroyed. */
typedef struct {
    const PlanNote *notes;
    size_t note_count;
} PlanNotes;

/*
 * A key span may be named by a relation instead of a key (R-597): the K6 relation entry (`omt.relation.dominant`) and the anchor it
 * is taken from, the plan's home key or the key span before. A span the Composer drew from a form region has both beside its key
 * (resolved); a span a plan file names only by them is unresolved, its payload holds the zero key until plan_resolve_key_spans
 * (plan_json.h) writes the key it reaches. Writing the payload of a key span does not change the relation.
 */
typedef enum {
    PLAN_KEY_ANCHOR_HOME,
    PLAN_KEY_ANCHOR_PREVIOUS,
    PLAN_KEY_ANCHOR_SECTION /* the key an earlier section is in: the key span at that section's start, named in anchor_section */
} PlanKeyAnchor;

#define PLAN_KEY_RELATION_MAX 64
#define PLAN_KEY_ANCHOR_SECTION_MAX 32

typedef struct {
    char relation[PLAN_KEY_RELATION_MAX]; /* the K6 id; "" for a span with no relation */
    PlanKeyAnchor anchor;
    char anchor_section[PLAN_KEY_ANCHOR_SECTION_MAX]; /* the node name of the anchor section; "" unless anchor is PLAN_KEY_ANCHOR_SECTION */
    bool resolved; /* false while the span's payload holds no key yet */
} PlanKeyRelation;

/* Which member is used follows the lane: meter, key, harmony, or notes for bass, melody and inner voice. */
typedef union {
    PlanMeter meter;
    SpelledKey key;
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

/*
 * Shortens a lane span to new_length, keeping its start, id, state and
 * constraints, so the room it frees can take more spans (a harmony span split
 * into one span per chord). new_length must be positive and no longer than the
 * span. Fails with PLAN_ERR_NOT_FOUND for an unknown id, PLAN_ERR_GIVEN_LOCKED
 * for a Given span, PLAN_ERR_BAD_SPAN for any other length, and
 * PLAN_ERR_BAD_PAYLOAD when the payload no longer fits (notes past the new end).
 */
PlanResult plan_lane_span_resize(PiecePlan *plan, LaneSpanId id, Rational new_length);

bool plan_has_lane_span(const PiecePlan *plan, LaneSpanId id);
/* The last id given out: ids run 1 to this count, and an id removed with its node (plan_remove_subtree) is not a span. */
size_t plan_lane_span_count(const PiecePlan *plan);

/* The accessors below return a zero value or NULL for an unknown id. */
PlanLaneKind plan_lane_span_lane(const PiecePlan *plan, LaneSpanId id);
int plan_lane_span_voice(const PiecePlan *plan, LaneSpanId id);
NodeId plan_lane_span_owner(const PiecePlan *plan, LaneSpanId id);
PlanSpan plan_lane_span_span(const PiecePlan *plan, LaneSpanId id);
const PlanLanePayload *plan_lane_span_payload(const PiecePlan *plan, LaneSpanId id);

/*
 * Note ids (R-601). plan_next_note_id is the id the next new note gets (1 in a new plan; it only rises). plan_note_by_id finds the
 * note with an id among the live note spans of the plan, and writes the span that holds it to *span_out when that is not NULL; NULL
 * for an unknown id (0 included). The pointer stays valid until the span is rewritten or the plan is destroyed.
 *
 * The plan gives ids when a span is added (plan_add_lane_span) or written (plan_lane_span_write): a note with id 0 gets the next id;
 * a note whose id another note of a live span holds (the span being rewritten does not count, nor does the rest of its own old
 * notes) gets the next id too, with the old id as its source and operation PLAN_NOTE_FROM_COPY when it has no source; any other
 * note keeps its id, and the next id stays above it. So a rewrite keeps the ids of the notes it passes through, a copy within the
 * plan gets fresh ones, and an id is never held by two live notes.
 */
PlanNoteId plan_next_note_id(const PiecePlan *plan);
const PlanNote *plan_note_by_id(const PiecePlan *plan, PlanNoteId id, LaneSpanId *span_out);

/*
 * Finds the spans of one lane and voice that overlap range, ordered by start.
 * Writes up to capacity ids to out_ids (which may be NULL when capacity is 0)
 * and returns how many matched in all.
 */
size_t plan_lane_find(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan range, LaneSpanId *out_ids,
                      size_t capacity);

/*
 * Names a key span by a relation and an anchor (see PlanKeyRelation); `resolved` says its payload holds the key the relation reaches.
 * Fails with PLAN_ERR_NOT_FOUND for an unknown id, PLAN_ERR_BAD_PAYLOAD for a span of another lane or a relation id that is empty
 * or does not fit PLAN_KEY_RELATION_MAX, and PLAN_ERR_NULL for a NULL relation. A Given span takes it too (it is a name, not a key).
 * anchor_section names the section node of a PLAN_KEY_ANCHOR_SECTION anchor (1 to PLAN_KEY_ANCHOR_SECTION_MAX - 1 characters) and is NULL
 * for any other, else PLAN_ERR_BAD_PAYLOAD.
 */
PlanResult plan_lane_span_set_key_relation(PiecePlan *plan, LaneSpanId id, const char *relation, PlanKeyAnchor anchor,
                                           const char *anchor_section, bool resolved);

/* The relation a key span is named by, or NULL for a span with none (or an unknown id). */
const PlanKeyRelation *plan_lane_span_key_relation(const PiecePlan *plan, LaneSpanId id);

/* The spans owned by a node, in the order they were added. Same out_ids rules as plan_lane_find. */
size_t plan_node_lane_spans(const PiecePlan *plan, NodeId owner, LaneSpanId *out_ids, size_t capacity);

#endif
