#ifndef URSATZ_COMPOSER_MELODY_INTERNAL_H
#define URSATZ_COMPOSER_MELODY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>

#include "harmony/harmony_internal.h"
#include "util/position.h"

/*
 * Inside the melody operator, a time is a Rational position in whole notes from the start of the target (util/position.h).
 * The shortest note the operator writes is a 128th: a density that would divide a cell into shorter ones keeps the cell.
 */
#define MELODY_FINEST_NOTE_DENOMINATOR 128L
#define MELODY_MAX_NOTES 256
#define MELODY_MAX_BASS 256
/* The most notes of a pickup (R-168). */
#define MELODY_MAX_PICKUP 4
#define MELODY_PAYLOAD_MAGIC 0x4D454C31UL
/* The smallest leap, in semitones, that a step the other way is asked to follow. */
#define MELODY_RECOVER_FROM 5

/* One note of the bass lane: when it sounds, its pitch, and the chord it sounds under. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
    size_t chord;
} MelodyBass;

/* What the operator reads from the plan: the harmony and bass under the phrase, and the motive to place. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    AnalysisKey key;
    PhraseMetre metre;
    Rational length;
    Rational cell;
    /*
     * The anacrusis before the target (PLAN_CONSTRAINT_PICKUP): its length, 0 for none. The target's span is what follows
     * it, so the notes of the pickup have onsets from -pickup, and the melody span the notes are written to starts
     * `pickup` before the target. The pitch classes of the chord sounding under the pickup (the harmony span
     * before the target) are `pickup_mask`, 0 when there is none, as at the start of a plan.
     */
    Rational pickup;
    unsigned pickup_mask;
    size_t count;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    Rational chord_start[HARMONY_MAX_CHORDS + 1]; /* the last entry is the phrase's length */
    unsigned chord_mask[HARMONY_MAX_CHORDS];  /* the pitch classes of each chord, bit pc */
    unsigned chord_seventh[HARMONY_MAX_CHORDS]; /* the pitch class of its seventh, or 0 */
    MelodyBass bass[MELODY_MAX_BASS];
    size_t bass_count;
    bool has_cadence;
    CadenceType cadence;
    bool motive_given; /* the motive is the original, not a development */
    bool restated; /* the motive_ref is an exact restatement of its source */
    NodeId motive_node;
    size_t motive_count;
    int motive_pitch[MOTIVE_MAX_NOTES];
    Rational motive_onset[MOTIVE_MAX_NOTES];
    Rational motive_duration[MOTIVE_MAX_NOTES];
    Rational motive_length;
    /*
     * The tone roles of the motive's notes (R-215), from the motive_tones constraint of its node (motive_tones in the plan,
     * read through the motive_ref chain of a developed motive). motive_tone[k] is MELODY_TONE_CHORD, MELODY_TONE_NONE (no
     * role: unclassified) or the NonChordTone of note k. The first motive_tone_count notes have a role; an exact
     * restatement says the whole melody of the idea before, whose later notes have none. 0 means no motive has tones, and
     * the motive is placed as it was before tones.
     */
    int motive_tone[MOTIVE_MAX_NOTES];
    size_t motive_tone_count;

    /*
     * A target that stands for an idea or a subphrase of a phrase (not the whole phrase) reads the phrase it is in,
     * and the melody written before it. Times are positions from the start of the target, so the phrase starts at or
     * before 0. The chord and bass lists are those under the target, the first and last clipped to it.
     */
    bool idea;
    Rational phase;         /* from the cell line at or before the target's start to the start (0 when it starts on one) */
    Rational phrase_start;  /* at most 0 */
    Rational phrase_length;
    bool phrase_has_top; /* a melody note of the phrase is written before the target */
    int phrase_top;      /* the highest pitch of those */
    size_t phrase_before_notes; /* how many melody notes of the phrase are written before the target */
    int lead_in[2];      /* the last notes of the written melody right before the target, oldest first */
    size_t lead_in_count;
    int lead_out;        /* the first note of the melody, Given or written, that starts where the target ends */
    bool has_lead_out;
    /* The target ends where the seam section starts (R-171): the first bass note of the seam and the melody note after
       lead_out, -1 for none. The candidates are also scored by how the end leads into the seam. */
    bool at_seam;
    int seam_bass;
    int seam_next;
    bool restating;      /* the motive is the melody of the idea before, to be said again as it was */
} MelodyContext;

#define MELODY_TONE_CHORD (-1)
#define MELODY_TONE_NONE (-2)

/* Whether a position of the target is the start of a cell of the meter (a strong beat). */
static inline bool melody_on_cell(const MelodyContext *context, Rational position) {
    return position_is_multiple(position_add(position, context->phase), context->cell);
}

/* Where a target sits in its phrase's arch (see melody_role). A target that is the whole phrase is MELODY_ROLE_WHOLE. */
typedef enum {
    MELODY_ROLE_WHOLE,
    MELODY_ROLE_BEFORE, /* the phrase's climax falls in a later target: no note above range_high - 3 */
    MELODY_ROLE_CLIMAX, /* the phrase's climax falls in this target: it writes the phrase's highest note */
    MELODY_ROLE_AFTER   /* the climax is written: every note is below it */
} MelodyRole;

/* The position of the target at which the phrase's climax falls, which may lie before 0 or past the end. */
Rational melody_climax_position(const MelodyContext *context, const MelodyFillParams *params);
MelodyRole melody_role(const MelodyContext *context, const MelodyFillParams *params);

/* One note of a candidate. motive_index is the motive note it carries, or -1 for a note that fills the phrase. */
typedef struct {
    Rational onset;
    Rational duration;
    int pitch;
    int motive_index;
    bool strong; /* it starts on a cell */
    size_t chord; /* the chord sounding at its onset */
    bool fixed;      /* a figure of the motive's tones whose pitch is set; the search between anchors keeps it (R-215) */
    bool chord_only; /* a figure that could not be formed: the search gives it a chord tone */
} MelodyNote;

/* A candidate's payload. */
typedef struct {
    unsigned long magic;
    MelodyContext context;
    MelodyFillParams params;
    MelodyNote notes[MELODY_MAX_NOTES];
    size_t count;
    MelodyNote pickup_notes[MELODY_MAX_PICKUP]; /* the anacrusis, oldest first, onsets from -context.pickup */
    size_t pickup_count;
} MelodyCandidate;

/* Sets every position of a context to 0, so a context that is not read still holds valid Rationals. */
void melody_context_zero(MelodyContext *context);

/* The pitch classes (bit pc of mask) of a chord in the key, and of its seventh (0 when it has none). */
bool melody_chord_mask(const AnalysisKey *key, const PhraseChord *chord, unsigned *mask, unsigned *seventh);
/* The index of the chord sounding at a position of the phrase. */
size_t melody_chord_at(const MelodyContext *context, Rational position);

/*
 * Reads the harmony and bass under a phrase: the chord spans must tile the span exactly, none of them open, and
 * the bass spans must tile it. The target and the motive are not read. False when the phrase cannot be read.
 */
bool melody_phrase_read(const PiecePlan *plan, NodeId owner, PlanSpan span, AnalysisKey key, MelodyContext *out);

/*
 * Reads the harmony, bass and motive under a Constrained melody span: the chord spans must tile the span exactly,
 * none of them open, the bass spans must tile it, and a motive_ref constraint must name a motive with a melody.
 * False when the span is not one the melody operator applies to.
 */
bool melody_context_read(const PiecePlan *plan, LaneSpanId target, MelodyContext *out);

/*
 * The pitch classes a note must be one of: those the chords it sounds through share, so that a note that is
 * held into the next chord is a chord tone of it too.
 */
unsigned melody_need_mask(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *note);

/* The pitch classes of the key's scale at chord (the seventh degree raised when the chord has the raised one). */
unsigned melody_scale_mask(const MelodyContext *context, size_t chord);

/*
 * Whether note i of a melody is allowed: a chord tone of every chord it sounds through, or a non-chord tone of a
 * figure params->nct_types allows (passing, neighbor, appoggiatura, suspension, anticipation), approached and left
 * as NonChordTone defines the figure, and never on a strong beat but as an appoggiatura or a suspension. It reads
 * the pitches of notes i - 1 and i + 1.
 */
bool melody_figure_ok(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *notes,
                      size_t count, size_t i);

/*
 * Gives the notes between from and to, which are non-chord-tone candidates, pitches with the pitches of notes from
 * and to fixed: chord tones, or the non-chord tones of figures that hold. ideal[j] is the pitch the note j would
 * take if it could, and nothing above cap is used. False when no assignment holds.
 */
bool melody_fill_between(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes,
                         size_t count, size_t from, size_t to, const int *ideal, int cap, FillRng *rng);

/*
 * A note that must be a chord tone: the first and the last, any on a strong beat, any as long as a cell, and any
 * held into the next chord. The others, when non-chord tones are allowed, are filled in between these.
 */
bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i);

/*
 * The shape of the phrase after the motive, planned once the motive's notes are placed: the note that is the
 * climax and its pitch, and the highest pitch any other note after the motive may take.
 */
typedef struct {
    bool made;
    bool in_motive; /* the climax is in the motive; no note after it is as high */
    size_t climax;  /* the index of the climax note, when it is after the motive */
    int climax_pitch;
    int cap;        /* the highest pitch of any other note after the motive */
    int motive_top; /* the highest pitch of the motive's notes placed so far */
} MelodyContour;

/* Plans the contour at note first, the first note after the motive. False when no climax can be placed. */
bool melody_contour_plan(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                         size_t first, FillRng *rng, MelodyContour *out);
/* The pitch the arch would give anchor i, whose anchor before it is `before`. */
int melody_contour_ideal(const MelodyContext *context, const MelodyContour *contour, const MelodyCandidate *melody,
                         size_t i, size_t before);
/* Whether exactly one note is the highest. */
bool melody_contour_unique(const MelodyCandidate *melody);
/*
 * How many of the contour's soft rules hold: one climax, at about the asked place, leaps of a fourth or more
 * recovered by a step the other way, and not many repeated notes. *held of *total.
 */
void melody_contour_score(const MelodyCandidate *melody, unsigned *held, unsigned *total);

/* The pitch classes the last note may be: the cadence's stable tone (the tonic for a PAC, the third for an IAC). */
unsigned melody_cadence_mask(const MelodyContext *context);

/*
 * Judges a melody: every rule of the checker (see melody_fill.h). Hard rules reject; soft rules score. False only
 * when it fails itself (allocation).
 */
bool melody_judge(const MelodyCandidate *melody, PhraseVerdict *verdict);
/*
 * The non-chord tone each note of the melody is, as the MELODY_NCT_ bit of the figure the framework labels it (0 for a
 * chord tone), types[0..count). False when the melody cannot be labeled. The same labeling melody_judge holds the
 * notes to, for the style's non-chord tone figures.
 */
bool melody_nct_types(const MelodyCandidate *melody, unsigned *types);
/*
 * Surveys the labeling of a melody without judging it: types[i] is the MELODY_NCT_ bit of the figure the framework gives
 * note i (0 for a chord tone or a note it cannot classify), and unclassified[i] is set for a note outside its chord that
 * it cannot classify. Every figure counts, whatever params.nct_types allows. False when the melody cannot be labeled.
 */
bool melody_nct_survey(const MelodyCandidate *melody, unsigned *types, bool *unclassified);
/* The MELODY_NCT_ bit of a NonChordTone (an int), 0 for a type the operator has no bit for. */
unsigned melody_nct_bit(int tone);
/* The SHAPING_ bits (check/shaping.h) of the shaping rules the melody does not hold, whatever params.plain_shaping says. */
unsigned melody_shaping_broken(const MelodyCandidate *melody);
/* The checker's verdict on a melody candidate's payload. */
bool melody_check(const void *payload, FillCheck *out);

/*
 * Builds one melody for the context, choosing only with rng. False when the motive cannot be placed (it does not
 * fit the phrase or the range, or a strong beat has no chord tone near enough to its contour).
 */
bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out);

/*
 * Realizes the motive of the context in a candidate whose notes are laid out (layout): the motive's notes, each moved
 * `shift` semitones, are bent to the chords as a whole (R-328). With keep_figures a clash that the framework labels as a
 * figure stays; every other clash moves to the nearest chord tone, or to a pitch that keeps its own figure. The figures
 * kept are added to out->params.nct_types. Uses no random numbers. False when a note falls out of the range.
 */
bool melody_realize(const MelodyContext *context, const MelodyFillParams *params, int shift, bool keep_figures,
                    MelodyCandidate *out);
/*
 * The octave shifts of {0, -12, 12, -24, 24} that put every motive note in the range, best first (R-334); shifts holds
 * five. Returns how many.
 */
size_t melody_realize_shifts(const MelodyContext *context, const MelodyFillParams *params, int *shifts);

/* How many of the standard counterpoint melodic rules have a soft preference. */
unsigned melody_melodic_soft_rules(void);

#endif
