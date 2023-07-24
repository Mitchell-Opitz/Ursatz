#ifndef URSATZ_COMPOSER_HARMONY_PHRASE_H
#define URSATZ_COMPOSER_HARMONY_PHRASE_H

#include <stdbool.h>

#include <harmony_chord.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/lanes.h>

/*
 * The phrase-model harmony operator: a Fill operator for harmony lane spans.
 * It writes a progression of root-position chords, one lane span per chord,
 * following the phrase model of standard-harmony (Tb, PD, D, Te) and ending in
 * the cadence the span asks for. Chords carry a Roman numeral and a function.
 *
 * A target is a Constrained harmony lane span that:
 *   - lies inside one meter lane span, starts on a barline and is a whole
 *     number of bars long;
 *   - has a key lane span over it, or a key constraint on the span or its owner
 *     (only the mode is read);
 *   - carries a cadence constraint, on the span or on its owner.
 * Anything else, the operator does not apply to.
 *
 * Cadence types, as standard-harmony's cadence recognizer hears them:
 *   PAC, IAC  Tb, PD, D, Te. The harmony is the same for both; they differ in
 *             the soprano (do for PAC), which a melody operator reads from the
 *             cadence constraint, which stays on the final chord's span.
 *   HC        Tb, PD, D. The phrase ends on V.
 *   DC        Tb, PD, D, then vi (VI in minor).
 *   PC        a PAC followed by IV to I, the only plagal cadence standard-
 *             harmony recognizes (IV to I elsewhere is a prolongation).
 *
 * Only the harmony lane is written. Chords are root position. A request the
 * phrase is too short for gets no candidates, so the fill loop leaves the span
 * open as FILL_STUCK_ALL_REJECTED.
 */

#define HARMONY_PHRASE_OPERATOR_NAME "harmony-phrase-model"

/* Registers the operator. FILL_OK, or the registry's error. */
FillResult harmony_phrase_register(FillOperatorRegistry *registry);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker.
 * It reads the candidate's chords against standard-harmony:
 *   - hard rules reject: the phrase model (a tonic before the dominant, IV
 *     before ii), the cadence's own progression (V to I, x to V, the deceptive
 *     motion, or the plagal IV to I), and the cadence standard-harmony's
 *     recognizer hears at the end, which must be the one requested;
 *   - soft rules score: the score is the share of the soft rules that hold.
 * Candidates from any other operator are accepted at 0/1, as the default
 * checker does.
 */
bool harmony_phrase_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                            const FillCandidate *candidate, void *context, FillCheck *out);

/*
 * The cadence standard-harmony's recognizer hears at the end of the harmony
 * lane over span, which must be tiled by chord spans written by this operator
 * inside one meter lane span. The recognizer needs a soprano, and the harmony
 * lane has none, so it is told the one the requested cadence implies: do for a
 * PAC, mi for an IAC (as a melody operator will write them). *at_end is true
 * when the cadence arrives on the last chord. False when the lane cannot be
 * read, or the recognizer hears no cadence.
 */
bool harmony_hear_cadence(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PlanCadenceType requested,
                          CadenceType *heard, bool *at_end);

/* The recognizer's name for a cadence type of the plan. */
CadenceType harmony_cadence_type_of(PlanCadenceType type);

/*
 * Roman numerals as this operator writes them: upper case for major chords
 * (V, IV, VI), lower case for minor ones (ii, vi, i), a degree sign for
 * diminished ones, a 7 after a seventh chord. The numeral does not depend on
 * inversion; only root position is written.
 */

/* Formats a root-position chord. False for an invalid chord, an inverted or cadential one. */
bool harmony_roman_format(PlanKeyMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]);
/* Reads a numeral this operator writes. False for any other text. */
bool harmony_roman_parse(PlanKeyMode mode, const char *roman, HarmonyChord *out);

#endif
