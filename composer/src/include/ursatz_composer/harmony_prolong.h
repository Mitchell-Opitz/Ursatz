#ifndef URSATZ_COMPOSER_HARMONY_PROLONG_H
#define URSATZ_COMPOSER_HARMONY_PROLONG_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/lanes.h>

/*
 * The prolongation harmony operator (R-050): a Fill operator for harmony lane spans that one harmony is to
 * stand for, as the first idea of a sentence does. It writes one lane span per chord, on the cells of the
 * meter, and no cadence.
 *
 * A target is a Constrained harmony lane span that:
 *   - carries a PROLONGATION harmony shape constraint, on the span or on its owner;
 *   - lies inside one meter lane span and starts and ends on a beat of it (a span between beats has a part cell at
 *     an end: see harmony_phrase.h);
 *   - has a key lane span over it, or a key constraint on the span or its owner (only the mode is read).
 * The shape's starts_on says whether the span opens on I (ANY and TONIC) or on V (DOMINANT, in an inversion,
 * since a root-position V that goes to I is a cadence); ends_on whether it ends on I (TONIC), on V (DOMINANT)
 * or on either (ANY). A span that ends on the dominant ends on V6, V6/5 or V4/3, or on a root-position V that
 * does not start on a downbeat. max_cells_per_chord and split_at_bars limit how the cells are dealt.
 *
 * The inside is built from standard-harmony's prolongation figures: I-V-I, I-V6-I, I-viio6-I6, the neighbor
 * I-IV-I and the passing I-V4/3-I6, and the same figures backwards for a span that opens on V. A request with
 * more cells than a figure has chords gives some chords more than one cell.
 *
 * A span of a single cell can only be one chord, I; a request for a different start and end gets no
 * candidates, so the fill loop leaves the span open as FILL_STUCK_ALL_REJECTED.
 */

#define HARMONY_PROLONG_OPERATOR_NAME "harmony-prolongation"

/* Registers the operator. FILL_OK, or the registry's error. */
FillResult harmony_prolong_register(FillOperatorRegistry *registry);

/* The same, working around Given material as harmony_phrase_register_given does (harmony_phrase.h). */
FillResult harmony_prolong_register_given(FillOperatorRegistry *registry);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker. It reads the candidate's chords
 * against standard-harmony: every chord readable, the applied chords resolving, no predominant ii after IV,
 * and no cadence heard inside the span or at its end, whatever the soprano. The score is the share of the soft
 * phrase rules that hold. A candidate of any other operator is checked by harmony_phrase_checker.
 */
bool harmony_prolong_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                             const FillCandidate *candidate, void *context, FillCheck *out);

#endif
