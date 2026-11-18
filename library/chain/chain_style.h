#ifndef URSATZ_CHAIN_CHAIN_STYLE_H
#define URSATZ_CHAIN_CHAIN_STYLE_H

#include <stdbool.h>

#include "chain/analysis_chain.h"
#include "theory/style/style_piece.h"
#include "theory/style/style_reference.h"

/*
 * The style of a piece as a chain heard it (R-617). chain_style_piece fills a StylePiece from the collected result of
 * analysis_chain_run_score (analysis_chain_result) and the score it ran on (analysis_chain_score): the same values the
 * Analyzer's document writes for the members a style profile counts, so a profile counted from this piece has the
 * statistics of one counted from that document.
 */

/*
 * Fills *out (any earlier content is dropped, not freed) and returns true; the caller releases it with
 * style_piece_destroy. false, with *out empty, for a NULL argument, a chain that ran on no score (analysis_chain_run), a
 * meter map the library cannot count the measures of, a time that does not fit a Rational, or on allocation failure. The
 * caller runs the chain with every stage of ANALYSIS_CHAIN_STAGE_PERCEPTION: with fewer the result is empty and so is the piece.
 */
bool chain_style_piece(const AnalysisChain *chain, StylePiece *out);

/*
 * How far the piece the chain heard is from the reference: the piece counted into a profile, its statistics measured
 * with style_reference_distance. Release out with style_comparison_destroy. STYLE_REFERENCE_ERR_ARGUMENT for a NULL
 * argument or a chain that ran on no score, STYLE_REFERENCE_ERR_ALLOC on allocation failure,
 * STYLE_REFERENCE_ERR_COMPARE when the comparison fails.
 */
StyleReferenceResult chain_style_distance(const AnalysisChain *chain, const StyleReference *reference,
                                          StyleComparison *out);

#endif
