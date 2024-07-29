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
 *   - a Form over the sections and the material, with a meter and a key lane span, both Given, over all of it;
 *   - the template's sections in order, each Constrained and asking for what grow-theme builds: ARCHETYPE,
 *     THEME_PAIRING, CADENCE, KEY (home: this branch plans no other keys), an antecedent's second CADENCE (HC), the
 *     ACCOMPANIMENT when the piece has one; and SECTION_LETTER, RETURN for a returning section, MOTIVE_REF (to m0,
 *     with the template's transform) for a section whose motive is not m0 itself, LAYERS in a layered form;
 *   - a Section "material" after them, marked MATERIAL, holding the motive node "m0": the motive as a Given melody
 *     lane span, or, with no motive, a Constrained one-bar melody span with an INVENT_MOTIVE constraint for
 *     motive-invent. The material has room for each section's own motive and the motives grow-theme makes for it.
 * Section keys and cadence plans other than home and the template's defaults are a later branch's (B5); a returning
 * section is grown afresh from its motive, its RETURN stored only (B6).
 */

typedef struct {
    FormTemplateKind form;
    PlanKey key;
    PlanMeter meter;
    size_t bars;           /* the template's size; 0 for the size count gives, or the default */
    size_t count;          /* variations or layers; 0 for the size bars gives, or the default */
    const Motive *motive;  /* NULL to have one invented */
    bool has_accompaniment;
    PlanAccompaniment accompaniment; /* read only with has_accompaniment; a layered form has one in any case */
} PieceRequest;

typedef enum {
    PIECE_OK = 0,
    PIECE_ERR_NULL,
    PIECE_ERR_ALLOC,
    PIECE_ERR_MOTIVE_TOO_LONG, /* the motive does not fit in one bar of the meter */
    PIECE_ERR_BAD_REQUEST,     /* a meter or key not supported, a motive of one note, an accompaniment that is not
                                  valid, a section grow-theme cannot build in the meter */
    PIECE_ERR_STUCK,           /* a stage left a target open */
    PIECE_ERR_BAD_SIZE         /* a size or count the template does not have */
} PieceResult;

const char *piece_result_name(PieceResult result);

/* The material bars a piece of section_count sections holds: m0, then five bars a section. */
size_t piece_material_bars(size_t section_count);

/* Builds the plan of the request into *out, which the caller destroys with plan_destroy. */
PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out);

#endif
