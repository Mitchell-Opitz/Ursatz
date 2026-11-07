#ifndef URSATZ_COMPOSER_GROW_INTERNAL_H
#define URSATZ_COMPOSER_GROW_INTERNAL_H

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/variant_registry.h>

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

/* What a candidate of grow-theme asks for: every choice, made from the run's seed. */
typedef struct {
    unsigned long magic;
    size_t bars;
    PlanMeter meter;
    AnalysisKey key;
    CadenceType cadence;
    PlanThemePairing pairing;
    CadenceType first_cadence;         /* the weak cadence an antecedent ends in */
    PlanHarmonyShapeKind beginning_shape;  /* the shape of an antecedent's chords */
    bool beginning_split;                  /* no chord of the antecedent crosses a barline */
    PlanHarmonyAnchor compound_ends_on;    /* where a compound basic idea's prolongation ends */
    MotiveRelationStep contrast;                /* the contrasting idea of an antecedent or a compound basic idea */
    MotiveRelationStep consequent_contrast;     /* the contrasting idea of a consequent */
    size_t cadential_drop[2];              /* notes the two ideas of a cadential ending drop of m0 */
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
    bool segment_room;                     /* the material has bars to put the motives grow-theme makes in */
    /*
     * The ideas that end the theme as liquidations by segments (ch. VIII): m0 as the motive-forms read it (liquid_base,
     * its segments its features), liquidated step by step until the result fits the idea and its last note can take the
     * cadence. liquidated[place] is the result, has_liquidated[place] says the idea is that motive restated, and
     * liquidation_steps[place] is the steps it took. liquidation_segment[place] is the segment of m0 (an index of
     * segments) whose rhythm and head interval the residue keeps (R-345). An idea without one is liquidated by notes
     * (liquidate_drop, cadential_drop).
     */
    Motive liquid_base;
    bool has_liquid_base;
    Motive liquidated[GROW_PLACE_COUNT];
    bool has_liquidated[GROW_PLACE_COUNT];
    size_t liquidation_steps[GROW_PLACE_COUNT];
    size_t liquidation_segment[GROW_PLACE_COUNT];
    size_t liquidate_drop;                 /* notes the cadential idea's liquidation drops */
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
} GrowThemeSpec;

/* The theme section grow-theme grows, read from the plan. */
typedef struct {
    NodeId section;
    PlanSpan span;
    PlanMeter meter;
    AnalysisKey key;                   /* the key the section starts in */
    bool has_start_change;         /* the key before the section is another, which the section opens */
    AnalysisKey from_key;              /* that key; read only with has_start_change */
    bool has_end_change;           /* the second half of the section is in another key, which it opens */
    AnalysisKey end_key;               /* that key; read only with has_end_change */
    CadenceType cadence;
    CadenceType first_cadence; /* meaningful when has_first */
    bool has_first;
    PlanThemePairing pairing;
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
    bool given_first_half;   /* Given melody, harmony or bass lies in the first half: its chords are not written whole */
} GrowThemeRequest;

/* Where a build step adds to the plan: the theme, its material, and the node the parts of a phrase go in. */
typedef struct {
    PiecePlan *plan;
    const GrowThemeSpec *spec;
    const GrowThemeRequest *request;
    Rational unit;           /* an eighth of the theme */
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
} GrowBuild;

/* A length of count units. */
Rational grow_scaled(Rational unit, size_t count);
/* A node of the theme, Realized, from start_unit units after the theme's start and units long. */
PlanResult grow_add_node(GrowBuild *build, NodeId parent, PlanNodeKind kind, size_t start_unit, size_t units,
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
 * Whether the chords of half of the theme can be kept from crossing a barline: a phrase model of this cadence has
 * only so many chords, and each may be no longer than a bar. False when they cannot cover the half's cells.
 */
bool grow_split_possible(const GrowThemeRequest *request, PlanHarmonyShapeKind shape, CadenceType cadence);
/* An eighth of the theme. */
Rational grow_unit(const GrowThemeRequest *request);
/* standard-form's own beginning and ending for the plan's. */
int grow_phrase_beginning(PlanThemeBeginning beginning);
int grow_phrase_ending(PlanThemeEnding ending);
/* Whether two motives go up and down in the same places. */
bool grow_same_contour(const Motive *a, const Motive *b);
/* The development of m0 the contrasting idea is, drawn from rng so that its contour differs from m0's and the result
   fits two units. want_pc is the pitch class its last note should land on (negative for any); avoid, if given, is a
   development it may not repeat. False when there is none. */
bool grow_draw_contrast(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec,
                        const Motive *source, int want_pc, const MotiveRelationStep *avoid, MotiveRelationStep *out);
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
 * Whether a motive longer than the basic idea is exactly as long as the first part of a theme of theme_bars bars, and the
 * beginning is one with a part to fill whole: a presentation or an antecedent (a compound basic idea is one idea).
 */
bool grow_motive_fills_part(const Motive *motive, Rational bar, size_t theme_bars, PlanThemeBeginning beginning);
/*
 * The basic idea a part-length motive gives the rest of the theme: its leading notes that start inside the idea's
 * bars, as the MotiveRelationStep (a leading fragment) that makes it from the motive. False when the motive is not
 * part-length, fewer than two notes start inside the idea, or a note runs past the idea's end.
 */
bool grow_part_basic_idea(const Motive *motive, Rational bar, size_t theme_bars, PlanThemeBeginning beginning,
                          Motive *basic, MotiveRelationStep *step);
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

/* The first section that asks for an archetype and a theme pairing; with open_only, a Constrained one. */
NodeId grow_find_theme(const PiecePlan *plan, bool open_only);

#endif
