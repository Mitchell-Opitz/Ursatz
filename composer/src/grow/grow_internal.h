#ifndef URSATZ_COMPOSER_GROW_INTERNAL_H
#define URSATZ_COMPOSER_GROW_INTERNAL_H

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive_develop.h>

/* The bars the material section holds: room for the motive and every motive made from it. */
#define GROW_MATERIAL_BARS 8
#define GROW_THEME_NAME "theme"
#define GROW_MATERIAL_NAME "material"
#define GROW_SOURCE_NAME "m0"
#define GROW_PAYLOAD_MAGIC 0x47525731UL
/* Fragments of the basic idea are this many notes at most; the first is the leading ones of the motive. */
#define GROW_FRAGMENT_MAX 3

/* The ideas of a sentence, in time order. */
typedef enum {
    GROW_IDEA_BASIC,
    GROW_IDEA_REPETITION,
    GROW_IDEA_FRAGMENT_1,
    GROW_IDEA_FRAGMENT_2,
    GROW_IDEA_CADENTIAL,
    GROW_IDEA_COUNT
} GrowIdea;

/* What a candidate of grow-theme asks for: every choice, made from the run's seed. */
typedef struct {
    unsigned long magic;
    size_t bars;
    PlanMeter meter;
    PlanKey key;
    PlanCadenceType cadence;
    PlanThemePairing pairing;
    size_t basic_cells;                    /* the cells of the basic idea, to size the harmonic rhythm after it */
    bool statement_response;               /* the repetition is a step up, the harmony goes tonic to dominant and back */
    size_t fragment_notes[2];              /* notes of the leading fragments the two fragment ideas are */
    size_t liquidate_drop;                 /* notes the cadential idea's liquidation drops */
    PlanHarmonyShapeKind continuation_shape;
    int continuation_cap;                  /* max_cells_per_chord of the continuation */
    Motive source;                         /* m0, with the meter and key of the plan */
    /* The motives the ideas after the basic idea are made from m0 as, planned here to be checked. */
    Motive developed[GROW_IDEA_COUNT];
    bool has_developed[GROW_IDEA_COUNT];
} GrowThemeSpec;

/* The theme section grow-theme grows, read from the plan. */
typedef struct {
    NodeId section;
    PlanSpan span;
    PlanMeter meter;
    PlanKey key;
    PlanCadenceType cadence;
    PlanThemePairing pairing;
    NodeId source;
    size_t bars;
    size_t cells_per_bar;
    PlanTime bar;
} GrowThemeRequest;

/* Where a build step adds to the plan: the theme, its material, and the node the parts of a phrase go in. */
typedef struct {
    PiecePlan *plan;
    const GrowThemeSpec *spec;
    const GrowThemeRequest *request;
    PlanTime unit;           /* an eighth of the theme */
    PlanConstraint cadence;  /* the theme's cadence */
    NodeId phrase;           /* the phrase the parts go in, made by the beginning */
    NodeId material;         /* the idea of the material section the motive nodes go in */
    PlanSpan material_span;
} GrowBuild;

/* A length of count units. */
PlanTime grow_scaled(PlanTime unit, size_t count);
/* A node of the theme, Realized, from start_unit units after the theme's start and units long. */
PlanResult grow_add_node(GrowBuild *build, NodeId parent, PlanNodeKind kind, size_t start_unit, size_t units,
                         const char *name, NodeId *out);
/* The motive node m<slot> in the material, made from the ref. */
PlanResult grow_add_motive(GrowBuild *build, size_t slot, const PlanMotiveRef *ref, NodeId *out);
/* A harmony span and the bass span beside it, both open, over an idea or a subphrase. */
PlanResult grow_add_chord_targets(GrowBuild *build, NodeId owner, const PlanHarmonyShape *shape,
                                  const PlanConstraint *cadence);
/* The open melody span of an idea, asking for the motive it is made from. */
PlanResult grow_add_melody_target(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref,
                                  const PlanConstraint *cadence);

/* The beginning of the theme: the choices it draws, and its nodes, motives and open lanes. */
bool grow_beginning_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec);
PlanResult grow_beginning_build(GrowBuild *build);
/* The ending of the theme; false when the draw does not make an ending the motive fits. */
bool grow_ending_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec);
PlanResult grow_ending_build(GrowBuild *build);

/* The cells in a bar the harmony operators place chords on; false for a meter they do not support. */
bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells);

/* The length of a bar of the meter. */
PlanTime grow_bar_length(const PlanMeter *meter);

/* The node named "m0" of the plan (its Given source motive), else PLAN_NODE_NONE. */
NodeId grow_find_source(const PiecePlan *plan);
/* Reads the motive on a motive node's Given or Realized melody lane span, with the meter and key given. */
bool grow_read_motive(const PiecePlan *plan, NodeId node, const PlanMeter *meter, const PlanKey *key, Motive *out);

/* The development of m0 each idea after the basic idea is, as a spec the motive operators read. */
void grow_idea_ref(const GrowThemeSpec *spec, GrowIdea idea, NodeId source, PlanMotiveRef *out);
/* The specs those refs make, to make the result here with the same Ursatz call the operators use. */
bool grow_idea_develop(const GrowThemeSpec *spec, GrowIdea idea, Motive *out);

/* The first section that asks for an archetype and a theme pairing; with open_only, a Constrained one. */
NodeId grow_find_theme(const PiecePlan *plan, bool open_only);

/* The chosen candidate's score from a record's last applied step; false when none was applied. */
bool grow_record_score(const FillRecord *record, GrowScore *out);

#endif
