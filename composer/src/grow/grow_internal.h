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
