#ifndef URSATZ_COMPOSER_FORM_TEMPLATE_H
#define URSATZ_COMPOSER_FORM_TEMPLATE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>

/*
 * Form templates (R-068): named short forms, each a table of theme sections in order. A section is what grow-theme
 * builds (4 or 8 bars, a pairing of a beginning and an ending, the cadence it ends in), with its form letter, the
 * section it returns (if any), how its motive is made from the piece's motive m0, and, in a layered form, the lanes
 * that sound in it. Every size a template has is listed here; any other is refused.
 *
 *   binary          16 = A 8, B 8; 8 = A 4, B 4. Each a presentation and continuation ending in a PAC.
 *   rounded binary  16 = A 8 (presentation + continuation, PAC), B 4 (compound basic idea + continuation, HC),
 *                   A' 4 (returns A, PAC); 24 = A 8, B 8, A' 8.
 *   ternary         24 = A 8 (antecedent + consequent, PAC), B 8 (presentation + continuation, PAC), A 8 (returns A);
 *                   12 = 4 each.
 *   variations      8 x (1 + n), n from 1 to 8 (3 by default): the theme 8 (antecedent + consequent, PAC), then
 *                   variations 1 to n, each the same and returning the theme (an ornament for k odd, a
 *                   reharmonization for k even).
 *   loop            8 = one section (presentation + continuation, HC, so the end leads back to the first bar);
 *                   16 = A 8 (IAC), B 8 (HC).
 *   layered         8 x layers, 2 to 4 layers (4 by default): layer 1 (presentation + continuation, HC), then each
 *                   later layer returning layer 1. The LAYERS masks build up: layer 1 the bass, layer 2 the bass and
 *                   the accompaniment, layer 3 adds the melody, the last layer everything. Every layer is grown in
 *                   full, with an accompaniment (pattern ANY when the request has none); the mask decides what
 *                   sounds (render.h).
 *
 * Motives: a B section is made from m0 inverted (tonal); variation k from m0 diminished (k odd) or with an interval
 * changed (k even); every other section, returning sections included, from m0 itself (a returning section takes its
 * source's motive).
 */

typedef enum {
    FORM_TEMPLATE_BINARY,
    FORM_TEMPLATE_ROUNDED_BINARY,
    FORM_TEMPLATE_TERNARY,
    FORM_TEMPLATE_VARIATIONS,
    FORM_TEMPLATE_LOOP,
    FORM_TEMPLATE_LAYERED
} FormTemplateKind;

#define FORM_TEMPLATE_KIND_COUNT 6

/* "binary", "rounded-binary", "ternary", "variations", "loop", "layered"; NULL for an unknown kind. */
const char *form_template_name(FormTemplateKind kind);
/* False for a name that is none of these. */
bool form_template_from_name(const char *name, FormTemplateKind *out);

/* The most sections a template has: a theme and eight variations. */
#define FORM_TEMPLATE_MAX_SECTIONS 9
/* A section that returns none. */
#define FORM_TEMPLATE_NO_RETURN ((size_t)-1)
#define FORM_TEMPLATE_NAME_MAX 16

typedef struct {
    char letter[PLAN_SECTION_LETTER_MAX]; /* "A", "B", "A'", "T", "V1", "L1" */
    char name[FORM_TEMPLATE_NAME_MAX];    /* the section node's name: "A", "theme", "variation 1", "layer 1" */
    size_t bars;                          /* 4 or 8 */
    PlanThemePairing pairing;
    CadenceType cadence;              /* the cadence it ends in */
    size_t returns;                       /* the index of the section it returns, or FORM_TEMPLATE_NO_RETURN */
    PlanReturnVariation variation;        /* how it varies the section it returns; read only when it returns one */
    bool has_motive;                      /* its motive is made from m0 by `motive`; false for m0 itself */
    MotiveRelationStep motive;            /* how its motive is made from m0 (open parameters are drawn) */
    unsigned layers;                      /* the LAYERS mask (PlanLayers) in a layered form; 0 for none */
} FormTemplateSection;

typedef struct {
    FormTemplateKind kind;
    size_t bars;
    FormTemplateSection sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t section_count;
} FormTemplate;

/* The bars a template has when none are asked for: 16, 16, 24, 32 (three variations), 8, 32 (four layers). */
size_t form_template_default_bars(FormTemplateKind kind);

/*
 * The table of a template at a size. bars 0 means the size count gives, or the default; count is the variations or
 * layers (0 for the size bars gives, or the default) and must be 0 for the other templates. False for a size or
 * count not in the table, or bars and count that disagree.
 */
bool form_template_layout(FormTemplateKind kind, size_t bars, size_t count, FormTemplate *out);

#endif
