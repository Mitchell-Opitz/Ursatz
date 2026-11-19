#ifndef URSATZ_COMPOSER_TEST_ORACLES_H
#define URSATZ_COMPOSER_TEST_ORACLES_H

#include <stdbool.h>
#include <stddef.h>

#include <model/core/rational.h>
#include <analysis/harmony/analysis_cadence.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The tests' check oracles: each reads a written plan back and applies the rules the generating operators check
 * (the production code does not call them). They read what the plan holds and nothing else.
 */

/*
 * Checks the harmony lane over span against standard-harmony, reading only what is in the plan: the same rules the checker
 * applies (phrase model, explained chords, applied chords resolving, the cadence's progression, and the cadence the
 * recognizer hears, which must be the requested one and end the phrase). A chord that carries a schema constraint needs no
 * reason of its own to be there. True when the lane could be read, with *violation NULL if nothing is broken and otherwise
 * the rule that is.
 */
bool harmony_check_lane(const PiecePlan *plan, PlanSpan span, HarmonyMode mode, CadenceType requested,
                        const char **violation);

/*
 * The same check for a lane that need not end in a cadence nor explain every inverted chord, such as the harmony under a
 * bass: only the phrase model, the applied chords resolving, and that every chord is readable are asked.
 */
bool harmony_check_lane_without_cadence(const PiecePlan *plan, PlanSpan span, HarmonyMode mode,
                                        const char **violation);

/*
 * Every cadence the recognizer hears over the harmony lane chords of span (read as harmony_read_lane does), the soprano
 * over chord i being soprano[i], a scale degree or 0 for none; count must be the number of chords. On true, *out is the
 * caller's (harmony_cadences_destroy), in order of arrival, its segment indices the chords'.
 *
 * section_starts (section_count of them, plan times inside span; NULL and 0 for none) are where the caller says a section
 * or phrase begins. Ursatz reads a V before such a start as a half cadence, with the authentic cadence into the tonic as
 * its competing reading; without one, a V to I6 is an evaded cadence (R-090, R-091).
 */
bool harmony_hear_lane_cadences(const PiecePlan *plan, PlanSpan span, HarmonyMode mode, const int *soprano,
                                size_t count, const Rational *section_starts, size_t section_count,
                                HarmonyCadences *out);

/*
 * Whether chords of the given lengths in cells, from cell 0, keep to the shape: none over max_cells_per_chord (when set;
 * the last chord is exempt when exempt_last) and, with split_at_bars, none across a barline.
 */
bool harmony_shape_keeps(const PlanHarmonyShape *shape, const size_t *cells, size_t count, size_t cells_per_bar,
                         bool exempt_last);

/*
 * Checks the melody lane over span, reading only what is in the plan: the harmony and bass lanes under it (which must be
 * written, and tile the span), in key, with params (NULL for the defaults). The same rules the checker applies. True when
 * the lane could be read, with *violation NULL if nothing is broken and otherwise the rule that is.
 */
bool melody_check_lane(const PiecePlan *plan, PlanSpan span, SpelledKey key, const MelodyFillParams *params,
                       const char **violation);

/*
 * Whether the section's melody is a figure over the skeleton: every cell not kept holds N equal notes from the skeleton
 * tone (an octave up for a raised climax) and keeps to the kind of its figure (neighbor: even positions the tone, odd
 * ones 1 or 2 semitones from it; scale: every interval inside the cell a step, 1 or 2 semitones, 3 for the augmented
 * second of the harmonic minor; broken-chord: every note a tone of the chord over the cell), and every kept cell holds the
 * notes of the returned section, moved (an octave up for a raised climax). On false, reason (size bytes) holds the first
 * fault.
 */
bool form_figure_check(const PiecePlan *plan, NodeId section, char *reason, size_t size);

#endif
