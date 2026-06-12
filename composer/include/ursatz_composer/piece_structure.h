#ifndef URSATZ_COMPOSER_PIECE_STRUCTURE_H
#define URSATZ_COMPOSER_PIECE_STRUCTURE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

struct GrowPoolForm;

/*
 * A piece structure (R-169): the parts of a game track as data. An intro, a body, a turnaround and an ending, each a
 * list of sections with a length in beats (any whole number of beats, not only whole bars), a pickup, a cadence and a
 * fill. The body and the turnaround are the loop; the intro is played once before it and the ending once after it.
 * docs/piece-structure.md has the full account.
 */

typedef enum { PIECE_PART_INTRO, PIECE_PART_BODY, PIECE_PART_TURNAROUND, PIECE_PART_ENDING } PiecePart;

#define PIECE_PART_COUNT 4

/* "intro", "body", "turnaround", "ending"; "unknown" for a value that is none of them. */
const char *piece_part_name(PiecePart part);
/* False for a name that is none of these. */
bool piece_part_from_name(const char *name, PiecePart *out);

/*
 * What fills a section (R-170): nothing of its own (empty, made from the piece's basic motive), a motive (a theme
 * section at 4 or 8 bars, else a free phrase), a composed theme's realized lanes (Given), or a progression.
 */
typedef enum { PIECE_FILL_EMPTY, PIECE_FILL_MOTIF, PIECE_FILL_THEME, PIECE_FILL_PROGRESSION } PieceFillKind;

/* "empty", "motif", "theme", "progression"; "unknown" for a value that is none of them. */
const char *piece_fill_name(PieceFillKind fill);
/* False for a name that is none of these. */
bool piece_fill_from_name(const char *name, PieceFillKind *out);

#define PIECE_SECTION_ID_MAX 16
#define PIECE_STRUCTURE_MAX_SECTIONS 32
#define PIECE_STRUCTURE_MAX_BARS 512
/* The pickup is a multiple of this fraction of a whole note and shorter than a bar. */
#define PIECE_PICKUP_GRID 32

typedef struct {
    char id[PIECE_SECTION_ID_MAX];
    char letter[PLAN_SECTION_LETTER_MAX];
    PiecePart part;
    size_t beats;                /* the length, in beats of the meter; at least 1 */
    PlanTime pickup;             /* the anacrusis before the section; 0 for none */
    bool has_cadence;            /* false: piece_structure_default_cadences decides */
    PlanCadenceType cadence;
    PieceFillKind fill;
    const Motive *motive;        /* motif: the section's own motive, else the piece's basic motive */
    bool has_into;               /* motif: a theme section's archetype */
    PlanArchetype into;          /* sentence by default; PLAN_ARCHETYPE_HYBRID reads `hybrid` */
    PlanThemePairing hybrid;
    const struct GrowPoolForm *pool; /* the motive forms the section's motives stage offers (B4) */
    size_t pool_count;
    const PiecePlan *theme_plan; /* theme: the composed theme to take the realized lanes from (B4) */
    const char *const *numerals; /* progression: the numerals, one per chord (B4) */
    size_t numeral_count;
    size_t cells_per_chord;
} PieceSection;

typedef struct {
    PlanKey key;
    PlanMeter meter;
    PieceSection *sections;      /* in time order, grouped by part: intro, body, turnaround, ending */
    size_t section_count;
    const Motive *motive;        /* the piece's basic motive (R-170); NULL for the first motif fill, else an invented one */
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_style;
    PlanStyle style;
} PieceStructure;

/*
 * PIECE_OK, or PIECE_ERR_BAD_REQUEST for: no body section; parts out of order; ids that are empty or repeat; more than
 * PIECE_STRUCTURE_MAX_SECTIONS sections or PIECE_STRUCTURE_MAX_BARS bars; a length under one beat; a pickup that is not
 * a multiple of 1/32 or not shorter than a bar; a meter or key not supported.
 */
PieceResult piece_structure_check(const PieceStructure *structure);

/*
 * The cadences R-169 defaults to, on every section without has_cadence: the last section of the loop (the turnaround,
 * else the body) and the last of the intro lead back (HC); the last section of the ending closes (PAC); any other is
 * an IAC.
 */
void piece_structure_default_cadences(PieceStructure *structure);

/*
 * Adds one 2-bar empty ending section (id "end", letter "E", PAC) when the structure has no ending. sections must hold
 * room for one more section than section_count; false when it does not (capacity is the size of the array) or an
 * ending is there already (then nothing is added and true is returned).
 */
bool piece_structure_default_ending(PieceStructure *structure, size_t capacity);

#endif
