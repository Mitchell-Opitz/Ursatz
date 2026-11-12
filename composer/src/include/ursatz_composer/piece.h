#ifndef URSATZ_COMPOSER_PIECE_H
#define URSATZ_COMPOSER_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

struct GrowPoolForm;

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

/*
 * A form whose sections the request gives (the game loop, R-169, R-419): the parts of a game track as data. An intro, a body,
 * a turnaround and an ending, each a list of sections with a length in beats (any whole number of beats, not only whole
 * bars), a pickup, a cadence and a fill. The body and the turnaround are the loop; the intro is played once before it and
 * the ending once after it. docs/forms.md has the full account.
 */

typedef enum { PIECE_PART_INTRO, PIECE_PART_BODY, PIECE_PART_TURNAROUND, PIECE_PART_ENDING } PiecePart;

#define PIECE_PART_COUNT 4

/* "intro", "body", "turnaround", "ending"; "unknown" for a value that is none of them. */
const char *piece_part_name(PiecePart part);
/* False for a name that is none of these. */
bool piece_part_from_name(const char *name, PiecePart *out);

/*
 * What fills a section (R-170): nothing of its own (empty, made from the piece's basic motive), a motive (a theme
 * section at 4 or 8 bars or with a construction, else a free phrase), a composed theme's realized lanes (Given), or a progression.
 */
typedef enum { PIECE_FILL_EMPTY, PIECE_FILL_MOTIF, PIECE_FILL_THEME, PIECE_FILL_PROGRESSION } PieceFillKind;

/* "empty", "motif", "theme", "progression"; "unknown" for a value that is none of them. */
const char *piece_fill_name(PieceFillKind fill);
/* False for a name that is none of these. */
bool piece_fill_from_name(const char *name, PieceFillKind *out);

#define PIECE_SECTION_ID_MAX FORM_SPEC_ID_MAX
#define PIECE_STRUCTURE_MAX_SECTIONS 32
#define PIECE_STRUCTURE_MAX_BARS 512
/* The pickup is a multiple of this fraction of a whole note and shorter than a bar. */
#define PIECE_PICKUP_GRID 32

typedef struct {
    char id[PIECE_SECTION_ID_MAX];
    char letter[PLAN_SECTION_LETTER_MAX];
    PiecePart part;
    size_t beats;                /* the length, in beats of the meter; at least 1 */
    Rational pickup;             /* the anacrusis before the section; 0 for none */
    bool has_cadence;            /* false: piece_default_cadences decides */
    CadenceType cadence;
    PieceFillKind fill;
    const Motive *motive;        /* motif: the section's own motive, else the piece's basic motive */
    bool has_into;               /* motif: a theme section's archetype */
    PlanArchetype into;          /* sentence by default; PLAN_ARCHETYPE_HYBRID reads `hybrid` */
    PlanThemePairing hybrid;
    bool has_construction;       /* motif: a theme of the section's whole bars, 3 to 16, with a construction (R-468); no pickup */
    bool construction_auto;      /* the construction is chosen from the bars and the motive (R-469); else `construction` */
    PlanConstruction construction; /* reason GIVEN; read when has_construction and not construction_auto */
    const struct GrowPoolForm *pool; /* the motive forms the section's motives stage offers (B6) */
    size_t pool_count;
    const PiecePlan *theme_plan; /* theme: the composed theme to take the realized lanes from; must outlive the build */
    const char *const *numerals; /* progression: the numerals, one per chord; must outlive the build */
    size_t numeral_count;
    size_t cells_per_chord;      /* progression: harmony cells each chord lasts; 0 for a bar */
} PieceSection;

typedef struct {
    const char *form;      /* the name of a built-in form: "binary", "rounded-binary", ... (form_spec.h) */
    AnalysisKey key;
    PlanMeter meter;
    size_t bars;           /* the form's size; 0 for the size count gives, or the default */
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
    /*
     * A form whose sections the request gives ("game-loop"): the sections in time order, grouped by part (intro, body,
     * turnaround, ending), with a four-part voicing on every section when has_voicing (not with an accompaniment). The
     * other forms ignore them, and the game loop ignores bars, count, modulate, source and given_melodies.
     */
    PieceSection *sections;
    size_t section_count;
    bool has_voicing;
    PlanVoicing voicing;
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
 * idea in it (1 of 4, 2 of 8); for a form whose sections the request gives, the shortest of the request's sections (4, or 8
 * when every section is 8 bars). False when the request has no such layout.
 */
bool piece_basic_idea(const PieceRequest *request, size_t *theme_bars, size_t *idea_bars);

/*
 * Builds the plan of the request into *out, which the caller destroys with plan_destroy, from the form document: the one
 * builder of every form (R-416, R-419).
 *
 * A form whose sections the request gives (the game loop) is built from the request's sections:
 *   - a Form named for the document's plan_name ("structure") over the sections and the material, with a meter lane span
 *     and a key lane span, both Given, over all of it: the piece is in its home key throughout;
 *   - one Section node for each section of the intro, the body and the turnaround, in order, each `beats` long from the
 *     end of the one before, Constrained, with SECTION_LETTER, CADENCE, KEY, PIECE_PART (its part and id) and, when it
 *     has a pickup, PICKUP (the pickup is the section's own: it is not part of its length). The ending is not laid out:
 *     the ending version is built from the realized loop (R-172, piece_build_ending);
 *   - a theme section (a motif or empty section of 4 or 8 whole bars on a barline with no pickup) also has the
 *     ARCHETYPE and THEME_PAIRING grow-theme builds from, and an antecedent's second CADENCE (HC); any other length is a
 *     free phrase (FREE_PHRASE);
 *   - the loop's end is held for the seam stage (HOLD) unless it is Given or holds the body's first bar, and the Section
 *     "seam" follows it: a bar, open and empty until the seam stage copies the body's first bar into it (R-171);
 *   - the Section "material" from the first barline at or after the end of the last laid out section (its motives are
 *     bars of the meter), marked MATERIAL, holding m0, the piece's basic motive (R-170): the request's `motive`, else the
 *     motive of the first motif section, else the theme fill's, else one to invent (INVENT_MOTIVE);
 *   - a theme fill (a section with a theme_plan) is the first theme section of that plan given to the section
 *     (plan_give_section): its realized lanes are Given, moved and transposed to the section, and its archetype,
 *     pairing and cadences replace the request's. The section ends Realized and no stage targets it; the theme must be
 *     as long as the section, in the same meter and mode (PIECE_ERR_BAD_REQUEST otherwise);
 *   - a progression fill has its numerals Given from the section's start (plan_give_progression), cells_per_chord
 *     cells each (a bar when 0); the harmony stage fills the rest of the section.
 * piece_request_check's refusals are PIECE_ERR_BAD_REQUEST.
 */
PieceResult form_build_plan(const FormSpec *form, const PieceRequest *request, PiecePlan **out);

/* Builds the plan of the request into *out: form_build_plan with the built-in form named request->form (BAD_REQUEST for none). */
PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out);

/*
 * The ending version (R-172) of a game loop into *out, which the caller destroys with plan_destroy: a copy of the realized
 * loop plan's intro and body, every lane Given (plan_give_section), with the turnaround, the seam and their spans dropped,
 * and the ending sections of the request appended open after the body's end, filled as form_build_plan fills them (motif
 * and empty from the loop's basic motive, which the new plan holds as its m0; theme and progression Given). The request
 * is the one the loop was built from, and must have an ending (piece_default_ending). Run it with compose_run and
 * seed + 20; the ending's harmony hears the body's last chord as a Given neighbour. PIECE_ERR_BAD_REQUEST for a loop plan
 * that is not realized or does not hold the request's intro and body sections, or a request with no ending;
 * form_build_plan's results otherwise.
 */
PieceResult piece_build_ending(const PiecePlan *loop_plan, const PieceRequest *request, PiecePlan **out);

/*
 * PIECE_OK, or PIECE_ERR_BAD_REQUEST for: a form that is not a built-in whose sections the request gives; no body section;
 * parts out of order; ids that are empty or repeat; more than PIECE_STRUCTURE_MAX_SECTIONS sections or
 * PIECE_STRUCTURE_MAX_BARS bars; a length under one beat; a pickup that is not a multiple of 1/32 or not shorter than a
 * bar; a meter or key not supported; an accompaniment and a voicing both.
 */
PieceResult piece_request_check(const PieceRequest *request);

/*
 * The cadences the form document gives, on every section of the request without has_cadence (the document's `cadences`:
 * the rules in order, each for the last section of its roles, else the default). The game loop's: the last section of the
 * loop (the turnaround, else the body) and the last of the intro lead back (HC); the last section of the ending closes
 * (PAC); any other is an IAC.
 */
void piece_default_cadences(const FormSpec *form, PieceRequest *request);

/*
 * Adds the document's default ending (a 2-bar empty section "end", letter "E", PAC) when the request has no ending section.
 * sections must hold room for one more section than section_count; false when it does not (capacity is the size of the
 * array), when the form has no default ending or the meter has no beats; true when an ending is there already (then nothing
 * is added) or one was added.
 */
bool piece_default_ending(const FormSpec *form, PieceRequest *request, size_t capacity);

#endif
