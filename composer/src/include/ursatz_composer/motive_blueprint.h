#ifndef URSATZ_COMPOSER_MOTIVE_BLUEPRINT_H
#define URSATZ_COMPOSER_MOTIVE_BLUEPRINT_H

#include <stdbool.h>
#include <stddef.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/motive.h>

/*
 * The melody of a blueprint (R-746, composer/docs/blueprints.md): where a chosen non-chord tone sounds between the notes
 * placed around it.
 */

/*
 * The pitch of an event (a chosen non-chord tone) between two placed notes, `previous` before it and `next` after it, by
 * R-746's rule, in the scale of the key (the natural scale, with the pitch classes of the two neighbours added, so a raised
 * leading tone steps as the scale does around it):
 *   appoggiatura   a scale step above next; below when previous is below next and next - previous is a step (1 or 2)
 *   suspension     previous
 *   retardation    previous
 *   passing        the scale step between when previous and next are a third apart (3 or 4 semitones), else the scale
 *                  tone nearest their middle (the lower on a tie)
 *   neighbor       a scale step above previous; below when next is above previous
 *   escape         a scale step from previous away from next (the direction of the line it leaves by a leap); up when
 *                  next is previous
 *   anticipation   next
 * A missing neighbour (-1) takes the other's pitch; -1 when both are missing. The chords are not read (the rule is the
 * scale's); NULL is allowed.
 */
int composer_event_pitch(const ComposerChords *chords, const SpelledKey *key, PlanEventType type, int previous, int next);

/* The most anchors and events a blueprint motive takes: eight anchors and four events a bar, over four bars. */
#define BLUEPRINT_MOTIVE_MAX_ANCHORS 32
#define BLUEPRINT_MOTIVE_MAX_EVENTS 16

/*
 * What a blueprint motive is drawn from (R-746): the key and meter, the notes' onsets and durations (a rhythm text read by the
 * note-list reader, rests as gaps between them), the chords by onset (each sounds from its onset to the next; the first at 0),
 * the anchors and events by offset (each the onset of a note), and the melody range. Times are whole notes from the motive's
 * start.
 */
typedef struct {
    SpelledKey key;
    PlanMeter meter;
    size_t note_count;
    Rational onsets[MOTIVE_MAX_NOTES];
    Rational durations[MOTIVE_MAX_NOTES];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t chord_count;
    PlanAnchor anchors[BLUEPRINT_MOTIVE_MAX_ANCHORS];
    size_t anchor_count;
    PlanEvent events[BLUEPRINT_MOTIVE_MAX_EVENTS];
    size_t event_count;
    int range_low;
    int range_high;
} BlueprintMotiveRequest;

/*
 * Draws a full motive (R-746): the anchors take their pitch; then each other note in order, not an event, takes the chord tone
 * of the chord sounding at its onset nearest the note placed before it (the first: nearest the middle of the range), a step
 * (1 or 2 semitones) where a chord tone lies a step away, never the pitch of the note before, inside the range; a leap of a
 * fourth or more (5 semitones or more) at most once, followed by a step the other way where a chord tone lies there; ties
 * between equal distances drawn from rng. A note right after an event that has an anchor's pitch takes the event's resolution
 * when it is a chord tone in range: a scale step below an appoggiatura or a suspension, above a retardation, the pitch of an
 * anticipation, the note before a neighbor, a step on through a passing tone. Then each event without an anchor takes
 * composer_event_pitch from the notes before and after it (a following event is passed over). The motive has the key, the
 * meter, the chords as its own harmony, and a tone override on every note: the event's type, else "chord" for a chord tone.
 *
 * MOTIVE_ERR_NULL for a NULL argument; MOTIVE_ERR_VALUE, with the reason in error (may be NULL), for no notes or too many,
 * notes that overlap, no chords, an anchor or event whose offset is no note's onset, or a note that cannot be placed in the
 * range ("note 3 at 1/2: no chord tone of IV in 63 to 79").
 */
MotiveResult composer_blueprint_motive(const ComposerChords *chords, const BlueprintMotiveRequest *request, FillRng *rng,
                                       Motive *out, MotiveError *error);

/*
 * Attaches a rhythm-only motive (R-746): the motive's notes (their pitches are not read: a rhythm sounds degree 1) as a Given motive
 * node under parent at start, as motive_attach does, with a motive_tones constraint that labels every note "rhythm". The melody stage
 * then places them as notes of its own (melody_context.c), and the harmony stage does not read the constraint's chords: the motive's
 * own chords when it has some, else one "I" over its length. PLAN_ERR_BAD_PAYLOAD for a motive of more notes than a plan's tones hold;
 * the other results are motive_attach's.
 */
PlanResult composer_rhythm_motive_attach(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                                         NodeId *out_node, LaneSpanId *out_span);

#endif
