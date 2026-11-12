#ifndef URSATZ_COMPOSER_GROW_INTERNAL_H
#define URSATZ_COMPOSER_GROW_INTERNAL_H

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/variant_registry.h>

#include "motive/motive_fidelity.h"

/* The bars the material section holds: room for the motive and every motive made from it. */
#define GROW_MATERIAL_BARS 8
#define GROW_THEME_NAME "theme"
#define GROW_MATERIAL_NAME "material"
#define GROW_SOURCE_NAME "m0"
#define GROW_ACCOMPANIMENT_NAME "acc"
/* The material nodes of the slot motives: "s1" contrasting, "s2" continuation, "s3" cadential (grow.h GrowSlot). */

/* The pitch of the note an open bass, melody or inner voice span holds until an operator writes it. */
#define BASS_PLACEHOLDER 40
#define MELODY_PLACEHOLDER 60
#define INNER_PLACEHOLDER 55
#define GROW_PAYLOAD_MAGIC 0x47525731UL
/* Fragments of the basic idea are this many notes at most; the first is the leading ones of the motive. */
#define GROW_FRAGMENT_MAX 3

/*
 * The motives the ideas of a theme are made from, by slot: slot k is the motive node m<k> in the material. The slot
 * is the idea's place, and what an idea is depends on the pairing:
 *   1  a presentation's repetition, or the contrasting idea of an antecedent or a compound basic idea
 *   2  a continuation's first fragment, a consequent's restated basic idea, or a cadential ending's first idea
 *   3  a continuation's second fragment, a consequent's contrasting idea, or a cadential ending's second idea
 *   4  a continuation's cadential idea
 */
typedef enum {
    GROW_PLACE_BASIC,
    GROW_PLACE_REPETITION,
    GROW_PLACE_FRAGMENT_1,
    GROW_PLACE_FRAGMENT_2,
    GROW_PLACE_CADENTIAL,
    GROW_PLACE_COUNT,
    GROW_PLACE_CONTRAST = GROW_PLACE_REPETITION,
    GROW_PLACE_RESTATED = GROW_PLACE_FRAGMENT_1,
    GROW_PLACE_CONSEQUENT_CONTRAST = GROW_PLACE_FRAGMENT_2,
    GROW_PLACE_CADENTIAL_1 = GROW_PLACE_FRAGMENT_1,
    GROW_PLACE_CADENTIAL_2 = GROW_PLACE_FRAGMENT_2
} GrowPlace;

/* How a presentation repeats its basic idea (ch. V). */
typedef enum { GROW_REPEAT_EXACT, GROW_REPEAT_DOMINANT_FORM, GROW_REPEAT_SEQUENCE, GROW_REPEAT_COUNT } GrowRepetition;

/*
 * One extension of a construction (R-468) as laid out. A repeat and an evaded cadence are an idea of their own: `place` is
 * the place of the idea a repeat follows, or the last place of the theme for an evaded cadence, and start and length are
 * the new idea's. A stretch is no idea: `place` is the place it lengthens, start that place's start and length the bars
 * added. An addition lies after the theme's last idea: place is that idea's, start and length are the addition's.
 */
typedef struct {
    PlanExtensionKind kind;
    GrowPlace place;
    Rational start;
    Rational length;
    bool varied;
} GrowLayoutExtension;

/*
 * Where each idea of a theme lies, as exact lengths from the theme's start, computed once per request (grow_layout_of).
 * A place the pairing does not have has length 0. The ending starts at `beginning`. A *_bars member is the whole bars the
 * length is, 0 when it is not whole. The halves hold their repeats, their stretched ideas and, in the ending, an evaded
 * cadence's idea; an addition is after the ending. `idea_beginning_bars` and `idea_ending_bars` are the halves of the
 * construction's ideas alone, without any extension.
 */
typedef struct {
    Rational start[GROW_PLACE_COUNT];
    Rational length[GROW_PLACE_COUNT];
    Rational beginning;      /* the length of the first half */
    Rational ending;         /* the length of the second half */
    size_t basic_bars;
    size_t beginning_bars;
    size_t ending_bars;
    size_t idea_beginning_bars;
    size_t idea_ending_bars;
    size_t main_ending_bars; /* the ending's bars up to the idea after an evaded cadence: `ending_bars` without it */
    size_t extension_count;
    GrowLayoutExtension extensions[PLAN_CONSTRUCTION_EXTENSIONS]; /* in the construction's order */
} GrowLayout;

/* What a candidate of grow-theme asks for: every choice, made from the run's seed. */
typedef struct {
    unsigned long magic;
    size_t bars;
    PlanMeter meter;
    AnalysisKey key;
    CadenceType cadence;
    PlanThemePairing pairing;
    GrowLayout layout;                 /* where each idea lies (grow_layout_of) */
    CadenceType first_cadence;         /* the weak cadence an antecedent ends in */
    PlanHarmonyShapeKind beginning_shape;  /* the shape of an antecedent's chords */
    bool beginning_split;                  /* no chord of the antecedent crosses a barline */
    PlanHarmonyAnchor compound_ends_on;    /* where a compound basic idea's prolongation ends */
    MotiveRelationStep contrast;                /* the contrasting idea of an antecedent or a compound basic idea */
    MotiveRelationStep consequent_contrast;     /* the contrasting idea of a consequent */
    size_t basic_cells;                    /* the cells of the basic idea, to size the harmonic rhythm after it */
    GrowRepetition repetition;             /* how the presentation repeats the basic idea */
    /*
     * The segments of the motive m0 (R-311): its features when the compose run's basic motive is it and has any, else
     * motive_segments_propose's. Labels as given; first and last are note indices of m0. Not written back to the motive.
     */
    MotiveSketchFeature segments[MOTIVE_MAX_FEATURES];
    size_t segment_count;
    size_t fragment_notes[2];              /* notes of the leading fragments the two fragment ideas are */
    /*
     * A continuation's fragments from the segments (ch. VIII): fragment k is the segment fragment_segment[k] of m0 as a
     * motive of its own (fragment_motive[k]) when fragment_from_segment[k]; else the leading fragment of m0 above.
     * fragment_sequence: fragment 2 is fragment 1's segment in sequence one step down (tonal), else the next segment.
     */
    bool fragment_from_segment[2];
    size_t fragment_segment[2];
    bool fragment_sequence;
    Motive fragment_motive[2];
    size_t fragment_cut[2];                /* the leading notes of the segment fragment k is cut to, 0 for the whole segment */
    bool fragment_diminished[2];           /* fragment k is its segment in diminution 1:2 */
    bool segment_room;                     /* the material has bars to put the motives grow-theme makes in */
    /*
     * The ideas that end the theme as liquidations by segments (ch. VIII): m0 as the motive-forms read it (liquid_base,
     * its segments its features), liquidated step by step until the result fits the idea and its last note can take the
     * cadence. liquidated[place] is the result, has_liquidated[place] says the idea is that motive restated, and
     * liquidation_steps[place] is the steps it took. liquidation_segment[place] is the segment of m0 (an index of
     * segments) whose rhythm and head interval the residue keeps (R-345). Every cadential idea without a motive of its
     * own has one: a draw with none fails, and no idea is liquidated by notes.
     */
    Motive liquid_base;
    bool has_liquid_base;
    Motive liquidated[GROW_PLACE_COUNT];
    bool has_liquidated[GROW_PLACE_COUNT];
    size_t liquidation_steps[GROW_PLACE_COUNT];
    size_t liquidation_segment[GROW_PLACE_COUNT];
    bool liquidation_head[GROW_PLACE_COUNT]; /* the residue is a head residue (R-350): a segment's head and a close */
    PlanHarmonyShapeKind ending_shape;
    bool ending_split;                     /* no chord of the ending crosses a barline */
    int ending_cap;                        /* max_cells_per_chord of the ending; 0 for none */
    Motive source;                         /* the motive the ideas are made from: m0, or the section's own */
    /*
     * A section with a MOTIVE_REF has a motive of its own, made from m0 as section_ref says (every parameter set, so
     * the motive operators make exactly it); source is then that motive. Without one source is m0.
     */
    bool has_section_motive;
    PlanMotiveRef section_ref;
    /*
     * The motive is as long as the first part (presentation or antecedent) and fills it whole: the part is one idea with
     * the motive as its melody, and source is the motive's first idea-length bars (made as the section's motive), the
     * basic idea the second half is made from.
     */
    bool part_whole;
    /* The motives of the slots that have their own (GROW_SLOT_BASIC is source); the ideas of the slot are made from it. */
    Motive slot_source[GROW_SLOT_COUNT];
    bool has_slot_source[GROW_SLOT_COUNT];
    /* The motives single ideas have of their own, by place (GROW_PLACE_BASIC has none); the idea is the motive exact. */
    Motive idea_source[GROW_PLACE_COUNT];
    bool has_idea_source[GROW_PLACE_COUNT];
    /* The motives the ideas after the basic idea are made from m0 as, planned here to be checked. */
    Motive developed[GROW_PLACE_COUNT];
    bool has_developed[GROW_PLACE_COUNT];
    /*
     * A contrasting idea shorter than the motive it is made from (a construction's, R-471) is made from the leading notes
     * of that motive that fit it: shortened[place], a made node of the material when has_shortened[place].
     */
    Motive shortened[GROW_PLACE_COUNT];
    bool has_shortened[GROW_PLACE_COUNT];
    /*
     * The motives of the extensions that have one of their own (R-468): extended[k] is what the repeat or the addition
     * layout.extensions[k] is, a made node of the material when has_extended[k]. A stretch and an evaded cadence make none.
     */
    Motive extended[PLAN_CONSTRUCTION_EXTENSIONS];
    bool has_extended[PLAN_CONSTRUCTION_EXTENSIONS];
    bool has_construction;                 /* the request's: the label names the halves' bars */
    GrowContinuation continuation;         /* the request's, for the build (grow_apply) */
    bool basic_as_written;                 /* the request's */
    bool basic_has_chords;                 /* the compose run's basic motive has harmony of its own (R-348) */
} GrowThemeSpec;

/* The theme section grow-theme grows, read from the plan. */
typedef struct {
    NodeId section;
    PlanSpan span;
    PlanMeter meter;
    AnalysisKey key;                   /* the key the section starts in */
    bool has_start_change;         /* the key before the section is another, which the section opens */
    bool pedal;                    /* the presentation is written over a dominant pedal (R-446); kept only in the home key */
    AnalysisKey from_key;              /* that key; read only with has_start_change */
    bool has_end_change;           /* the second half of the section is in another key, which it opens */
    AnalysisKey end_key;               /* that key; read only with has_end_change */
    CadenceType cadence;
    CadenceType first_cadence; /* meaningful when has_first */
    bool has_first;
    PlanThemePairing pairing;
    bool has_construction;   /* the section has a construction (R-468): its ideas are the bars it gives */
    PlanConstruction construction;
    NodeId source;
    size_t bars;
    size_t cells_per_bar;
    Rational bar;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_voicing;
    PlanVoicing voicing;
    bool has_motive_ref;     /* the section names the motive its basic idea is made from */
    PlanMotiveRef motive_ref; /* to m0, with a transform; parameters left open are drawn by grow-theme */
    NodeId slot_node[GROW_SLOT_COUNT]; /* the material node of each slot with a motive of its own, else PLAN_NODE_NONE */
    NodeId idea_node[GROW_PLACE_COUNT]; /* the material node of each place with a motive of its own (R-206), else none */
    GrowDevelopment development; /* from the GrowContext: how far the ideas stray from the motive */
    GrowContinuation continuation; /* from the GrowContext: how the continuation accelerates; zero for today's */
    bool basic_as_written;   /* from the GrowContext: a motif as long as the basic idea is it as written */
    GrowContrast contrast;   /* from the GrowContext: how the contrasting ideas contrast; zero for today's */
    const MotiveRandomPreferences *motif; /* from the GrowContext: the motif vocabulary a drawn rhythm uses; NULL for the defaults */
    bool given_first_half;   /* Given melody, harmony or bass lies in the first half: its chords are not written whole */
} GrowThemeRequest;

/* Where a build step adds to the plan: the theme, its material, and the node the parts of a phrase go in. */
typedef struct {
    PiecePlan *plan;
    const GrowThemeSpec *spec;
    const GrowThemeRequest *request;
    GrowLayout layout;       /* the spec's layout */
    PlanConstraint cadence;  /* the theme's cadence */
    NodeId phrase;           /* the phrase the parts go in, made by the beginning */
    NodeId material;         /* the idea of the material section the motive nodes go in */
    PlanSpan material_span;
    NodeId source;           /* the motive node the ideas are made from: m0, or the section's own motive */
    size_t motive_bars;      /* the bars a derived motive node takes in the material: those of the longest motive given */
    size_t slot_base;        /* the bar of the material motive slot 1 goes in; slot k goes (k - 1) * motive_bars after it */
    size_t result_base;      /* the bar of the material the motives grow-theme makes go in, one bar each */
    NodeId segment_node[2];  /* the material node of the segment fragment k is made from, else PLAN_NODE_NONE */
    NodeId liquid_node[GROW_PLACE_COUNT]; /* the material node holding an idea's liquidation, else PLAN_NODE_NONE */
    NodeId short_node[GROW_PLACE_COUNT];  /* the material node holding an idea's shortened source, else PLAN_NODE_NONE */
    NodeId extension_node[PLAN_CONSTRUCTION_EXTENSIONS]; /* the material node holding an extension's motive, else none */
} GrowBuild;

/* A length of count units. */
Rational grow_scaled(Rational unit, size_t count);
/* A node of the theme, Realized, starting `start` after the theme's start and `length` long. */
PlanResult grow_add_node(GrowBuild *build, NodeId parent, PlanNodeKind kind, Rational start, Rational length,
                         const char *name, NodeId *out);
/* The motive node m<slot> in the material, made from the ref, at bar slot_base + (slot - 1) * motive_bars of the material. */
PlanResult grow_add_motive(GrowBuild *build, size_t slot, const PlanMotiveRef *ref, NodeId *out);
/* One diatonic chord of the key (degree 1 to 7) written whole over the owner, with the bass and the rest open as for targets. */
PlanResult grow_add_chord_written(GrowBuild *build, NodeId owner, int degree);
/* A harmony span and the bass span beside it, both open, over an idea or a subphrase. */
PlanResult grow_add_chord_targets(GrowBuild *build, NodeId owner, const PlanHarmonyShape *shape,
                                  const PlanConstraint *cadence);
/* The open melody span of an idea, asking for the motive it is made from. */
PlanResult grow_add_melody_target(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref,
                                  const PlanConstraint *cadence);

/*
 * The melody of an idea written as the motive is (R-466): one span over the idea with the motive's notes from time 0, the
 * pitches as given, asking for the motive ref and already Realized, so the melody stage leaves it and the harmony, the
 * bass and the fidelity read it.
 */
PlanResult grow_add_melody_written(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref, const Motive *motive);

/* The key lane over the section: see grow_theme.c. False when the lane does not tile the section. */
bool grow_read_keys(const PiecePlan *plan, GrowThemeRequest *request);
/* The first bar of the material section a run of `wanted` motive bars can go in (see grow_theme.c). */
/*
 * The most motives grow-theme makes in the material, one bar each: the two segments of the fragments and a liquidation for each of
 * the two fragments and the cadential idea (apply takes a bar for every one it makes).
 */
#define GROW_MADE_NODES 5

size_t grow_first_free_bar(const PiecePlan *plan, NodeId material, PlanSpan material_span, Rational bar, size_t wanted);

/*
 * Free phrases (R-170): a section with a FREE_PHRASE constraint is a phrase of any length in beats, laid out as one
 * idea per bar (grow_free.c). grow-theme builds it when no ARCHETYPE and THEME_PAIRING are on the section.
 */
#define GROW_FREE_MAGIC 0x47465231UL
typedef struct GrowFreeSpec GrowFreeSpec;
bool grow_free_applies(const PiecePlan *plan, FillTarget target);
bool grow_free_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, const GrowContext *context,
                        FillCandidateList *out);
PlanResult grow_free_apply(PiecePlan *plan, FillTarget target, const GrowFreeSpec *spec);
/* The checker's verdict on a free phrase candidate: NULL when it holds, else the hard reason; soft rules in *soft. */
const char *grow_free_judge(const GrowFreeSpec *spec, unsigned *held, unsigned *total);

/* The ARCHETYPE (PlanArchetype) a pairing is, or -1 for a pairing standard-form does not know. */
int grow_pairing_archetype(const PlanThemePairing *pairing);
/*
 * The chords a phrase model of this shape and cadence has before its last: a tonic unless the shape is cadential, the
 * predominants the key's mode has, a dominant unless the cadence is half.
 */
size_t grow_chords_before_last(const GrowThemeRequest *request, PlanHarmonyShapeKind shape, CadenceType cadence);
/*
 * Whether the chords of a half of the theme of half_bars bars can be kept from crossing a barline: a phrase model of this cadence has
 * only so many chords, and each may be no longer than a bar. False when they cannot cover the half's cells.
 */
bool grow_split_possible(const GrowThemeRequest *request, size_t half_bars, PlanHarmonyShapeKind shape,
                         CadenceType cadence);
/* An eighth of the theme. */
Rational grow_unit(const GrowThemeRequest *request);
/* Where each idea of the request's theme lies; false when the theme has no length or a length does not fit a Rational. */
bool grow_layout_of(const GrowThemeRequest *request, GrowLayout *out);
/*
 * Whether an extension of this kind can follow or stretch the idea under the pairing (R-468): a repeat follows the
 * repetition, the contrasting idea, fragment 1, fragment 2, the consequent contrasting idea or cadential 1; a stretch
 * lengthens the idea that ends the theme (the cadential idea, the consequent contrasting idea, cadential 2) or an
 * antecedent's contrasting idea. An addition and an evaded cadence name no idea: they are always allowed here.
 */
bool grow_extension_allowed(const PlanThemePairing *pairing, PlanExtensionKind kind, GrowIdea idea);
/*
 * The motives of the extensions of the spec's layout, drawn after the beginning and the ending: a repeat's is its idea's
 * developed motive in sequence one step down (or, varied, with its last interval changed), an addition's the trailing
 * segment of the theme's last idea ending on the cadence's tone. Each is kept in spec->extended and fits its bars. False,
 * with nothing to build, when one cannot be made or an evaded cadence's idea cannot restate the last idea's motive.
 */
bool grow_extensions_draw(const GrowThemeRequest *request, GrowThemeSpec *spec);
/* The bars the made motives of the spec's extensions take in the material, one bar at least each. */
size_t grow_extended_bars(const GrowThemeSpec *spec, Rational bar);
/*
 * The bars of a construction as grow_layout_of lays them out (R-468): its beginning and its ending with their stretches
 * and repeats, the idea after an evaded cadence (not in the ending) and the addition (after it).
 */
typedef struct {
    size_t beginning;
    size_t ending;
    size_t evaded;
    size_t addition;
} GrowConstructionBars;
void grow_construction_bars(const PlanThemePairing *pairing, const PlanConstruction *construction, GrowConstructionBars *out);
/*
 * Where the ideas and the extensions of a construction lie, as grow_construction_describe counts them: 0-based first bars
 * and bars, a stretched idea with its stretch, an extension with its bars made explicit (a stretch spans the idea it
 * lengthens). An idea the pairing does not have has 0 bars; `halves` are the halves' bars, the ending with the idea after an
 * evaded cadence.
 */
typedef struct {
    size_t halves[2];
    size_t idea_first[GROW_IDEA_COUNT];
    size_t idea_bars[GROW_IDEA_COUNT];
    size_t extension_first[PLAN_CONSTRUCTION_EXTENSIONS];
    size_t extension_bars[PLAN_CONSTRUCTION_EXTENSIONS];
} GrowConstructionSpans;
void grow_construction_spans(const PlanThemePairing *pairing, const PlanConstruction *construction, GrowConstructionSpans *out);
/* The bars the extensions of a construction add to its ideas' (R-468), as grow_layout_of lays them out. */
size_t grow_construction_extra_bars(const PlanThemePairing *pairing, const PlanConstruction *construction);
/*
 * The sentence grow_construction_describe writes for the theme section's CONSTRUCTION and THEME_PAIRING constraints, ""
 * (and 0) for a section without a construction.
 */
size_t grow_section_describe(const PiecePlan *plan, NodeId section, char *text, size_t size);
/* The bars a construction's repeats and additions need in the material at most: a bar each at least, else their own. */
size_t grow_extension_material_bars(const PlanThemePairing *pairing, const PlanConstruction *construction);
/*
 * As grow_add_chord_targets, over `span` of the owner, which may stop short of the owner's end (the harmony of an ending
 * up to the idea after an evaded cadence, R-468).
 */
PlanResult grow_add_chord_targets_over(GrowBuild *build, NodeId owner, PlanSpan span, const PlanHarmonyShape *shape,
                                       const PlanConstraint *cadence);
/* The idea node of the repeat layout.extensions[k] in the container, named "<the idea it follows> repeated". */
PlanResult grow_add_repeat_node(GrowBuild *build, NodeId container, size_t k, NodeId *out);
/* The ref of an extension's idea: its motive node (`extension <k>`), restated exactly. */
void grow_extension_ref(const GrowBuild *build, size_t k, PlanMotiveRef *out);
/* standard-form's own beginning and ending for the plan's. */
int grow_phrase_beginning(PlanThemeBeginning beginning);
int grow_phrase_ending(PlanThemeEnding ending);
/* Whether two motives go up and down in the same places. */
bool grow_same_contour(const Motive *a, const Motive *b);
/*
 * Makes the source of the idea at `place` fit the idea: when it is longer than the idea's room, its leading notes with the
 * most notes that fit become spec->shortened[place] (has_shortened), which grow_idea_source then returns. True with nothing
 * set when the source fits; false when no leading fragment of two notes or more does.
 */
bool grow_shorten_source(GrowThemeSpec *spec, GrowPlace place);
/* The development of m0 the contrasting idea is, drawn from rng so that its contour differs from m0's and the result
   fits the idea at `place`. want_pc is the pitch class its last note should land on (negative for any); avoid, if given, is a
   development it may not repeat. False when there is none. */
bool grow_draw_contrast(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec,
                        GrowPlace place, const Motive *source, int want_pc, const MotiveRelationStep *avoid,
                        MotiveRelationStep *out);
/* The development of m0 an idea is made as, and whether the idea is made from m0 at all. The first idea of a slot with
   a motive of its own is that motive exact. */
bool grow_idea_dev(const GrowThemeSpec *spec, GrowPlace idea, MotiveRelationStep *out);

/*
 * The slot an idea belongs to under the pairing, and whether it is the slot's first idea (the one that takes the slot's
 * motive itself): the repetition of a presentation, the consequent basic idea and the basic idea are the basic slot's.
 */
GrowSlot grow_idea_slot(const PlanThemePairing *pairing, GrowPlace idea, bool *first);
/* Whether the pairing has an idea of the slot, and the eighths of the theme its first idea has. */
bool grow_slot_used(const PlanThemePairing *pairing, GrowSlot slot, size_t *units);
/* Whether the idea's motive is one given for it: its own (R-206), or its slot's for the slot's first idea. */
bool grow_idea_own(const GrowThemeSpec *spec, GrowPlace idea);
/* The place an idea has under a pairing; false when the pairing has no such idea. */
bool grow_idea_place(const PlanThemePairing *pairing, GrowIdea idea, GrowPlace *place);
/* The eighths of the theme the idea at a place has. */
size_t grow_place_units(const PlanThemePairing *pairing, GrowPlace place);
/* The material node of an idea's own motive ("i<k>"), else PLAN_NODE_NONE. */
NodeId grow_find_idea_node(const PiecePlan *plan, GrowIdea idea);
/* The name of the idea's material node. */
const char *grow_idea_node_name(GrowIdea idea, char *buffer, size_t size);
/* Whether the idea is a segment of m0 (a continuation's fragment, ch. VIII), and the segment when it is. */
bool grow_idea_segment(const GrowThemeSpec *spec, GrowPlace idea, const MotiveSketchFeature **segment);
/* The motive an idea is made from: its own, else its slot's, else the spec's source. */
const Motive *grow_idea_source(const GrowThemeSpec *spec, GrowPlace idea);
/* The motive node an idea's motive ref names: its slot's own node, else the build's source. */
NodeId grow_idea_node(const GrowBuild *build, GrowPlace idea);
/* The material node of the slot's motive ("s1" to "s3"), else PLAN_NODE_NONE. */
NodeId grow_find_slot_node(const PiecePlan *plan, GrowSlot slot);
/* The name of the slot's material node; NULL for the basic slot. */
const char *grow_slot_node_name(GrowSlot slot);

/* The beginning of the theme: the choices it draws, and its nodes, motives and open lanes. */
bool grow_beginning_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec);
PlanResult grow_beginning_build(GrowBuild *build);
/* The ending of the theme; false when the draw does not make an ending the motive fits. */
bool grow_ending_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec);
PlanResult grow_ending_build(GrowBuild *build);

/*
 * Whether grow-theme can build a theme of these bars, pairing and cadence in the meter: a supported meter, 4 or 8
 * bars, a pairing standard-form knows, a PAC, IAC or HC with room in half the theme, and for an antecedent a weaker
 * first cadence (HC or IAC) that has room too.
 */
bool grow_theme_buildable(const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                          CadenceType cadence, CadenceType first);
/*
 * The same for a theme with a construction (R-468): 3 to 16 bars, the construction's ideas fit the pairing and add up
 * to the bars, and each half has room for its cadence by the bars the construction gives it.
 */
bool grow_construction_buildable(const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                                 CadenceType cadence, CadenceType first, const PlanConstruction *construction);
/* Whether an accompaniment can be written in the meter, as a homophonic texture, Alberti only in a simple meter. */
bool grow_accompaniment_valid(const PlanMeter *meter, const PlanAccompaniment *accompaniment);

/* The cells in a bar the harmony operators place chords on; false for a meter they do not support. */
bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells);

/* The length of a bar of the meter. */
Rational grow_bar_length(const PlanMeter *meter);

/* How a motif compares with the idea it fills: shorter (grown to the idea), as long (placed as written), longer. */
typedef enum { GROW_FIT_SHORTER, GROW_FIT_WHOLE, GROW_FIT_LONGER } GrowMotiveFit;
/*
 * The motif's bars, ceil((length less its pickup) / bar), against the bars of the idea it fills. A pickup stays before
 * the idea's first barline, so it does not count. A motif of no length is shorter.
 */
GrowMotiveFit grow_motive_fit(const Motive *motive, Rational bar, size_t idea_bars);
/* Whether the motive fits an idea of idea_bars bars: not longer by grow_motive_fit, and its length (pickup included) within it. */
/*
 * Whether every given chord in a bar the cadence fixes (the last bar of the theme, and of the first part of an antecedent)
 * is one that cadence allows; else GROW_ERR_BAD_REQUEST with the first refusal, at /chords/<entry>. `first` is the
 * antecedent's cadence.
 */
GrowResult grow_check_given_cadences(const GrowRequest *request, CadenceType first, GrowRefusal *refusal);
bool grow_motive_fits_idea(const Motive *motive, Rational bar, size_t idea_bars);
/*
 * Whether a motive longer than the basic idea is exactly as long as the first part of the theme, part_bars bars, and the
 * beginning is one with a part to fill whole: a presentation or an antecedent (a compound basic idea is one idea).
 */
bool grow_motive_fills_part(const Motive *motive, Rational bar, size_t part_bars, PlanThemeBeginning beginning);
/*
 * The basic idea a part-length motive gives the rest of the theme: its leading notes that start inside the idea's
 * bars, as the MotiveRelationStep (a leading fragment) that makes it from the motive. False when the motive is not
 * part-length, fewer than two notes start inside the idea, or a note runs past the idea's end.
 */
bool grow_part_basic_idea(const Motive *motive, Rational bar, size_t part_bars, size_t basic_bars,
                          PlanThemeBeginning beginning, Motive *basic, MotiveRelationStep *step);
/* The bars a motive's node takes in the material, ceil(length / bar), at least one: its pickup is part of the node. */
size_t grow_motive_layout_bars(const Motive *motive, Rational bar);

/* The node named "m0" of the plan (its Given source motive), else PLAN_NODE_NONE. */
NodeId grow_find_source(const PiecePlan *plan);
/* Reads the motive on a motive node's Given or Realized melody lane span, with the meter and key given. */
bool grow_read_motive(const PiecePlan *plan, NodeId node, const PlanMeter *meter, const AnalysisKey *key, Motive *out);

/*
 * One segment of a motive as a motive of its own: its notes from time 0 (no pickup), with the source's meter and key, the
 * base harmony chords that sound under it (onsets moved to the segment) and the tone roles of its notes. False for a
 * segment of fewer than two notes or one that does not lie in the source.
 */
bool grow_segment_motive(const Motive *source, const MotiveSketchFeature *segment, Motive *out);

/* The development of m0 each idea after the basic idea is, as a spec the motive operators read. */
void grow_idea_ref(const GrowThemeSpec *spec, GrowPlace idea, NodeId source, PlanMotiveRef *out);
/*
 * The motive the ref makes from source through the variant registry: one chain (no step or an exact step is `exact`; a
 * liquidation with ref->steps is that many liquidate-features steps; every other kind the registry row of that relation,
 * its parameters set from the step). Deterministic: a fixed seed of 1 draws any open parameter. False when it does not apply.
 */
bool grow_develop_motive(const Motive *source, const PlanMotiveRef *ref, Motive *out);
/* Whether every pitch of the motive is within low to high and, with in_scale and a key, in its scale (a minor key also has the raised seventh). */
bool grow_motive_in_range(const Motive *motive, int low, int high, bool in_scale);
/* The chain grow_develop_motive applies, and the seed it draws open parameters from. False when the ref has no chain. */
#define GROW_DEVELOP_SEED 1ul
bool grow_develop_chain(const PlanMotiveRef *ref, VariantChain *chain);
/* The specs those refs make, to make the result here with the same Ursatz call the operators use. */
bool grow_idea_develop(const GrowThemeSpec *spec, GrowPlace idea, Motive *out);

/*
 * Fidelity of an idea to its source, read from the plan (R-354, grow_fidelity.c). Nothing in src/ calls these yet.
 * The role an idea has by its name (R-355): presentation (basic idea, repetition, consequent basic idea), continuation
 * (fragment 1 and 2), cadential (cadential, cadential 1 and 2) and contrast (contrasting idea, consequent contrasting idea).
 */
/* GROW_ROLE_NONE for a NULL name or one that is no idea's. */
GrowIdeaRole grow_idea_role(const char *idea_name);
/*
 * The least fidelity (R-354's score) an idea of the role may have under the development setting before the melody stage
 * refuses it (R-355); 0 for GROW_ROLE_NONE, GROW_ROLE_CONTRAST and values out of range.
 */
MotiveScore grow_fidelity_minimum(GrowDevelopment development, GrowIdeaRole role);
/* The default table: the minimums grow_fidelity_minimum reads. */
void grow_fidelity_table_default(GrowFidelityTable *out);
/* grow_fidelity_minimum over a table (R-411); a NULL table is the default one. */
MotiveScore grow_fidelity_minimum_in(const GrowFidelityTable *table, GrowDevelopment development, GrowIdeaRole role);
/*
 * The motive an idea's melody span asks for is made from, as the ref says: the span's MOTIVE_REF names the idea's motive
 * node; that node's own MOTIVE_REF gives the source node and the ref (a node with none, m0 or a motive given for the idea,
 * is its own source, with the span's ref). A ref that names a segment makes the source that segment of m0 (the labelled
 * feature of basic when its notes are m0's, else the proposal) and sets *from_segment; a liquidation by segments is made
 * from m0 whole. basic may be NULL. False when the span has no MOTIVE_REF or its nodes cannot be read.
 */
bool grow_idea_fidelity_source(const PiecePlan *plan, LaneSpanId melody, const Motive *basic, Motive *source,
                               PlanMotiveRef *ref, bool *from_segment);
/*
 * The notes the idea's melody span holds inside its motive node's length (onsets from the span's start, the pickup part of
 * the length), as a motive in the key and meter of the lanes over the span. False when no note falls inside.
 */
bool grow_idea_realized(const PiecePlan *plan, LaneSpanId melody, Motive *out);
/* The two above and motive_fidelity. */
bool grow_idea_fidelity(const PiecePlan *plan, LaneSpanId melody, const Motive *basic, MotiveFidelity *out);

/*
 * Whether the ref itself makes the idea differ from its source (a liquidation, a fragment by notes, an inversion, any relation
 * but exact, a transposition, a dominant form and a segment's own fragment, R-355), so its fidelity is low by what it was asked
 * to be. Such an idea is ranked but never refused.
 */
bool grow_ref_departs(const PlanMotiveRef *ref, bool from_segment);

/*
 * The melody stage's fidelity checker (R-355). Measuring is the costly part (the recognizer, R-307), so a candidate whose
 * realized notes were measured before for the same span, in any octave, is not measured again: the cache holds those
 * results for the length of one stage run (zero it before use; NULL in the context means none).
 */
#define GROW_FIDELITY_CACHE 128
typedef struct {
    struct {
        unsigned long long key;
        MotiveFidelity fidelity;
        bool departs;
    } entries[GROW_FIDELITY_CACHE];
    size_t count; /* entries made so far; the oldest is replaced once full */
} GrowFidelityCache;

typedef struct {
    GrowDevelopment development;
    const Motive *basic;      /* the basic motive the run was made from (grow_idea_fidelity_source), may be NULL */
    FillCheckerFn inner;      /* the melody stage's own checker, run first */
    void *inner_context;
    GrowFidelityCache *cache; /* NULL for none */
    const MotiveScore *minimum; /* NULL for the table's (grow_fidelity_minimum_in); else the minimum of every role, for tests */
    const GrowFidelityTable *table; /* NULL for the default table */
} GrowFidelityContext;

/*
 * Runs context->inner first; a hard violation stands. For a melody-fill candidate whose target span has a MOTIVE_REF and
 * whose owner idea has a role (grow_idea_role): the realized notes (melody_candidate_realized) are measured against the
 * idea's source, and a score under the role's minimum (grow_fidelity_minimum) is a hard violation, "motive fidelity n/d
 * under n/d (role)". Otherwise the score becomes
 *   ((agree * 100 + floor(100 * fidelity)) * (total + 1) + held) / (201 * (total + 1))
 * where held/total is the inner score, so agreement ranks first, then fidelity, then the melody rules' share; the style
 * part the inner left (a part of held/total) is scaled as held is, so it stays a part of the score. An idea whose ref makes
 * it differ from its source (grow_ref_departs) is ranked but never refused. Every other candidate passes through unchanged.
 * A cadential idea is for its cadence: it is refused under its minimum (its score only; the search never reaches its residue)
 * but ranked by the melody rules alone, which put the cadence tone first.
 */
bool grow_fidelity_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out);

/* The first section that asks for an archetype and a theme pairing; with open_only, a Constrained one. */
NodeId grow_find_theme(const PiecePlan *plan, bool open_only);

#endif
