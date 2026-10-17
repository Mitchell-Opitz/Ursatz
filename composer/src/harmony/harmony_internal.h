#ifndef URSATZ_COMPOSER_HARMONY_INTERNAL_H
#define URSATZ_COMPOSER_HARMONY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis_cadence.h>
#include <analysis_input.h>
#include <harmony_chord.h>
#include <harmony_pivot.h>
#include <pitch/pitch_spelling.h>
#include <semantics/beat_strength.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>

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

/* The most cells of a target given material is read over, and the most Given chords read on each side of it. */
#define HARMONY_GIVEN_CELLS 128
#define HARMONY_JOIN_CHORDS 8

/*
 * The meter under a phrase: its bars and the places in a bar where a chord may start, and the cells of the span.
 *
 * The cells are the places in a bar where a chord may start, anchored to the barlines of the meter span the phrase
 * lies in. A span that starts or ends between two of them has a part cell at that end: `lead` from the span's start
 * to the first cell boundary, and `tail` from the last cell boundary to the span's end (zero when the span is on a
 * boundary). Each part cell is a cell of its own, so the span's cells are the lead (when there is one), whole cells,
 * and the tail (when there is one), and `cells` counts them all. A span of bars has neither, and its cells are
 * `bars * cells_per_bar` of `cell_length`. `offset` is where the span starts in its bar.
 */
typedef struct {
    PlanMeter meter;
    PlanTime bar_length;
    size_t bars; /* the bars the span touches */
    size_t cells_per_bar;
    PlanTime cell_length;
    size_t cells; /* the span's cells, part cells included */
    PlanTime start;
    PlanTime lead;
    PlanTime tail;
    PlanTime offset;
} PhraseMetre;

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
    size_t cells;         /* the cells of the span, part cells included (see PhraseMetre) */
    PlanTime lead;        /* from the span's start to the first cell boundary; zero when it starts on one */
    PlanTime tail;        /* from the last cell boundary to the span's end; zero when it ends on one */
    PhraseMetre metre;    /* the cells, as the functions on PhraseMetre read them */
    bool has_shape;       /* a harmony shape constraint (PHRASE_MODEL, CADENTIAL or PROLONGATION) is on the span or its owner */
    PlanHarmonyShape shape;
    /* The span opens a new key (shape.starts_on is PIVOT): the key before it, and the degrees in the new key that a
       pivot chord can be, best first. The key lane over the span is the new key. */
    bool pivot;
    PlanKey from_key;
    PlanKey key;
    size_t pivot_count;
    int pivot_degrees[HARMONY_PIVOT_MAX];
    /*
     * Given material around the target (R-071), read only for an operator registered to work around it
     * (harmony_phrase_register_given): the pitch class of the Given bass note sounding at the start of each cell and of
     * the Given melody note that starts on each strong cell (-1 for none), and the Given chords that touch the target at
     * either end, as many as keep a whole number of bars (before) or are contiguous (after).
     */
    bool around_given;
    bool has_given;
    int bass_pc[HARMONY_GIVEN_CELLS];
    int melody_pc[HARMONY_GIVEN_CELLS];
    size_t before_count;
    PhraseChord before[HARMONY_JOIN_CHORDS];
    size_t after_count;
    PhraseChord after[HARMONY_JOIN_CHORDS];
    /* The target starts or ends where a phrase or a section of the plan does: a cadence heard there is a boundary's. */
    bool start_at_boundary;
    bool end_at_boundary;
    /* The target ends where the seam section of a piece structure starts and the first Given chord after it is the
       seam's (R-171): the candidates are also scored by how the last chord leads into it. */
    bool at_seam;
} PhraseRequest;

/* A candidate's payload. */
typedef struct {
    PhraseRequest request;
    PhraseChord *chords;
    size_t count;
    /* The prolongation operator: the span follows one of the same shape, and this candidate repeats its chords. */
    bool repeatable;
    bool repeats;
} PhraseProgression;

/*
 * A candidate whose target ends at the seam (request.at_seam) is also scored by the seam's progression rule (R-171): the
 * check's denominator grows by 1, and its numerator by 1 when the last chord leads into the seam's first. Nothing is
 * rejected for it. Any other candidate is left as it is.
 */
void harmony_seam_score(const PhraseProgression *progression, FillCheck *out);

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

/*
 * The Given chords before the target and its one chord, in a row (sequence has room for HARMONY_JOIN_CHORDS + 1), and the
 * metre they sit on, to judge a target too short for two chords. False when there are no chords before or they do not measure.
 */
bool harmony_joined_chain(const PhraseRequest *request, const PhraseChord *chords, PhraseChord *sequence,
                          PhraseMetre *metre);

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
    /*
     * Chords across key changes (R-069): the mode of the key each chord is in, and a number for that key (the
     * recognizer's key reference). NULL for a phrase in one key, which is `mode` for every chord.
     */
    const PlanKeyMode *chord_modes;
    const unsigned *chord_keys;
    /* Leaves out the root progression rule (R-164), for tests and comparisons. */
    bool plain_shaping;
    /*
     * The cells the chords sit on, when the phrase does not start on a barline or has part cells (see PhraseMetre): the
     * time, the bar and the strength of each chord are read from it, and cells_per_bar and cell_length are not used.
     * NULL for a phrase of whole bars from a barline, which is every cell of cell_length from cell 0.
     */
    const PhraseMetre *metre;
} HeardPhrase;

HarmonyMode harmony_mode_of(PlanKeyMode mode);

/* Reads the chords of the harmony lane over span, which must tile it exactly. False if it cannot be read. */
bool harmony_read_lane(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PhraseChord *chords, size_t *count,
                       PhraseMetre *metre);

/*
 * The same lane with each chord read in the key of the key lane under it (the mode given is for a chord under no key),
 * that key's mode in modes[i] and its number in keys[i]. A phrase in one key reads as harmony_read_lane does.
 */
bool harmony_read_lane_keys(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PhraseChord *chords, size_t *count,
                            PhraseMetre *metre, PlanKeyMode *modes, unsigned *keys);

/* What the recognizer hears at the end of the phrase, and whether it arrives on the last chord. */
bool harmony_hear_phrase(const HeardPhrase *phrase, CadenceType *heard, bool *at_end);

/*
 * Every cadence the recognizer hears over the harmony lane chords of span (read as harmony_read_lane does), the
 * soprano over chord i being soprano[i], a scale degree or 0 for none; count must be the number of chords. On true,
 * *out is the caller's (harmony_cadences_destroy), in order of arrival, its segment indices the chords'.
 *
 * section_starts (section_count of them, plan times inside span; NULL and 0 for none) are where the caller says a
 * section or phrase begins. Ursatz reads a V before such a start as a half cadence, with the authentic cadence into
 * the tonic as its competing reading; without one, a V to I6 is an evaded cadence (R-090, R-091).
 */
bool harmony_hear_lane_cadences(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, const int *soprano,
                                size_t count, const PlanTime *section_starts, size_t section_count,
                                HarmonyCadences *out);

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
/* The key of the key lane that ends where start is, when the span of the lane after it starts there. */
bool harmony_key_before(const PiecePlan *plan, PlanTime start, PlanKey *out);
/*
 * The degrees in the key `to` of the chords a pivot from the key `from` can be: triads of both (harmony_pivot_find), the
 * best of harmony_pivot_rank first, with the chords a phrase cannot open on left out (a dominant in either key, a
 * diminished triad, and in minor the minor v). 0 when the keys are not closely related.
 */
size_t harmony_pivot_degrees(PlanKey from, PlanKey to, int degrees[HARMONY_PIVOT_MAX]);
/* Whether the chord is a pivot chord from the key `from` into the key `to`: a root-position triad both keys have. */
bool harmony_pivot_chord_valid(PlanKey from, PlanKey to, const HarmonyChord *chord);
/*
 * The meter under span, which must lie in one meter lane span and start and end on a beat of it (a beat is
 * 1/beat_unit of a whole note). A span of whole bars has no part cells (see PhraseMetre).
 */
bool harmony_read_metre(const PiecePlan *plan, PlanSpan span, PhraseMetre *out);

/* Where cell `cell` starts, counted from the span's start; cell == cells is the span's end. */
bool harmony_metre_cell_offset(const PhraseMetre *metre, size_t cell, PlanTime *out);
/*
 * Where cells [first, first + cells) lie in the span, counted from its start: `start` and `length`. The harmony, melody
 * and texture operators read the same anchored grid, part cells included, through it.
 */
bool harmony_cells_of_span(const PhraseMetre *metre, size_t first, size_t cells, PlanTime *start, PlanTime *length);
/* The span of cells [first, first + cells). */
PlanResult harmony_metre_cells_span(const PhraseMetre *metre, size_t first, size_t cells, PlanSpan *out);
/* The bar (counted from the span's first bar, 0 for it) and the place in it where cell `cell` starts. */
bool harmony_metre_cell_position(const PhraseMetre *metre, size_t cell, size_t *bar, PlanTime *in_bar);
/* Whether the cell starts on a barline, and whether it is a part cell (the lead or the tail). */
bool harmony_metre_cell_on_downbeat(const PhraseMetre *metre, size_t cell);
bool harmony_metre_cell_is_part(const PhraseMetre *metre, size_t cell);
/* A cell on a downbeat is strong; any other whole cell is medium, and a part cell not on a downbeat is weak. */
BeatStrength harmony_metre_cell_strength(const PhraseMetre *metre, size_t cell);
/* The first cell and the number of cells that span covers exactly (inside the metre's span). False if it covers none. */
bool harmony_metre_find_cells(const PhraseMetre *metre, PlanSpan span, size_t *first, size_t *count);
/*
 * The cell the last chord of a phrase over the metre starts on: the last downbeat the span holds, else the first cell
 * boundary after the lead. For a span of whole bars it is the first cell of the last bar.
 */
size_t harmony_metre_last_cell(const PhraseMetre *metre);
/* The span of cells [first_cell, first_cell + cells) counted from start. */
PlanResult harmony_cells_span(PlanTime start, PlanTime cell_length, size_t first_cell, size_t cells, PlanSpan *out);

/* Fills out from the plan. False when the span is not one this operator applies to. */
bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);
/* The same for the prolongation operator: a PROLONGATION shape, whole bars, a key, and no cadence asked for. */
bool harmony_prolong_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);
/*
 * The same two for an operator registered to work around Given material (harmony_phrase_register_given): a Given bass
 * over the span does not refuse the span, it is read into the request with the Given melody and the Given chords at the
 * ends (harmony_given_read).
 */
bool harmony_phrase_read_request_given(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);
bool harmony_prolong_read_request_given(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);

/* Reads into the request what the Given bass, the Given melody and the Given chords around the target ask of it. */
void harmony_given_read(const PiecePlan *plan, PhraseRequest *request);
/* Whether chords keep to the given bass (at each chord's start) and the given melody (its notes on strong beats). */
bool harmony_given_fits(const PhraseRequest *request, const PhraseChord *chords, size_t count, const char **why);
/*
 * Whether the chords, heard with the Given chords before and after them, make no cadence the plan does not ask for:
 * one that arrives inside the target and is not the target's own at its end (cadence_planned), or that the target's
 * last chord leads into the Given chords. Where a phrase or a section starts or ends is a boundary: a half cadence's V
 * followed by the tonic there is no cadence. False only when the recognizer fails; *why is NULL when nothing is wrong.
 */
bool harmony_given_join(const PhraseRequest *request, const PhraseChord *chords, size_t count, bool cadence_planned,
                        const char **why);

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
/* The same over the cells of a metre, which may start between barlines and have part cells at its ends. */
bool harmony_shape_keeps_in(const PlanHarmonyShape *shape, const size_t *cells, size_t count, const PhraseMetre *metre,
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
/* The same on the cells of a metre: the span must start and end on a cell boundary of it. */
bool harmony_read_chord_in(const PiecePlan *plan, LaneSpanId id, PlanKeyMode mode, const PhraseMetre *metre,
                           PhraseChord *out);

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

/* Whether the chords' root changes are mostly strong (R-164): at least 2/3, or fewer than two changes. */
bool harmony_root_progression_holds(const HarmonyChord *chords, size_t count);

/* The verdict of standard-harmony on a phrase. hard is NULL when no hard rule is broken, else says which. */
typedef struct {
    const char *hard;
    unsigned soft_held;
    unsigned soft_total;
    CadenceType heard;
    unsigned shaping_broken; /* SHAPING_ bits of the shaping rules that did not hold (see check/shaping.h) */
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
    PhraseMetre metre; /* the cells the phrase sits on */
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

/*
 * The cells a phrase needs before its last bar, plus that bar, to end in the cadence: the room a phrase of whole bars
 * (or what is left of a span after a skeleton) must have.
 */
size_t harmony_phrase_room_cells(PlanCadenceType cadence, size_t cells_per_bar);

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
