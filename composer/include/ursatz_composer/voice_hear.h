#ifndef URSATZ_COMPOSER_VOICE_HEAR_H
#define URSATZ_COMPOSER_VOICE_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * Hearing the voice leading of a written plan, the way Ursatz-Analyzer's voice-leading stage hears real music (R-064,
 * R-066). Only the plan's written lanes over the span are read, never its constraints or its nodes:
 *
 *   - four voices: the melody lane (voice 0) is the soprano, the inner voices 1 and 2 are the alto and the tenor, the bass
 *     lane is the bass;
 *   - one row for each harmony lane span (a chord): the note each voice sounds at the start of the chord, or a rest where
 *     it sounds none, spelled as a degree of the key (a tone of the chord as the tone it is);
 *   - standard-counterpoint's analysis finds the parallel fifths and octaves between every pair of voices
 *     (analysis_perfects.h);
 *   - standard-harmony's analysis, given the chords (harmony lane numerals in the key, as the form is heard), finds the
 *     chordal sevenths and the leading tones of a V that do not resolve in their voice (analysis_resolution.h). Applied
 *     chords are not judged by it.
 *
 * The span must be a whole number of bars in one meter lane span and one key lane span, and its chords must tile it.
 */

typedef struct {
    size_t parallel_fifths;
    size_t parallel_octaves;
    size_t unresolved_sevenths;
    size_t unresolved_leading_tones;
    size_t rows; /* the chords heard */
} VoiceHeard;

typedef enum {
    VOICE_HEAR_OK = 0,
    VOICE_HEAR_ERR_NULL,
    VOICE_HEAR_ERR_ALLOC,
    VOICE_HEAR_ERR_NOT_WRITTEN, /* a melody, inner voice, bass or harmony span over the range is Constrained */
    VOICE_HEAR_ERR_UNREADABLE,  /* no key, a meter the chords do not tile, or a lane the grid cannot be made from */
    VOICE_HEAR_ERR_ANALYSIS     /* an analysis that failed */
} VoiceHearResult;

const char *voice_hear_result_name(VoiceHearResult result);

/* Hears the voice leading of plan over span into *out. On failure *out is all zero. */
VoiceHearResult voice_hear(const PiecePlan *plan, PlanSpan span, VoiceHeard *out);

#endif
