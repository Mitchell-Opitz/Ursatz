#ifndef URSATZ_COMPOSER_PIECE_H
#define URSATZ_COMPOSER_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A whole short piece from a form template (R-068). A request names the template and its size, the key, the meter, an
 * accompaniment if one is wanted, and the motive, or none to have one invented.
 *
 * piece_build_plan makes the plan:
 *   - a Form over the sections and the material, with a meter lane span and the key lane spans, all Given, over all of it;
 *   - the template's sections in order, each Constrained and asking for what grow-theme builds: ARCHETYPE,
 *     THEME_PAIRING, CADENCE, KEY (home, or the key the section ends in), an antecedent's second CADENCE (HC), the
 *     ACCOMPANIMENT when the piece has one; and SECTION_LETTER, RETURN for a returning section, MOTIVE_REF (to m0,
 *     with the template's transform) for a section whose motive is not m0 itself, LAYERS in a layered form;
 *   - a Section "material" after them, marked MATERIAL, holding the motive node "m0": the motive as a Given melody
 *     lane span, or, with no motive, a Constrained one-bar melody span with an INVENT_MOTIVE constraint for
 *     motive-invent. The material has room for each section's own motive and the motives grow-theme makes for it.
 * The form's key lane is Given, over the sections and the material: one span in the home key, or with `modulate` the
 * spans piece_plan_keys plans (piece_plan_keys.h). A returning section is given the material of the section it
 * returns by form-return (form_return.h) when the two are as long, in the same meter and in keys of the same shape;
 * any other is grown afresh from its motive, its RETURN stored only.
 */

typedef struct {
    FormTemplateKind form;
    AnalysisKey key;
    PlanMeter meter;
    size_t bars;           /* the template's size; 0 for the size count gives, or the default */
    size_t count;          /* variations or layers; 0 for the size bars gives, or the default */
    const Motive *motive;  /* NULL to have one invented */
    const PlanSource *source; /* NULL, or where a corpus gave the motive (corpus_seed.h): a SOURCE constraint on m0 */
    bool has_accompaniment;
    PlanAccompaniment accompaniment; /* read only with has_accompaniment; a layered form has one in any case */
    /*
     * Keys (R-069): with modulate the sections' keys are planned from seed (piece_plan_keys): home, or a closely
     * related key reached by a pivot chord, and the cadences across the sections follow (piece_plan_cadences). Without
     * it every section is in the home key and the cadences are the template's, as before.
     */
    bool modulate;
    unsigned long seed;
    /*
     * Given melodies (given.h) the plan is to take: the material holds a bar more for each, the motive the idea that
     * has a Given melody inside it is made from (R-071). 0 for none.
     */
    size_t given_melodies;
    /*
     * A style (R-080): with has_style the form node gets a STYLE constraint holding the model file's path and the
     * weight, which fill reads. The form is Given once the plan is built, so the constraint has to be added here.
     */
    bool has_style;
    PlanStyle style;
} PieceRequest;

typedef enum {
    PIECE_OK = 0,
    PIECE_ERR_NULL,
    PIECE_ERR_ALLOC,
    PIECE_ERR_MOTIVE_TOO_LONG, /* the motive is longer than the basic idea it fills (2 bars of an 8-bar section, else 1) */
    PIECE_ERR_BAD_REQUEST,     /* a meter or key not supported, a motive of one note, an accompaniment that is not
                                  valid, a section grow-theme cannot build in the meter */
    PIECE_ERR_STUCK,           /* a stage left a target open */
    PIECE_ERR_BAD_SIZE         /* a size or count the template does not have */
} PieceResult;

const char *piece_result_name(PieceResult result);

/* The material bars a piece of section_count sections holds: m0, then five bars a section. */
size_t piece_material_bars(size_t section_count);
/* The same for a motive that takes motive_bars bars: m0 takes them, and so does each of the five motives a section holds. */
size_t piece_material_bars_of(size_t section_count, size_t motive_bars);

/*
 * The theme size the request's motive has to fit: the bars of the shortest theme section (4 or 8) and the bars of the basic
 * idea in it (1 of 4, 2 of 8). False when the request has no such layout.
 */
bool piece_basic_idea(const PieceRequest *request, size_t *theme_bars, size_t *idea_bars);

/* Builds the plan of the request into *out, which the caller destroys with plan_destroy. */
PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out);

#endif
