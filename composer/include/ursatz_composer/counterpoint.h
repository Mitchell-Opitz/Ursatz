#ifndef URSATZ_COMPOSER_COUNTERPOINT_H
#define URSATZ_COMPOSER_COUNTERPOINT_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>

/*
 * The counterpoint operator: a Fill operator that writes a counter-melody against a given line, in the first, second,
 * third or fourth species of strict counterpoint (R-067).
 *
 * A target is a Constrained inner voice span, voice 1, with a COUNTERPOINT constraint (constraint.h) on the span or on
 * its owner, and a key. The given line is the melody lane when the constraint's above is false (the counter-melody sounds
 * below it) and the bass lane when above is true (the counter-melody sounds above it). The given line is written, its
 * notes follow one another from the start of the target to its end with no rest, and every note is in the key's scale (the
 * raised seventh included in a minor key). Anything else, the operator does not apply to.
 *
 * Species 1 writes one note against each note of the given line, whatever its rhythm. Species 2 to 4 need a cantus
 * firmus: one note for each whole bar, from a barline, three bars at the least. Species 2 writes two notes a bar, species
 * 3 four, species 4 syncopated half notes tied over the barline, after a half rest. Species 3 needs a meter of four
 * beats. The final bar is a whole note on the tonic.
 *
 * The counter-melody is searched note by note with a beam (counterpoint_species.c): the pitches are the key's scale inside
 * the range of a soprano (above) or a tenor (below), a step is pruned by standard-counterpoint's rules for the species
 * (the rule groups that hold for a line that is not finished yet), and a complete line is kept only when the whole check
 * holds. The cost of a line is the semitones its leaps span over 2, a repeated note, and a run of three parallel imperfect
 * consonances; the cheapest lines are the candidates, ties ordered by the run's rng.
 */

#define COUNTERPOINT_OPERATOR_NAME "counterpoint-species"

#define COUNTERPOINT_DEFAULT_CANDIDATES 4
#define COUNTERPOINT_DEFAULT_BEAM 128

typedef struct {
    size_t candidates; /* the lines proposed; 0 means COUNTERPOINT_DEFAULT_CANDIDATES */
    size_t beam;       /* the lines kept at each step; 0 means COUNTERPOINT_DEFAULT_BEAM */
} CounterpointParams;

CounterpointParams counterpoint_params_default(void);

/*
 * Why the operator does not apply to a target, as a short phrase for people, or NULL when it does. The target must be a
 * lane span of the plan.
 */
const char *counterpoint_why_not(const PiecePlan *plan, LaneSpanId target);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker. Hard: standard-counterpoint's check of the
 * pair (the given line the cantus firmus, the tonic from the key, the default limits) breaks no hard rule in any group,
 * and for species 2 to 4 neither does the species' own exercise check. The given line is not judged. Soft, the score being
 * the share of soft groups with no violation. A candidate of another operator goes to harmony_phrase_checker.
 */
bool counterpoint_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out);

#define COUNTERPOINT_REPORT_MAX_GROUPS 8

/* The rule groups standard-counterpoint asked of a written counter-melody, with whether each held. */
typedef struct {
    int species;
    size_t count;
    const char *name[COUNTERPOINT_REPORT_MAX_GROUPS];
    bool hard_ok[COUNTERPOINT_REPORT_MAX_GROUPS]; /* no hard rule of the group is broken */
    bool soft_ok[COUNTERPOINT_REPORT_MAX_GROUPS]; /* no soft rule of the group is broken */
    bool hard_ok_all;
} CounterpointReport;

/*
 * Judges the counter-melody written in a target (Realized or Given, with its counterpoint constraint) the way the checker
 * does, group by group. The group of the given line itself is not judged and not listed. False when the target is not a
 * written counter-melody the operator reads.
 */
bool counterpoint_report(const PiecePlan *plan, LaneSpanId target, CounterpointReport *out);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params must outlive
 * it (NULL means the defaults).
 */
FillResult counterpoint_register(FillOperatorRegistry *registry, const CounterpointParams *params);

#endif
