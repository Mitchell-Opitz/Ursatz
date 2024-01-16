#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_RESOLUTION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_RESOLUTION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "analysis_voices.h"
#include "harmony_resolution_rules.h"
#include "interpretation/interpretation.h"
#include "structure/structure.h"

/*
 * Seventh and leading tone resolution on separated voices (the standard-harmony analysis adapter, step 6). The rules are
 * standard-harmony's (harmony_resolution_rules.h, which extends the V7 resolution rules of harmony_seventh_rules.h to one
 * voice of any chord); the adapter finds each voice's role in the chord and what it does in the next chord, and asks the rule.
 *
 * WHERE. A chord is a run of consecutive segments making the same chord (analysis_runs.h). Resolution is judged where the chord
 * CHANGES: from the run's LAST segment to the next run's FIRST segment, for each voice sounding in both. A seventh or leading
 * tone held through the changes of its own chord is still the chord's note, and it is judged only when the chord leaves. A
 * voice that rests in either segment is not judged. Applied chords, invalid segments and a missing bass in the from-segment are
 * not judged (the root of the chord is found from the bass pitch and the inversion).
 *
 * ROLES. A voice's role comes from its pitch class against the chord's: the chord's root pitch class is the bass's pitch class
 * less the interval of the chord member in the bass (`inversion` indexes the interval set). A voice is the chord's SEVENTH when
 * its pitch class is the seventh above the root of a seventh chord (V7, ii7, vii-diminished 7 and so on); it is the LEADING TONE
 * in a V or V7 whose third is major (the third is the leading tone) and in a vii-diminished or its seventh chord (the root is). A
 * V7 is COMPLETE when its root, third, fifth and seventh each sound in some voice.
 *
 * RULES (harmony_resolution_rules.h): SEVENTH_RESOLVES_DOWN for the seventh voice, to any next chord (a held, repeated, leaping or
 * rising seventh is a violation, and so is a suspension or a transfer to another voice, as the framework's rules allow none);
 * LEADING_TONE_RESOLVES for the leading tone voice when the next chord is a tonic (an outer voice rises to the tonic; an inner
 * voice rises, or in a complete V7 falls to sol). The voice's move is read in semitones and turned into diatonic steps (1 or 2
 * semitones a step, 3 or 4 a third, and so on): the rules read steps. The semitone is a step of the scale only in the common
 * cases, which is all these two rules ask.
 *
 * OUTPUT. A violation, one Interpretation per occurrence, targets a Structure shell whose event refs are the voice's two note ids
 * (the seventh or leading tone and the note it moved to): FrameworkReference standard-harmony, claim = a HarmonyResolutionClaim
 * (rule name, voice, the two segments, whether it was satisfied), and
 *
 *   uncertainty = 1 - c (1 - u),  c the least voice-assignment confidence of the voice's two cells,
 *                                 u the chord uncertainty (the larger of the two chords' input uncertainties),
 *
 * that is, the confidence in the voice's identity and the confidence in the chord multiply.
 *
 * Satisfied resolutions are not output unless config.emit_satisfied is set (a debug flag).
 */

#define HARMONY_RESOLUTION_CLAIM_TYPE "standard-harmony/resolution"

typedef struct {
    bool emit_satisfied; /* default false */
} HarmonyResolutionConfig;

HarmonyResolutionConfig harmony_resolution_config_default(void);

typedef struct {
    char rule_name[24];
    int rule;        /* HarmonyResolutionRule */
    int voice;
    int segment_from; /* the last segment of the chord that resolves */
    int segment_to;   /* the first segment of the next chord */
    bool satisfied;
} HarmonyResolutionClaim;

typedef struct {
    HarmonyResolutionRule rule;
    size_t voice;
    size_t segment_from;
    size_t segment_to;
    bool satisfied;
    double uncertainty;
    Structure *shell;               /* owned: the voice's two note ids */
    Interpretation *interpretation; /* owned; targets the shell's id */
} HarmonyResolutionResult;

typedef struct {
    HarmonyResolutionResult *items; /* owned; in order of chord change, then voice; NULL when none */
    size_t count;
} HarmonyResolutions;

typedef enum {
    HARMONY_RESOLUTION_OK = 0,
    HARMONY_RESOLUTION_ERR_NULL_ARGUMENT,
    HARMONY_RESOLUTION_ERR_GRID_MISMATCH, /* the grid does not have a row per segment, or has no voices or a bad cell */
    HARMONY_RESOLUTION_ERR_ALLOC
} HarmonyResolutionStatus;

/* Checks every chord change. config may be NULL for the defaults. On OK, *out is owned by the caller
   (harmony_resolutions_destroy); otherwise untouched. */
HarmonyResolutionStatus harmony_analysis_check_resolutions(const HarmonySegment *segments, size_t count,
                                                           const HarmonyVoiceGrid *grid,
                                                           const HarmonyResolutionConfig *config,
                                                           HarmonyResolutions *out);

void harmony_resolutions_destroy(HarmonyResolutions *resolutions);

/* Reads the claim back out of a resolution Interpretation. False if it is not one. */
bool harmony_resolution_claim_read(const Interpretation *interpretation, HarmonyResolutionClaim *out);

#endif
