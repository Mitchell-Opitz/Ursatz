#ifndef URSATZ_COMPOSER_HARMONY_PHRASE_H
#define URSATZ_COMPOSER_HARMONY_PHRASE_H

#include <stdbool.h>

#include <standard-harmony/harmony_applied.h>
#include <standard-harmony/harmony_chord.h>
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
 *   - lies inside one meter lane span and starts and ends on a beat of it (a
 *     span of whole bars from a barline, or any span between beats: see
 *     harmony_phrase_min_cells);
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

/*
 * Whether the harmony operators can write for a meter: 2/4, 3/4, 4/4 and 6/8, where a chord may start on a
 * strong beat.
 */
bool harmony_meter_supported(const PlanMeter *meter);

/*
 * The fewest cells a phrase can have and still end in the cadence, in a meter the operators support; 0 for any
 * other meter. The phrase model lays the tonic, predominant, dominant and tonic of the cadence over the cells of the
 * span, a chord to a cell at the least, and the last chord starts on the last downbeat the span holds (else on the
 * first cell boundary after its lead); a span with fewer cells gets no candidates from the phrase-model operator.
 * A cell is a place in a bar where a chord may start, and a part cell at either end of a span that starts or ends
 * between two of them counts as one.
 */
size_t harmony_phrase_min_cells(CadenceType cadence, const PlanMeter *meter);

/*
 * The fewest bars a phrase of whole bars can have and still end in the cadence, in a meter the operators support;
 * 0 for any other meter. A phrase of fewer bars gets no candidates from the phrase-model operator. Its last chord is
 * the whole last bar, so this is the cells before it, plus that bar, rounded up to bars.
 */
size_t harmony_phrase_min_bars(CadenceType cadence, const PlanMeter *meter);

/* Registers the operator. FILL_OK, or the registry's error. */
FillResult harmony_phrase_register(FillOperatorRegistry *registry);

/*
 * The same operator for a plan with Given material in it (R-071): a Given bass under the span does not leave the span to
 * harmony_octave (the chords it writes stand on the bass, one chord's bass member at the pitch class of the Given bass
 * note at its start); a Given melody over it is harmonized (a note it starts on a strong beat is a tone of the chord
 * there); and the chords are heard with the Given chords at either end, so that no cadence arises across the join that
 * the plan does not ask for. A target with no Given material around it is written as harmony_phrase_register does.
 */
FillResult harmony_phrase_register_given(FillOperatorRegistry *registry);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker.
 * It reads the candidate's chords against standard-harmony:
 *   - hard rules reject: the phrase model (a tonic before the dominant, IV
 *     before ii), the cadence's own progression (V to I, x to V, the deceptive
 *     motion, or the plagal IV to I), and the cadence standard-harmony's
 *     recognizer hears at the end, which must be the one requested;
 *   - soft rules score: the score is the share of the soft rules that hold.
 * It checks the candidates of the embellishment operator (harmony_embellish.h) the
 * same way, on the whole phrase as the edit would leave it, and every chord that is
 * not a plain root-position one needs a reason to be there: a 6/4 the classifier
 * names, the inside of a prolongation, a strong predominant, a predominant seventh,
 * or V7; an applied chord must resolve to its target. An unchanged progression is
 * always accepted. Candidates from any other operator are accepted at 0/1, as the
 * default checker does.
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
bool harmony_hear_cadence(const PiecePlan *plan, PlanSpan span, KeyStructureMode mode, CadenceType requested,
                          CadenceType *heard, bool *at_end);

/*
 * Checks the harmony lane over span against standard-harmony, reading only what is in the plan:
 * the same rules the checker applies (phrase model, explained chords, applied chords resolving,
 * the cadence's progression, and the cadence the recognizer hears, which must be the requested
 * one and end the phrase). A chord that carries a schema constraint (harmony_schema.h) needs no
 * reason of its own to be there. True when the lane could be read, with *violation NULL if nothing
 * is broken and otherwise the rule that is.
 */
bool harmony_check_lane(const PiecePlan *plan, PlanSpan span, KeyStructureMode mode, CadenceType requested,
                        const char **violation);

/*
 * The same check for a lane that need not end in a cadence nor explain every inverted chord, such
 * as the harmony found from a bass (harmony_octave.h): only the phrase model, the applied chords
 * resolving, and that every chord is readable are asked.
 */
bool harmony_check_lane_without_cadence(const PiecePlan *plan, PlanSpan span, KeyStructureMode mode,
                                        const char **violation);

/*
 * Roman numerals as this operator writes them: upper case for major chords
 * (V, IV, VI), lower case for minor ones (ii, vi, i), a degree sign for
 * diminished ones, then the figures for the inversion: none, 6 and 6/4 for a
 * triad; 7, 6/5, 4/3 and 4/2 for a seventh chord. An applied chord is
 * standard-harmony's own label (V7/V, V/vi, viio7/V), with figures for its inversion (V6/5/V). The numeral does not say
 * whether a tonic 6/4 is cadential; that comes from its place in the phrase.
 */

/* Formats a diatonic chord. False for an invalid chord, and for a half-diminished seventh. */
bool harmony_roman_format(KeyStructureMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]);
/* Reads a numeral this operator writes, never cadential. False for any other text, and for an applied chord. */
bool harmony_roman_parse(KeyStructureMode mode, const char *roman, HarmonyChord *out);
/* Formats and reads an applied chord. False for an invalid chord or other text. */
bool harmony_roman_format_applied(KeyStructureMode mode, const HarmonyAppliedChord *applied,
                                  char out[PLAN_ROMAN_NUMERAL_MAX]);
bool harmony_roman_parse_applied(KeyStructureMode mode, const char *roman, HarmonyAppliedChord *out);
/*
 * The same with an inversion, for the applied chords that standard-harmony's schemas use: a triad in
 * root position or first inversion (V/vi, V6/vi), a seventh chord in any inversion (V7/V, V6/5/V,
 * V4/3/V, V4/2/V). The figures sit between the chord and its target. The parser reads root position
 * too; the plain one above reads root position only.
 */
bool harmony_roman_format_applied_inverted(KeyStructureMode mode, const HarmonyAppliedChord *applied, int inversion,
                                           char out[PLAN_ROMAN_NUMERAL_MAX]);
bool harmony_roman_parse_applied_inverted(KeyStructureMode mode, const char *roman, HarmonyAppliedChord *out,
                                          int *inversion);

#endif
