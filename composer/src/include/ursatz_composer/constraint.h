#ifndef URSATZ_COMPOSER_CONSTRAINT_H
#define URSATZ_COMPOSER_CONSTRAINT_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis/motive_relation.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Constraints: requirements attached to a node or a lane span ("8 bars",
 * "ends with a PAC", "uses motive m1 inverted", "embellish this chord"). They are data only; nothing
 * here evaluates or checks them. A node or span keeps its constraints in the
 * order they were added. Adding or removing one on a Given node or span is
 * refused with PLAN_ERR_GIVEN_LOCKED.
 */

typedef enum {
    PLAN_CONSTRAINT_LENGTH_BARS,
    PLAN_CONSTRAINT_CADENCE,
    PLAN_CONSTRAINT_MOTIVE_REF,
    PLAN_CONSTRAINT_KEY,
    PLAN_CONSTRAINT_ARCHETYPE,
    PLAN_CONSTRAINT_EMBELLISH, /* no parameters: a harmony span reopened for the embellishment pass */
    PLAN_CONSTRAINT_SCHEMA,    /* the galant schema a span was made from, and which stage of it */
    PLAN_CONSTRAINT_MATERIAL,  /* no parameters: on a section node, its subtree is source material, not part of the piece */
    PLAN_CONSTRAINT_HARMONY_SHAPE, /* the shape a harmony span is to take (R-050) */
    PLAN_CONSTRAINT_THEME_PAIRING, /* how a theme begins and how it ends */
    PLAN_CONSTRAINT_ACCOMPANIMENT, /* the accompaniment an inner voice span is to be written as */
    PLAN_CONSTRAINT_VOICING,       /* the four-part voicing an inner voice span is to be written as */
    PLAN_CONSTRAINT_COUNTERPOINT,  /* the counter-melody an inner voice span is to be written as */
    PLAN_CONSTRAINT_RETURN,        /* on a section node: the earlier section it returns, and how it varies it */
    PLAN_CONSTRAINT_LAYERS,        /* on a section node: which lanes sound in it */
    PLAN_CONSTRAINT_SECTION_LETTER, /* on a section node: its form letter ("A", "B", "A'") */
    PLAN_CONSTRAINT_INVENT_MOTIVE,  /* no parameters: a motive's melody span to be invented (motive-invent) */
    PLAN_CONSTRAINT_STYLE,          /* on the form node: the style model file the fill prefers, and how strongly (R-080) */
    PLAN_CONSTRAINT_SOURCE,         /* on a motive node, or a section holding a given progression: the corpus passage it was taken from */
    PLAN_CONSTRAINT_PICKUP,         /* on a melody span: its first `length` is an anacrusis before the first downbeat note (R-168) */
    PLAN_CONSTRAINT_PIECE_PART,     /* on a section node: the part of a piece structure it is and its id (R-169) */
    PLAN_CONSTRAINT_FREE_PHRASE,    /* no parameters: on a section node, a phrase of any length in beats (R-170), not a theme */
    PLAN_CONSTRAINT_HOLD,           /* no parameters: on a section node, the stages from structure to texture leave it alone; the seam stage fills it (R-171) */
    PLAN_CONSTRAINT_SEAM,           /* no parameters: on a section node, the bar after the loop's end, a Given copy of the body's first bar (R-171) */
    PLAN_CONSTRAINT_MOTIVE_TONES    /* on a motive node: its base harmony and the tone role of each note (R-215) */
} PlanConstraintKind;

typedef enum {
    PLAN_ARCHETYPE_SENTENCE,
    PLAN_ARCHETYPE_PERIOD,
    PLAN_ARCHETYPE_HYBRID
} PlanArchetype;

/*
 * motive is a motive node in the same plan. The constraint says how the node's motive is made from it: by the
 * library relation step (analysis/motive_relation.h), or, with has_step false, not at all (the node's melody is the
 * source motive itself, as the first idea of a theme is). Parameters of the step left at their default are open: the
 * motivic development operators (motive_develop.h) choose them, so an all-default step asks the operator to choose.
 * An open parameter is a transpose with by 0, an invert with axis -1, an interval-change with amount 0, a fragment
 * with position 0 or length 0, a liquidate with drop 0, an augment or diminish with ratio 0:0 (2:1 and 1:2).
 */
#define PLAN_SEGMENT_LABEL_MAX 8 /* the longest label of a motive's segment (MOTIVE_FEATURE_LABEL_MAX) */

typedef struct {
    NodeId motive;
    bool has_step;
    MotiveRelationStep step;
    /* The label of the segment of the basic motive the motive node is made from (ch. VIII); empty for none. */
    char segment[PLAN_SEGMENT_LABEL_MAX + 1];
    /*
     * The liquidation steps (ch. VIII) the motive node holds is made by, each a liquidate-features step with steps 1 and
     * shorten 1; 0 for none. A record: grow-theme makes the liquidated motive itself.
     */
    size_t steps;
} PlanMotiveRef;

/* Whether the ref restates its source exactly (an exact step; a transposition of it may be drawn, see motive_develop.h). */
static inline bool plan_motive_ref_restates(const PlanMotiveRef *ref) {
    return ref->has_step && ref->step.kind == MOTIVE_RELATION_EXACT;
}

/*
 * The schema a span was made from: schema is the id of a galant-schemas prototype (GalantSchemaId in
 * Ursatz's frameworks/galant-schemas), stage the index of its stage the span sounds. The prototype's
 * other voices (the melody) are read from the framework by that id, not copied here.
 */
typedef struct {
    int schema;
    int stage;
} PlanSchemaRef;

typedef enum {
    PLAN_HARMONY_SHAPE_PROLONGATION, /* one harmony prolonged across the span, no cadence */
    PLAN_HARMONY_SHAPE_PHRASE_MODEL, /* the phrase model, optionally with a faster harmonic rhythm */
    PLAN_HARMONY_SHAPE_CADENTIAL     /* the cadential progression over the whole span */
} PlanHarmonyShapeKind;

/*
 * Where a harmony span starts or ends. PIVOT is for starts_on of a phrase-model span (R-069) that opens a new key: the
 * key lane changes where the span starts, and its first chord is a pivot chord, one both keys have as a triad
 * (standard-harmony's harmony_pivot_find), written as its numeral in the new key. It is not an end anchor.
 */
typedef enum {
    PLAN_HARMONY_ANCHOR_ANY,
    PLAN_HARMONY_ANCHOR_TONIC,
    PLAN_HARMONY_ANCHOR_DOMINANT,
    PLAN_HARMONY_ANCHOR_PIVOT
} PlanHarmonyAnchor;

/* max_cells_per_chord 0 means no limit; split_at_bars lets no chord cross a barline. */
typedef struct {
    PlanHarmonyShapeKind kind;
    PlanHarmonyAnchor starts_on;
    PlanHarmonyAnchor ends_on;
    int max_cells_per_chord;
    bool split_at_bars;
} PlanHarmonyShape;

typedef enum {
    PLAN_THEME_BEGINNING_PRESENTATION,
    PLAN_THEME_BEGINNING_ANTECEDENT,
    PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA
} PlanThemeBeginning;

typedef enum {
    PLAN_THEME_ENDING_CONTINUATION,
    PLAN_THEME_ENDING_CONSEQUENT,
    PLAN_THEME_ENDING_CADENTIAL
} PlanThemeEnding;

/* The nine pairings of a beginning and an ending; the standard-form theme types are made from them. */
typedef struct {
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
} PlanThemePairing;

/*
 * An accompaniment under a written melody and bass, in the inner voice lanes (R-065). The pattern is Composer
 * vocabulary; ANY leaves it to the operator, which a texture target narrows. texture is a StandardTexture value
 * (standard-texture's texture_rules.h) the written parts are to be heard as, or -1 for none. low and high are the
 * MIDI pitches the accompaniment keeps to, 0 for the defaults 48 and 67. The pattern MOTIVE restates the motive named
 * by motive_node (the pattern needs the name, and no other pattern takes one).
 */
typedef enum {
    PLAN_ACCOMPANIMENT_ANY,
    PLAN_ACCOMPANIMENT_BLOCK,
    PLAN_ACCOMPANIMENT_ARPEGGIATED,
    PLAN_ACCOMPANIMENT_ALBERTI,
    PLAN_ACCOMPANIMENT_OOM_PAH,
    PLAN_ACCOMPANIMENT_MOTIVE /* a saved accompaniment motive, restated on each chord; ANY never picks it */
} PlanAccompanimentPattern;

#define PLAN_ACCOMPANIMENT_DEFAULT_LOW 48
#define PLAN_ACCOMPANIMENT_DEFAULT_HIGH 67
#define PLAN_ACCOMPANIMENT_NODE_MAX 32

typedef struct {
    PlanAccompanimentPattern pattern;
    int texture;
    int low;
    int high;
    /* The name of the material node holding the accompaniment motive (a Given melody span), "" for none. */
    char motive_node[PLAN_ACCOMPANIMENT_NODE_MAX];
} PlanAccompaniment;

/*
 * A four-part voicing of a written progression (R-066). It is on a Constrained inner voice span, voice 1 (the alto);
 * the tenor is inner voice 2. With write_soprano the melody span over the same time is Constrained too and the
 * voicing writes it, one soprano note per chord; without it the soprano is the written melody note sounding at the
 * start of each chord.
 */
typedef struct {
    bool write_soprano;
} PlanVoicing;

/*
 * A counter-melody against a given line (R-067). It is on a Constrained inner voice span, voice 1. species is 1 to 4.
 * With above false the given line is the melody lane and the counter-melody sounds below it; with above true the
 * given line is the bass lane (the cantus) and the counter-melody sounds above it.
 */
typedef struct {
    int species;
    bool above;
} PlanCounterpoint;

/*
 * A returning section (R-068, R-070): section is the section node it returns, variation how the return differs
 * from it. The constraint must name a section node.
 */
typedef enum {
    PLAN_RETURN_EXACT,
    PLAN_RETURN_ORNAMENT,
    PLAN_RETURN_REHARMONIZE,
    PLAN_RETURN_REREGISTER
} PlanReturnVariation;

typedef struct {
    NodeId section;
    PlanReturnVariation variation;
} PlanReturn;

/*
 * The lanes that sound in a layer section: bit (1u << lane) for each PlanLaneKind. A layer section is grown in
 * full; the mask decides only what is rendered. At least one bit, none past PLAN_LANE_INNER_VOICE.
 */
#define PLAN_LAYER_BIT(lane) (1u << (unsigned)(lane))
#define PLAN_LAYERS_ALL (PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE + 1) - 1u)

typedef struct {
    unsigned lanes;
} PlanLayers;

/* A section's form letter: one to three characters, NUL-terminated ("A", "B", "A'", "V1"). */
#define PLAN_SECTION_LETTER_MAX 4

/*
 * The style a plan is filled in (style.h): the path of a StyleModel file as the command line was given it, and the
 * weight of the preference as a fraction. A weight of 0/0 is the default, 1/1. The file is read when the plan is filled,
 * not when the plan is read, so a plan can name a style that is not there yet.
 *
 * A blend (style.h, composer_style_blend) names a second file, blend_path ("" for none), and the weight of that file
 * in each dimension (StyleDimension order: harmony, rhythm, melody, form) as fractions from 0 to 1; with a blend_path
 * every blend denominator is at least 1.
 */
#define PLAN_STYLE_PATH_MAX 512

/*
 * The categories the style is read without (composer_style_load_without, the command line's --style-without), up to
 * PLAN_STYLE_WITHOUT_MAX names of fewer than PLAN_STYLE_WITHOUT_NAME_MAX characters each. A plan keeps the list so a
 * fill reads the style the way the plan was made.
 */
#define PLAN_STYLE_WITHOUT_MAX 8
#define PLAN_STYLE_WITHOUT_NAME_MAX 32

typedef struct {
    char path[PLAN_STYLE_PATH_MAX];
    unsigned long weight_numerator;
    unsigned long weight_denominator;
    char blend_path[PLAN_STYLE_PATH_MAX];
    unsigned long blend_numerator[4];
    unsigned long blend_denominator[4];
    char without[PLAN_STYLE_WITHOUT_MAX][PLAN_STYLE_WITHOUT_NAME_MAX];
    size_t without_count;
} PlanStyle;

/*
 * Where seeded material came from (corpus_seed.h): the piece's path in the corpus, the passage as the Analyzer reports
 * it (a measure and the beat plus the fraction of it, at both ends) and the similarity it scored, as a fraction from 0
 * to 1. A note for the reader of the plan; nothing in a fill reads it.
 */
#define PLAN_SOURCE_PIECE_MAX 512

typedef struct {
    char piece[PLAN_SOURCE_PIECE_MAX];
    int start_measure;
    double start_beat;
    int end_measure;
    double end_beat;
    unsigned long score_numerator;
    unsigned long score_denominator;
} PlanSource;

/*
 * The part of a game loop section (piece.h) a section is, and the id the request gave it. part is a PiecePart:
 * 0 intro, 1 body, 2 turnaround, 3 ending.
 */
#define PLAN_PIECE_ID_MAX 16
#define PLAN_PIECE_PART_COUNT 4

typedef struct {
    int part;
    char id[PLAN_PIECE_ID_MAX];
} PlanPiecePart;

/*
 * The tones of a motive (R-215), on its motive node beside the melody span's notes: the base harmony the notes were
 * labelled against, and one role label per note in note order. The melody operator places the motive by these labels.
 *
 * A chord is a Roman numeral as a motive's own harmony writes it ("I", "V65", "bVII7") with its onset from the start of
 * the motive and its length, both in whole notes. A label is "chord", "unclassified" or the name of a non-chord tone
 * ("passing", "neighbor", "appoggiatura", "suspension", "retardation", "escape", "anticipation", "double-neighbor",
 * "changing-tone", "pedal"); the plan holds the text, the melody operator reads it. The label count is the note count of
 * the node's melody span, which the plan reader checks.
 */
#define PLAN_TONES_MAX_CHORDS 8
#define PLAN_TONES_MAX_NOTES 64
#define PLAN_TONE_LABEL_MAX 16

typedef struct {
    char numeral[PLAN_ROMAN_NUMERAL_MAX];
    Rational onset;
    Rational length;
} PlanToneChord;

typedef struct {
    PlanToneChord chords[PLAN_TONES_MAX_CHORDS];
    size_t chord_count;
    char labels[PLAN_TONES_MAX_NOTES][PLAN_TONE_LABEL_MAX];
    size_t label_count;
} PlanMotiveTones;

/* The member of params that is used follows the kind. */
typedef struct {
    PlanConstraintKind kind;
    union {
        int bars;
        CadenceType cadence;
        PlanMotiveRef motive;
        AnalysisKey key;
        PlanArchetype archetype;
        PlanSchemaRef schema;
        PlanHarmonyShape harmony_shape;
        PlanThemePairing theme_pairing;
        PlanAccompaniment accompaniment;
        PlanVoicing voicing;
        PlanCounterpoint counterpoint;
        PlanReturn section_return;
        PlanLayers layers;
        char letter[PLAN_SECTION_LETTER_MAX];
        PlanStyle style;
        PlanSource source;
        Rational pickup; /* the anacrusis: a multiple of 1/32 of a whole note, shorter than a bar */
        PlanPiecePart piece_part;
        PlanMotiveTones motive_tones;
    } params;
} PlanConstraint;

/* Fails with PLAN_ERR_BAD_PAYLOAD for a constraint that is malformed or names a missing motive node. */
PlanResult plan_node_add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint);
size_t plan_node_constraint_count(const PiecePlan *plan, NodeId id);
/* Whether the node has a constraint of the kind. */
bool plan_node_has_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind);
/* The first constraint of the kind on the node, or NULL. The pointer is valid until the list changes. */
const PlanConstraint *plan_node_find_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind);
/* NULL for an unknown id or an index past the end. The pointer is valid until the list changes. */
const PlanConstraint *plan_node_constraint_at(const PiecePlan *plan, NodeId id, size_t index);
/* Later constraints shift down one. PLAN_ERR_BAD_INDEX for an index past the end. */
PlanResult plan_node_remove_constraint(PiecePlan *plan, NodeId id, size_t index);

PlanResult plan_lane_span_add_constraint(PiecePlan *plan, LaneSpanId id, const PlanConstraint *constraint);
size_t plan_lane_span_constraint_count(const PiecePlan *plan, LaneSpanId id);
const PlanConstraint *plan_lane_span_constraint_at(const PiecePlan *plan, LaneSpanId id, size_t index);
/* The first constraint of the kind on the lane span, or NULL. The pointer is valid until the list changes. */
const PlanConstraint *plan_lane_span_find_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind);
PlanResult plan_lane_span_remove_constraint(PiecePlan *plan, LaneSpanId id, size_t index);

#endif
