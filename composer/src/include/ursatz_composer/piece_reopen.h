#ifndef URSATZ_COMPOSER_PIECE_REOPEN_H
#define URSATZ_COMPOSER_PIECE_REOPEN_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Regenerating a piece (R-173), the helpers regenerate.h does not have. They read the plan of a form whose sections the
 * request gives (the game loop, piece.h) by the piece parts and ids its sections carry.
 *
 * piece_section_find: the section of a piece plan with the piece-part id (the request's section id), else
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
