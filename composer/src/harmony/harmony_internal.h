#ifndef URSATZ_COMPOSER_HARMONY_INTERNAL_H
#define URSATZ_COMPOSER_HARMONY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis_cadence.h>
#include <analysis_input.h>
#include <harmony_chord.h>
#include <pitch/pitch_spelling.h>
#include <semantics/beat_strength.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>

/* What a harmony span asks for, read from the plan. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKeyMode mode;
    PlanCadenceType cadence;
    PlanMeter meter;
    PlanTime bar_length;
    size_t bars;
    size_t cells_per_bar; /* the places in a bar where a chord may start */
    PlanTime cell_length;
    size_t cells;         /* bars * cells_per_bar */
    bool has_shape;       /* a harmony shape constraint (PHRASE_MODEL, CADENTIAL or PROLONGATION) is on the span or its owner */
    PlanHarmonyShape shape;
} PhraseRequest;

/* The meter under a phrase: its bars and the places in a bar where a chord may start. */
typedef struct {
    PlanMeter meter;
    PlanTime bar_length;
    size_t bars;
    size_t cells_per_bar;
    PlanTime cell_length;
    size_t cells; /* bars * cells_per_bar */
} PhraseMetre;

/* One chord of a progression. A chord starts where the one before it ends. */
typedef struct {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    PlanHarmonicFunction function;
    HarmonyChord chord;          /* the chord, when it is not applied */
    bool applied;
    HarmonyAppliedChord applied_chord; /* the chord, when it is applied */
    int applied_inversion;             /* 0 for root position; a triad has 0 or 1, a seventh chord 0 to 3 */
    size_t cells;
} PhraseChord;

/* A candidate's payload. */
typedef struct {
    PhraseRequest request;
    PhraseChord *chords;
    size_t count;
} PhraseProgression;

#define HARMONY_MAX_CHORDS 64

/* A phrase of chord spans in the plan, as the embellishment operator reads it. */
typedef struct {
    NodeId owner;
    PlanSpan span;
    PlanKeyMode mode;
    PlanCadenceType cadence; /* the cadence constraint on the last chord */
    PhraseMetre metre;
    size_t count;
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t target; /* the index of the marked chord */
} PhraseContext;

/*
 * The chord as the recognizer wants it: a set of intervals above the root, in semitones, by the
 * quality the chord has in the key. False for a chord this module does not write.
 */
bool harmony_diatonic_intervals(PlanKeyMode mode, const HarmonyChord *chord, int intervals[4], size_t *count);
bool harmony_applied_intervals(const HarmonyAppliedChord *applied, int intervals[4], size_t *count);

/* A phrase as the recognizer is shown it. */
typedef struct {
    const PhraseChord *chords;
    size_t count;
    PlanKeyMode mode;
    PlanCadenceType requested;
    size_t cells_per_bar;
    PlanTime cell_length;
    /* A phrase that need not end in a cadence, nor explain every inverted chord: a harmonization of a given bass. */
    bool open_ended;
    /* Chords, by index, that need no reason to be there beyond the phrase they are in: a schema's. NULL for none. */
    const bool *exempt;
    /* A cadential progression, or a prolongation, has no opening tonic: the phrase model's tonic-then-dominant is not asked. */
    bool no_opening_tonic;
    /* A prolongation: no cadence may be heard anywhere in it. Used with open_ended and no_opening_tonic. */
    bool no_cadence;
} HeardPhrase;

HarmonyMode harmony_mode_of(PlanKeyMode mode);

/* Reads the chords of the harmony lane over span, which must tile it exactly. False if it cannot be read. */
bool harmony_read_lane(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PhraseChord *chords, size_t *count,
                       PhraseMetre *metre);

/* What the recognizer hears at the end of the phrase, and whether it arrives on the last chord. */
bool harmony_hear_phrase(const HeardPhrase *phrase, CadenceType *heard, bool *at_end);

/*
 * Every cadence the recognizer hears over the harmony lane chords of span (read as harmony_read_lane does), the
 * soprano over chord i being soprano[i], a scale degree or 0 for none; count must be the number of chords. On true,
 * *out is the caller's (harmony_cadences_destroy), in order of arrival, its segment indices the chords'.
 */
bool harmony_hear_lane_cadences(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, const int *soprano,
                                size_t count, HarmonyCadences *out);

/* How many strong beats a bar of the meter has, which is where a chord may start. False for a meter not supported. */
bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells);
/* The strength of the cell with this index in its bar: the first is strong, any other medium. */
BeatStrength harmony_rhythm_cell_strength(size_t cell_in_bar);

/* A constraint of this kind on the span, else on its owner. */
bool harmony_find_constraint(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanConstraintKind kind,
                             PlanConstraint *out);
bool harmony_span_has_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind);
/* The mode of the key lane over the span, else of a key constraint on the span or its owner. */
bool harmony_find_mode(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKeyMode *out);
/* The key of the key lane over the span, else of a key constraint on the span or its owner. */
bool harmony_find_key(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKey *out);
/* The meter under span, which must lie in one meter lane span, start on a barline and be whole bars long. */
bool harmony_read_metre(const PiecePlan *plan, PlanSpan span, PhraseMetre *out);
/* The span of cells [first_cell, first_cell + cells) counted from start. */
PlanResult harmony_cells_span(PlanTime start, PlanTime cell_length, size_t first_cell, size_t cells, PlanSpan *out);

/* Fills out from the plan. False when the span is not one this operator applies to. */
bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);
/* The same for the prolongation operator: a PROLONGATION shape, whole bars, a key, and no cadence asked for. */
bool harmony_prolong_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);

typedef bool (*ReadRequestFn)(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);
/* Writes a progression candidate over its target span, one chord span each; the first keeps the target's id. */
PlanResult harmony_progression_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate,
                                     ReadRequestFn read_request);

/*
 * Whether chords of the given lengths in cells, from cell 0, keep to the shape: none over max_cells_per_chord
 * (when set; the last chord is exempt when exempt_last) and, with split_at_bars, none across a barline.
 */
bool harmony_shape_keeps(const PlanHarmonyShape *shape, const size_t *cells, size_t count, size_t cells_per_bar,
                         bool exempt_last);

/* The span of cells [first_cell, first_cell + cells) of the request. */
PlanResult harmony_request_cells_span(const PhraseRequest *request, size_t first_cell, size_t cells, PlanSpan *out);

/* The cell grid under a span: the meter lane span containing it, and where the span sits in that grid. */
typedef struct {
    PlanMeter meter;
    PlanTime bar_length;
    size_t cells_per_bar;
    PlanTime cell_length;
    size_t first_cell; /* cells from the start of the meter lane span to the start of the span */
    size_t cells;      /* the span's length in cells */
} CellMetre;

/* False unless the span lies in one meter lane span, starts on a cell and is a whole number of cells. */
bool harmony_read_cells(const PiecePlan *plan, PlanSpan span, CellMetre *out);
/* As harmony_count_cells, but a length of zero is zero cells. */
bool harmony_count_cells_or_zero(PlanTime cell_length, PlanTime length, size_t *cells);

/* How many cells of cell_length make length exactly. False if it is not a whole number. */
bool harmony_count_cells(PlanTime cell_length, PlanTime length, size_t *cells);

/* Reads the chord of a harmony span: a diatonic numeral, else an applied one, and its length in cells. */
bool harmony_read_chord(const PiecePlan *plan, LaneSpanId id, PlanKeyMode mode, PlanTime cell_length, PhraseChord *out);

/*
 * Reads the chords of the phrase around a marked chord span (see
 * harmony_embellish.h): the run of harmony spans of the same owner that touch
 * end to start through the target. False when the target is not marked, the run
 * holds a chord it cannot read or an open span that is not marked, or the phrase
 * has no meter, key or cadence.
 */
bool harmony_context_read(const PiecePlan *plan, LaneSpanId target, PhraseContext *out);

/* The segments the recognizer reads, one per chord of a phrase. */
typedef struct SegmentSet SegmentSet;
SegmentSet *harmony_segments_create(const HeardPhrase *phrase);
const HarmonySegment *harmony_segments_get(const SegmentSet *set, size_t *count);
void harmony_segments_destroy(SegmentSet *set);

/* The verdict of standard-harmony on a phrase. hard is NULL when no hard rule is broken, else says which. */
typedef struct {
    const char *hard;
    unsigned soft_held;
    unsigned soft_total;
    CadenceType heard;
} PhraseVerdict;

/*
 * Judges a phrase: the phrase model, the explanation of every chord that is not a plain
 * root-position one (a classified 6/4, the inside of a prolongation, a strong predominant, a
 * predominant seventh, V7), the applied chords resolving to their targets, the cadence's own
 * progression, and the cadence the recognizer hears, which must be the one requested and must
 * end the phrase. False only when it fails itself (allocation).
 */
bool harmony_judge(const HeardPhrase *phrase, PhraseVerdict *verdict);

/*
 * One change to the marked chord. The chords of lead take the last steal_cells cells of the chord
 * before the marked one, and the chords of pieces take the marked chord's place, their cells
 * adding up to its cells.
 */
#define EDIT_MAX_LEAD 2
#define EDIT_MAX_PIECES 4
typedef struct {
    size_t steal_cells;
    PhraseChord lead[EDIT_MAX_LEAD];
    size_t lead_count;
    PhraseChord pieces[EDIT_MAX_PIECES];
    size_t piece_count;
} Edit;

/* What an embellishment candidate does to the marked chord. */
typedef struct {
    bool plain; /* leave the progression as it is; the rest is unused */
    Edit edit;
    size_t target_cells; /* the marked chord's cells when the candidate was made */
    PlanKeyMode mode;
    PlanCadenceType cadence;
    size_t cells_per_bar;
    PlanTime cell_length;
    PhraseChord chords[HARMONY_MAX_CHORDS]; /* the whole phrase after the edit */
    size_t count;
} EmbellishCandidate;

/* The options a family offers for the marked chord. */
#define OPTIONS_MAX 48
typedef struct {
    Edit items[OPTIONS_MAX];
    size_t count;
} OptionList;

/* Adds an option, unless the list is full or the edit would not fit in HARMONY_MAX_CHORDS chords. */
void harmony_options_add(OptionList *list, const PhraseContext *phrase, const Edit *edit);
/* Up to two distinct values from low to high, chosen with rng; returns how many. */
size_t harmony_options_pick(FillRng *rng, size_t low, size_t high, size_t out[2]);

/* The options of each family. They judge nothing; the checker does. */
void harmony_options_prolongation(const PiecePlan *plan, const PhraseContext *phrase, FillRng *rng, OptionList *list);
void harmony_options_predominant(const PhraseContext *phrase, FillRng *rng, OptionList *list);
void harmony_options_applied(const PiecePlan *plan, const PhraseContext *phrase, OptionList *list);

/* Builds one diatonic chord for the progression. False when the chord is not valid in the mode. */
bool harmony_make_chord(PlanKeyMode mode, int degree, int inversion, bool seventh, bool cadential, size_t cells,
                        PhraseChord *out);

/* The leading-tone triad (the chord on the raised seventh degree in minor). */
bool harmony_make_leading_tone(PlanKeyMode mode, int inversion, size_t cells, PhraseChord *out);

/* The lane payload that writes a chord. */
void harmony_chord_payload(const PhraseChord *chord, PlanLanePayload *payload);

/* Moves the cadence constraint from one chord span to another. */
PlanResult harmony_move_cadence(PiecePlan *plan, LaneSpanId from, LaneSpanId to);

/* The phrase as the edit would leave it, in out->chords, ready for the checker. False if it does not fit. */
bool harmony_edit_compose(const PhraseContext *phrase, const Edit *edit, EmbellishCandidate *out);

/*
 * Proposes progressions for the request into out, using only rng for choices.
 * True on success, including with no candidates when the phrase is too short.
 */
bool harmony_phrase_propose(const PhraseRequest *request, FillRng *rng, FillCandidateList *out);

void harmony_progression_destroy(void *payload);

/* The fewest cells a phrase can have and still end in the cadence (the phrase-model operator's minimum). */
size_t harmony_phrase_min_cells(PlanCadenceType cadence, size_t cells_per_bar);

/* The verdict of the harmony-from-bass operator's checker on a candidate payload. */
bool harmony_octave_check(const void *payload, FillCheck *out);

/* The harmony under a bass span, as the bass operator reads it. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    PhraseMetre metre;
    size_t count;
    PhraseChord chords[HARMONY_MAX_CHORDS];
} BassContext;

/* One note of a bass candidate: its MIDI pitch, its length in cells, and the chord it sounds under. */
#define BASS_MAX_NOTES 256
typedef struct {
    int pitch;
    size_t cells;
    size_t chord;
} BassNote;

/* A bass candidate's payload: the harmony it was made for, the range it keeps to, and the notes. */
typedef struct {
    BassContext context;
    HarmonyBassParams params;
    BassNote notes[BASS_MAX_NOTES];
    size_t note_count;
} BassCandidate;

/*
 * Reads the harmony under a Constrained bass span: the chord spans must tile the span exactly, none
 * of them open, with a meter and a key over it. False when the span is not one the bass operator
 * applies to.
 */
bool harmony_bass_context_read(const PiecePlan *plan, LaneSpanId target, BassContext *out);

/*
 * Judges a bass line: every note in range and the bass member of its chord, then standard-counterpoint's
 * melodic rules over the whole line. Hard rules reject (all but the one for the last note of a line,
 * which a cadence outline breaks); soft rules score. False only when it fails itself (allocation).
 */
bool harmony_bass_judge(const BassCandidate *line, PhraseVerdict *verdict);
/* The checker's verdict on a bass candidate's payload. */
bool harmony_bass_check(const void *payload, FillCheck *out);

/* The most notes one chord's bass span holds. */
#define BASS_MAX_NOTES_PER_CHORD 4

/*
 * Builds one bass line for the harmony in context (its key, metre and chords), choosing octaves in the range
 * with only rng, and splitting chords by the parameters. False when a chord has no pitch in range.
 */
bool harmony_bass_build(const BassContext *context, const HarmonyBassParams *params, FillRng *rng,
                        BassCandidate *out);

/* The notes of one chord of a bass line, as lane notes with offsets from the chord's start. */
PlanResult harmony_bass_chord_notes(const BassCandidate *line, size_t chord, PlanNote *notes, size_t *count);

/* The scale degree of the chord's bass member: its root, or its third, fifth or seventh by inversion. */
int harmony_member_degree(PlanKeyMode mode, const PhraseChord *chord);

/* A MIDI pitch spelled as a scale degree of the key (see harmony_bass.h); NULL when it cannot be. */
PitchSpelling *harmony_spell_pitch(const PlanKey *key, int degree, int pitch);
/* The key's tonic, spelled with the fewest accidentals, in the octave given. */
PitchSpelling *harmony_spell_tonic(const PlanKey *key, int octave);

/* The pitch class (0 to 11) of the chord's bass member in the key. False for a chord not written here. */
bool harmony_chord_bass_pc(const PlanKey *key, const PhraseChord *chord, int *pc);


/*
 * Skeletons: the schema and sequence operators write harmony and bass together for the start of a
 * harmony target span, and leave the rest of the span open for the loop. This is the span as they read it.
 */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    CellMetre metre;
    bool at_start;      /* the span starts the owner node */
    bool has_cadence;   /* the span, or its owner, asks for a cadence */
    PlanCadenceType cadence;
    size_t before_count; /* the written chords that lead up to the span, from the start of the owner node */
    PhraseChord before[HARMONY_MAX_CHORDS];
    LaneSpanId bass_placeholder; /* the Constrained bass span equal to the target, or PLAN_LANE_SPAN_NONE for none */
} SkeletonContext;

/* A skeleton candidate: the chords and bass it writes at the start of the span. */
typedef struct {
    int schema;           /* a GalantSchemaId, or -1 when the skeleton is not a schema */
    SkeletonContext context;
    size_t count;         /* the skeleton's chords */
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t remainder_cells; /* what is left of the span after the skeleton */
    unsigned preference;  /* added to the score, in thousandths */
    BassCandidate bass;
} SkeletonCandidate;

/* Reads a Constrained harmony target for a skeleton; false when it is not one (see harmony_schema.h). */
bool harmony_skeleton_read(const PiecePlan *plan, LaneSpanId target, SkeletonContext *out);

/*
 * Judges the skeleton in its phrase: the chords before it and its own, the schema's chords needing no
 * further reason to be there, standard-harmony's phrase model, and, when the skeleton ends the span
 * and a cadence is asked for, the cadence heard; then the bass by the melodic rules. A skeleton that
 * leaves the span open must leave room for the cadence. False only when it fails itself.
 */
bool harmony_skeleton_judge(const SkeletonCandidate *skeleton, PhraseVerdict *verdict);
/* The checker's verdict on a skeleton candidate: the judgment, and the preference added to its score. */
bool harmony_skeleton_check(const void *payload, FillCheck *out);

/* Builds the bass line for the skeleton's chords, with the parameters. False when no bass fits. */
bool harmony_skeleton_bass(SkeletonCandidate *skeleton, const HarmonyBassParams *params, FillRng *rng);

/*
 * Writes the skeleton: the chords over the start of the span (the span itself becomes the first), the bass
 * beside them, a schema constraint on each chord of a schema, and a new Constrained span for what is left,
 * carrying the cadence.
 */
PlanResult harmony_skeleton_apply(PiecePlan *plan, const SkeletonCandidate *skeleton);

#endif
