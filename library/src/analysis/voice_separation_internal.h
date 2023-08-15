#ifndef MUSIC_ANALYSIS_VOICE_SEPARATION_INTERNAL_H
#define MUSIC_ANALYSIS_VOICE_SEPARATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/voice_separation.h"
#include "time/exact_rational.h"

/*
 * Internal to voice separation: the steps as separate functions so each has its own unit tests. Not part of
 * the public API (analysis/voice_separation.h is).
 */

/* One voice as it stands at an onset. */
typedef struct {
    bool held;                /* a sounding note keeps it: it cannot take a new note */
    ExactRational pitch;      /* the held note's pitch when held, else the pitch to measure from */
    unsigned long recency;    /* larger = sounded more recently (the keep-the-previous-voice tie-break) */
} VsVoice;

/* One note attacked at the onset. */
typedef struct {
    const char *id;
    ExactRational pitch;
} VsAttack;

/*
 * Step 4: assigns the attacks to free voices. attacks must have at most as many entries as there are free
 * voices. out_voice[i] is the voice of attacks[i]. The best assignment has the fewest violations (a note on
 * the wrong side of a held voice's note, only counted when !allow_crossing), then the least total distance,
 * then the more recently sounding voices, then the lower note id on the lower voice. Without allow_crossing
 * the attacks also keep their pitch order (a higher note takes a lower-index voice; equal pitches order by
 * note id). *out_margin is the confidence in [0, 1] (see voice_separation.h). Returns 0 on success, -1 when
 * arithmetic overflows, -2 for more attacks than free voices.
 */
int vs_assign_onset(const VsVoice *voices, size_t voice_count, const VsAttack *attacks, size_t attack_count,
                    bool allow_crossing, size_t *out_voice, double *out_margin);

/*
 * The length of the beat at time `at`, from the beat grid (injected). False on failure.
 */
typedef bool (*VsBeatLength)(void *context, ExactRational at, ExactRational *out_beat);

/*
 * Step 5: whether a voice whose last note ended at last_release still has a reference at `at`: true when the
 * note is still sounding or ends exactly at `at`, or when the rest (at - last_release) is no longer than
 * hold_beats beats of the beat that holds last_release. Returns 0 on success, -1 on overflow, -3 when the
 * beat grid fails.
 */
int vs_reference_kept(ExactRational last_release, ExactRational at, ExactRational hold_beats,
                      VsBeatLength beat_length, void *beat_context, bool *out_kept);

/*
 * Step 6: when more notes are attacked than there are free voices, chooses `keep` of them to take voices and
 * marks the rest as chord members, by repeatedly dropping the lower note of the closest pair (the lower
 * note id of an equal pair). is_member[i] is true for the dropped ones. keep <= attack_count. Returns 0, or -1
 * on overflow.
 */
int vs_split_overflow(const VsAttack *attacks, size_t attack_count, size_t keep, bool *is_member);

/*
 * The voice a chord member joins: the voice whose current pitch (current[v], valid where has_current[v])
 * is nearest the member's pitch; the lower voice index wins a tie. Returns voice_count when no voice has a
 * current pitch. -1 is not used: distances that overflow count as farthest.
 */
size_t vs_nearest_voice(const ExactRational *current, const bool *has_current, size_t voice_count,
                        ExactRational pitch);

#endif
