#ifndef URSATZ_COMPOSER_PIECE_INTERNAL_H
#define URSATZ_COMPOSER_PIECE_INTERNAL_H

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>

/*
 * The material section, its phrase and its idea, Realized, with m0 in it: the motive as a Given melody lane span, or,
 * with none, a Constrained one-bar span with an INVENT_MOTIVE constraint. motive_source is the SOURCE constraint on m0
 * (NULL for none). The m0 node is in *out. The material span is where the section goes; bar is the meter's bar.
 */
PieceResult piece_add_material(PiecePlan *plan, const Motive *motive, const PlanSource *motive_source, NodeId form,
                               PlanSpan material, PlanTime bar, NodeId *out);

#endif
