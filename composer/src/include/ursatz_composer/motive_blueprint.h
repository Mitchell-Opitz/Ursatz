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
 * The rhythms of a blueprint's bars (R-745), as the draws of the blueprint generator (R-751) and of the blueprint motif strategy (R-753)
 * share them. A rhythm is the PlanNote list of one bar: offsets from the bar's start, a gap a rest, the pitches not read; the list holds
 * at most COMPOSER_RHYTHM_NOTES_MAX notes and every `out` below has room for that many.
 */
#define COMPOSER_RHYTHM_NOTES_MAX 63

/* The K11 rhythm cells of the meter that have the job's bit (PATTERN_JOB_*; a mask of 0: every one), up to `capacity`. */
size_t composer_rhythm_cells(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, unsigned job, const PatternEntry **out, size_t capacity);

/* The rhythm of a K11 cell as one bar of the meter; false when its text does not fill a bar exactly or has no note. */
bool composer_rhythm_cell_notes(const PatternEntry *entry, const KnowledgeSnapshot *knowledge, const PlanMeter *meter, PlanNote *out,
                                size_t *out_count);

/*
 * The variant of a rhythm (R-745): one note split in two equal halves or two neighbours joined, one draw (fill_rng_below) among all of
 * them. False, with nothing drawn, for a rhythm with none (no notes).
 */
bool composer_rhythm_variant(const PlanNote *base, size_t count, FillRng *rng, PlanNote *out, size_t *out_count);

/*
 * The contrast to a rhythm (R-745): a cell of the meter whose rhythm is at most half as similar to `base` as similarity_rhythm_score
 * measures it, one draw among those; none that far takes the least similar one with no draw. False when the meter has no cell.
 */
bool composer_rhythm_contrast(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, const PlanNote *base, size_t count, FillRng *rng,
                              PlanNote *out, size_t *out_count);

/* The index of a draw in proportion to the weights (fill_rng_below on their sum); `count` when every weight is 0 (no draw is made). */
size_t composer_draw_weighted(FillRng *rng, const unsigned *weights, size_t count);

/*
 * One K11 cell of the meter with the job (PATTERN_JOB_*), drawn in proportion to its weight: 1, or 1 + `long_weak` when it holds a note of a
 * beat or longer that starts off the downbeat (a beat: the beat unit, a dotted quarter in 6/8); a cell whose first note is exactly a half
 * note has weight 0 when `previous_ends_half` (the bar before ends with exactly a half note). The cell and its rhythm in out/out_count; NULL,
 * with no draw, when none has weight. With `commit` false nothing is drawn: the first cell is answered when one has weight, NULL when none.
 */
const PatternEntry *composer_rhythm_draw_cell(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, unsigned pattern_job,
                                              bool previous_ends_half, unsigned long_weak, bool commit, FillRng *rng, PlanNote *out,
                                              size_t *out_count);

/*
 * Attaches a rhythm-only motive (R-746): the motive's notes (their pitches are not read: a rhythm sounds degree 1) as a Given motive
 * node under parent at start, as motive_attach does, with a motive_tones constraint that labels every note "rhythm". The melody stage
 * then places them as notes of its own (melody_context.c), and the harmony stage does not read the constraint's chords: the motive's
 * own chords when it has some, else one "I" over its length. PLAN_ERR_BAD_PAYLOAD for a motive of more notes than a plan's tones hold;
 * the other results are motive_attach's.
 */
PlanResult composer_rhythm_motive_attach(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                                         NodeId *out_node, LaneSpanId *out_span);

/*
 * The blueprint preferences (R-750, composer/docs/preferences-blueprint.md): what the blueprint generator and the blueprint motif strategy
 * draw from. The weights are whole numbers 0 to 20 (0 never draws it). sounds: how often a sound is drawn against none (ninth on the
 * downbeat, suspended fourth before the dominant, appoggiatura, major seventh, leading tone into the tonic) and `long_weak`, the extra
 * weight of a rhythm cell holding a note of a beat or longer off the downbeat. jobs: per formal function of an idea, the weight of each
 * rhythm job a unit may take. climax: the weight of the climax in bar 5 (m6) and in bar 6 (m7) of a sentence. range: the semitones
 * the melody spans below the climax (12 to 24). candidates: the themes drawn for one request to rank (1 to 8).
 */
typedef enum {
    BLUEPRINT_SOUND_NINTH_DOWNBEAT,
    BLUEPRINT_SOUND_SUS4_DOMINANT,
    BLUEPRINT_SOUND_APPOGGIATURA,
    BLUEPRINT_SOUND_MAJOR_SEVENTH,
    BLUEPRINT_SOUND_LEADING_TONE,
    BLUEPRINT_SOUND_LONG_WEAK,
    BLUEPRINT_SOUND_COUNT
} BlueprintSound;

typedef enum {
    BLUEPRINT_FUNCTION_BASIC,
    BLUEPRINT_FUNCTION_FRAGMENT,
    BLUEPRINT_FUNCTION_CONTRASTING,
    BLUEPRINT_FUNCTION_CADENTIAL,
    BLUEPRINT_FUNCTION_COUNT
} BlueprintFunction;

typedef enum {
    BLUEPRINT_JOB_RUNNING,
    BLUEPRINT_JOB_LONG,
    BLUEPRINT_JOB_SYNCOPATED,
    BLUEPRINT_JOB_VARIANT,
    BLUEPRINT_JOB_CONTRAST,
    BLUEPRINT_JOB_COUNT
} BlueprintJob;

typedef struct {
    unsigned sounds[BLUEPRINT_SOUND_COUNT];
    unsigned jobs[BLUEPRINT_FUNCTION_COUNT][BLUEPRINT_JOB_COUNT];
    unsigned climax[2]; /* m6 (bar 5), m7 (bar 6) */
    unsigned range;
    unsigned candidates;
} BlueprintPreferences;

/* Writes the defaults of R-750 into *out (composer/src/preferences/preferences_blueprint.c). */
void blueprint_preferences_default(BlueprintPreferences *out);

#endif
