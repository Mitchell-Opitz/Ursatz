#ifndef URSATZ_COMPOSER_PIECE_STRUCTURE_H
#define URSATZ_COMPOSER_PIECE_STRUCTURE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A piece structure (R-169): the parts of a game track as data, the request of the form "game-loop" before P44 B5 moved
 * the sections to PieceRequest (piece.h, which has PiecePart, PieceFillKind and PieceSection). Retired by B5.
 */

typedef struct {
    AnalysisKey key;
    PlanMeter meter;
    PieceSection *sections;      /* in time order, grouped by part: intro, body, turnaround, ending */
    size_t section_count;
    const Motive *motive;        /* the piece's basic motive (R-170); NULL for the first motif fill, else an invented one */
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_voicing;            /* a four-part voicing (PlanVoicing) on every section; not with an accompaniment */
    PlanVoicing voicing;
    bool has_style;
    PlanStyle style;
} PieceStructure;

/*
 * PIECE_OK, or PIECE_ERR_BAD_REQUEST for: no body section; parts out of order; ids that are empty or repeat; more than
 * PIECE_STRUCTURE_MAX_SECTIONS sections or PIECE_STRUCTURE_MAX_BARS bars; a length under one beat; a pickup that is not
 * a multiple of 1/32 or not shorter than a bar; a meter or key not supported; an accompaniment and a voicing both.
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

/*
 * Builds the plan of a structure into *out, which the caller destroys with plan_destroy. The structure is checked first
 * (piece_structure_check) and its default cadences applied to a copy.
 *
 *   - a Form "structure" over the sections and the material, with a meter lane span and a key lane span, both Given,
 *     over all of it;
 *   - one Section node for each section of the intro, the body and the turnaround, in order, each `beats` long from the
 *     end of the one before, Constrained, with SECTION_LETTER, CADENCE, KEY, PIECE_PART (its part and id) and, when it
 *     has a pickup, PICKUP (the pickup is the section's own: it is not part of its length). The ending is not laid out:
 *     the ending version is built from the realized loop (R-172);
 *   - a theme section (a motif or empty section of 4 or 8 whole bars on a barline with no pickup) also has the
 *     ARCHETYPE and THEME_PAIRING grow-theme builds from, and an antecedent's second CADENCE (HC);
 *   - the Section "material" from the first barline at or after the end of the last laid out section (its motives are
 *     bars of the meter), marked MATERIAL, holding m0, the piece's basic motive (R-170): the structure's `motive`,
 *     else the motive of the first motif section, else one to invent (INVENT_MOTIVE).
 *   - a theme fill (a section with a theme_plan) is the first theme section of that plan given to the section
 *     (plan_give_section): its realized lanes are Given, moved and transposed to the section, and its archetype,
 *     pairing and cadences replace the structure's. The section ends Realized and no stage targets it; the theme must be
 *     as long as the section, in the same meter and mode (PIECE_ERR_BAD_REQUEST otherwise). The theme's m0, in the
 *     structure's key, is the basic motive when no motive comes first (R-170).
 *   - a progression fill has its numerals Given from the section's start (plan_give_progression), cells_per_chord
 *     cells each (a bar when 0); the harmony stage fills the rest of the section, and the melody grows from the basic
 *     motive over the chords as it does in an empty section. Numerals that cannot be read, or that run past the section,
 *     are PIECE_ERR_BAD_REQUEST.
 */
PieceResult piece_structure_build_plan(const PieceStructure *structure, PiecePlan **out);

/*
 * The theme size the structure's motive has to fit: the bars of the shortest section (4, or 8 when every section is 8
 * bars) and the bars of the basic idea in it (1 of 4, 2 of 8).
 */
void piece_structure_basic_idea(const PieceStructure *structure, size_t *theme_bars, size_t *idea_bars);

/*
 * The ending version (R-172) into *out, which the caller destroys with plan_destroy: a copy of the realized loop plan's
 * intro and body, every lane Given (plan_give_section), with the turnaround, the seam and their spans dropped, and the
 * ending sections of the structure appended open after the body's end, filled as piece_structure_build_plan fills them
 * (motif and empty from the loop's basic motive, which the new plan holds as its m0; theme and progression Given). The
 * structure is the one the loop was built from, and must have an ending (piece_structure_default_ending). Run it with
 * compose_run and seed + 20; the ending's harmony hears the body's last chord as a Given neighbour. PIECE_ERR_BAD_REQUEST
 * for a loop plan that is not realized or does not hold the structure's intro and body sections, or a structure with no
 * ending; piece_structure_build_plan's results otherwise.
 */
PieceResult piece_ending_plan(const PiecePlan *loop_plan, const PieceStructure *structure, PiecePlan **out);

/*
 * Regenerating a piece (R-173), the helpers regenerate.h does not have.
 *
 * piece_section_find: the section of a structure plan with the piece-part id (the request's section id), else
 * PLAN_NODE_NONE.
 *
 * piece_held_sections: the sections the loop's end holds back for the seam stage (a HOLD constraint), written to out up
 * to capacity; the number of them, which may be more than capacity.
 *
 * piece_section_holds_body_start: whether the section is a body section that starts where the body does, so that its
 * first bar is the one the seam copies.
 *
 * piece_seam_reset: removes the lane spans of the seam's Given copy, so the next compose_run copies the body's first
 * bar as it is then. PIECE_ERR_BAD_REQUEST when they cannot be removed; PIECE_OK without a seam.
 *
 * piece_reopen_lane: makes the Given lane spans of one lane in a section and what is under it Realized, so that
 * plan_regenerate can write them again (the lanes of a theme fill or a progression are Given). The number reopened. This
 * is the one way out of Given: it is for a plan that is about to be regenerated, never for the user's own material.
 */
NodeId piece_section_find(const PiecePlan *plan, const char *id);
size_t piece_held_sections(const PiecePlan *plan, NodeId *out, size_t capacity);
bool piece_section_holds_body_start(const PiecePlan *plan, NodeId section);
PieceResult piece_seam_reset(PiecePlan *plan);
size_t piece_reopen_lane(PiecePlan *plan, NodeId section, PlanLaneKind lane);

#endif
