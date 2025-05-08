#ifndef URSATZ_COMPOSER_CONSTRAINT_H
#define URSATZ_COMPOSER_CONSTRAINT_H

#include <stdbool.h>
#include <stddef.h>

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
    PLAN_CONSTRAINT_STYLE           /* on the form node: the style model file the fill prefers, and how strongly (R-080) */
} PlanConstraintKind;

typedef enum {
    PLAN_CADENCE_PAC,
    PLAN_CADENCE_IAC,
    PLAN_CADENCE_HC,
    PLAN_CADENCE_DC,
    PLAN_CADENCE_PC
} PlanCadenceType;

typedef enum {
    PLAN_TRANSFORM_NONE,
    PLAN_TRANSFORM_TRANSPOSED,
    PLAN_TRANSFORM_INVERTED,
    PLAN_TRANSFORM_RETROGRADE,
    PLAN_TRANSFORM_AUGMENTED,
    PLAN_TRANSFORM_DIMINISHED,
    PLAN_TRANSFORM_FRAGMENTED,
    /* The transforms below were added after the first six; they keep the order of the plan format. */
    PLAN_TRANSFORM_RESTATED,       /* exact repetition, or a transposition of it */
    PLAN_TRANSFORM_INTERVAL_CHANGED,
    PLAN_TRANSFORM_LIQUIDATED
} PlanMotiveTransform;

/* Where a fragment is taken from. PLAN_FRAGMENT_ANY leaves it to the operator. */
typedef enum {
    PLAN_FRAGMENT_ANY,
    PLAN_FRAGMENT_LEADING,
    PLAN_FRAGMENT_TRAILING,
    PLAN_FRAGMENT_MIDDLE
} PlanFragmentPosition;

typedef enum {
    PLAN_ARCHETYPE_SENTENCE,
    PLAN_ARCHETYPE_PERIOD,
    PLAN_ARCHETYPE_HYBRID
} PlanArchetype;

/*
 * motive is a motive node in the same plan. The constraint says how the node's motive is to be made from it.
 *
 * semitones is used only by PLAN_TRANSFORM_TRANSPOSED (a real transposition). The members after it are optional
 * parameters for the motivic development operators (motive_develop.h); all-zero asks the operator to choose, so a
 * constraint written without them reads as it always did:
 *   tonal          transposed or inverted in the steps of the key's scale instead of in semitones
 *   steps          transposed, tonal: diatonic steps up (negative: down)
 *   has_axis, axis inverted: the MIDI pitch the motive is mirrored about (the first note by default)
 *   interval, amount, keep_contour
 *                  interval_changed: the 1-based interval to widen (negative amount: narrow) by amount semitones,
 *                  or every interval when interval is 0; keep_contour refuses a result that changes a direction
 *   position, length
 *                  fragmented: leading, trailing or middle, and how many notes
 *   drop, keep_rhythm
 *                  liquidated: how many notes to drop; keep_rhythm leaves the dropped notes' time as rests
 *   ratio_num, ratio_den
 *                  augmented or diminished: the factor the durations are multiplied by (2:1 and 1:2 by default)
 */
typedef struct {
    NodeId motive;
    PlanMotiveTransform transform;
    int semitones;
    bool tonal;
    int steps;
    bool has_axis;
    int axis;
    int interval;
    int amount;
    bool keep_contour;
    PlanFragmentPosition position;
    int length;
    int drop;
    bool keep_rhythm;
    int ratio_num;
    int ratio_den;
} PlanMotiveRef;

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
 * MIDI pitches the accompaniment keeps to, 0 for the defaults 48 and 67.
 */
typedef enum {
    PLAN_ACCOMPANIMENT_ANY,
    PLAN_ACCOMPANIMENT_BLOCK,
    PLAN_ACCOMPANIMENT_ARPEGGIATED,
    PLAN_ACCOMPANIMENT_ALBERTI,
    PLAN_ACCOMPANIMENT_OOM_PAH
} PlanAccompanimentPattern;

#define PLAN_ACCOMPANIMENT_DEFAULT_LOW 48
#define PLAN_ACCOMPANIMENT_DEFAULT_HIGH 67

typedef struct {
    PlanAccompanimentPattern pattern;
    int texture;
    int low;
    int high;
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

typedef struct {
    char path[PLAN_STYLE_PATH_MAX];
    unsigned long weight_numerator;
    unsigned long weight_denominator;
    char blend_path[PLAN_STYLE_PATH_MAX];
    unsigned long blend_numerator[4];
    unsigned long blend_denominator[4];
} PlanStyle;

/* The member of params that is used follows the kind. */
typedef struct {
    PlanConstraintKind kind;
    union {
        int bars;
        PlanCadenceType cadence;
        PlanMotiveRef motive;
        PlanKey key;
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
    } params;
} PlanConstraint;

/* Fails with PLAN_ERR_BAD_PAYLOAD for a constraint that is malformed or names a missing motive node. */
PlanResult plan_node_add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint);
size_t plan_node_constraint_count(const PiecePlan *plan, NodeId id);
/* NULL for an unknown id or an index past the end. The pointer is valid until the list changes. */
const PlanConstraint *plan_node_constraint_at(const PiecePlan *plan, NodeId id, size_t index);
/* Later constraints shift down one. PLAN_ERR_BAD_INDEX for an index past the end. */
PlanResult plan_node_remove_constraint(PiecePlan *plan, NodeId id, size_t index);

PlanResult plan_lane_span_add_constraint(PiecePlan *plan, LaneSpanId id, const PlanConstraint *constraint);
size_t plan_lane_span_constraint_count(const PiecePlan *plan, LaneSpanId id);
const PlanConstraint *plan_lane_span_constraint_at(const PiecePlan *plan, LaneSpanId id, size_t index);
PlanResult plan_lane_span_remove_constraint(PiecePlan *plan, LaneSpanId id, size_t index);

#endif
