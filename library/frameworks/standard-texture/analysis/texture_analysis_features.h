#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_FEATURES_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_FEATURES_H

#include <stdbool.h>
#include <stddef.h>

#include "texture_analysis_time.h"

/*
 * Texture features per measure window: theory-neutral measurements of a stretch of separated voices. Each feature is its own function
 * over the events that overlap the window; the formulas below are the documentation.
 *
 * VOICES. The separation gives voices that sound together, so a hand's arpeggio, whose notes ring into each other, or a melody whose
 * notes overlap a little, comes as several voices. For texture a LINE is what attacks in turn: within a window, the voices of one track
 * that never attack at the same time are consolidated into one voice (a voice with no attack in the window is left alone), taken in
 * order of mean pitch from the top, each joining the first line of its track that it does not attack together with. Voices of
 * different tracks are never consolidated (a piano track is a hand), and two voices that ever attack together stay two.
 *
 * THE WINDOW is one measure [start, end) with its beat length. An EVENT is a note read on the grid: an onset, a release after it, a
 * pitch (semitones), the voice it belongs to (a dense index, below STANDARD_TEXTURE_MAX_VOICES) and the voice's confidence. An event
 * ATTACKS in the window when start <= onset < end, and SOUNDS at time t when onset <= t < release. An ATTACK TIME is a distinct onset
 * time inside the window. Window length in beats is (end - start) / beat.
 *
 *   voice counts   the number of distinct voices sounding at an instant: its maximum over the window, and its mean weighted by the
 *                  time each count lasts over the whole window length (a rest counts 0)
 *   synchrony      the share of attack times at which two or more voices attack: |{t : voices attacking at t >= 2}| / |attack times|.
 *                  0 with no attack. A homorhythmic passage is near 1; a line over an accompaniment in another rhythm is low
 *   independence   per pair of voices that both attack in the window: 1 - |A and B| / |A or B|, A and B their sets of attack times (1
 *                  when they share no attack, 0 when they attack together always); and the mean over those pairs (0 with no pair)
 *   prominence     of the top voice, the voice with the highest mean pitch among the voices sounding in the window. Its attack density
 *                  is its attacks per beat; the others' density is the mean of the other sounding voices' attacks per beat (a voice
 *                  that only sustains has density 0). The ratio is top / others; the prominence is max(ratio, 1 / ratio), at least 1
 *                  (1: the same density), capped at STANDARD_TEXTURE_PROMINENCE_CAP, and 0 with fewer than two sounding voices. The
 *                  gap is the mean over the top voice's attacks (the window start when it has none) of its sounding pitch minus the
 *                  highest sounding pitch of any other voice at that time (a time with no other voice sounding does not count)
 *   register       the span (highest minus lowest sounding pitch in the window) and the spacing: at each attack time and at the window
 *                  start, the sounding voices (each at its highest sounding pitch) in pitch order and the gaps between adjacent ones;
 *                  the spacing is the mean of all those gaps (0 with none)
 *   density        attack times per beat: |attack times| / window length in beats
 *   pattern        per voice with at least 3 attacks: the voice's attacks in order give intervals d[i] (pitch change to the next
 *                  attack) and inter-onset intervals o[i]. A CELL is L consecutive (d, o) pairs, L = 1 to 4, whose o sum to at most one
 *                  beat. Take the cell that repeats most: occurrences are found left to right without overlap, at least 2; the voice's
 *                  score is (the pairs the occurrences cover) / (attacks - 1). The window's pattern score is the mean (and the maximum)
 *                  of the voices' scores. High for an arpeggiated or repeated figure; evidence for accompaniment patterns
 *   voice pattern  the pattern score of one voice (as above, with at least 3 attacks), for the top voice and for the voices that accompany it:
 *                  pattern_top, and pattern_support, the greatest score among the other voices. A figure that repeats a short cell
 *                  is an accompaniment, so a top voice above one (a higher score in support than in the top voice) is evidence of a melody
 *   unison         the share of the times (attack times and the window start) with two or more voices sounding at which every sounding
 *                  voice has the same pitch class; the window is in UNISON when that share is 1 over at least one such time and
 *                  synchrony is 1
 *   confidence     the mean voice confidence over the events that attack in the window (1 with none)
 */

#define STANDARD_TEXTURE_PROMINENCE_CAP 100.0
#define STANDARD_TEXTURE_MAX_PAIRS 120

typedef struct {
    StandardTextureQ onset;
    StandardTextureQ release;
    long pitch;
    size_t voice;
    double confidence;
    size_t track; /* the track of the voice; voices of different tracks are never consolidated */
} StandardTextureEvent;

typedef struct {
    StandardTextureQ start;
    StandardTextureQ end;
    StandardTextureQ beat;
} StandardTextureWindow;

typedef struct {
    size_t first;
    size_t second;
    double independence;
} StandardTexturePair;

typedef struct {
    int measure;
    StandardTextureQ start;
    StandardTextureQ end;
    size_t events;           /* the events that sound in the window */
    size_t attacks;          /* the events that attack in it */
    int voices_max;
    double voices_mean;
    size_t attack_times;
    double synchrony;
    size_t pair_count;
    double independence_mean;
    double independence_min;
    double prominence;
    double prominence_ratio;
    double top_gap;
    size_t top_voice;
    long register_span;
    double spacing;
    double density;
    double pattern_mean;
    double pattern_max;
    size_t pattern_voices;
    double pattern_top;
    double pattern_support;
    double unison_share;
    bool unison;
    double voice_confidence;
} StandardTextureWindowFeatures;

/* True when the event sounds in the window at some instant. */
bool standard_texture_event_in_window(const StandardTextureEvent *event, const StandardTextureWindow *window);

/* The window length in beats. */
double standard_texture_window_beats(const StandardTextureWindow *window);

/* voice counts: the maximum and the time-weighted mean. */
void standard_texture_feature_voice_counts(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           int *out_max, double *out_mean);

/* synchrony: the share (0 to 1), and the attack times when `out_attack_times` is not NULL. */
double standard_texture_feature_synchrony(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                          size_t *out_attack_times);

/* independence: the pairs (up to `capacity`) and the mean and minimum over all of them; returns the number of pairs. */
size_t standard_texture_feature_independence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                             StandardTexturePair *pairs, size_t capacity, double *out_mean, double *out_min);

/* prominence: 0 with fewer than two sounding voices. */
double standard_texture_feature_prominence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           double *out_ratio, double *out_gap, size_t *out_top_voice);

/* register: the span, and the spacing as the return value. */
double standard_texture_feature_register(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                         long *out_span);

/* density: attack times per beat. */
double standard_texture_feature_density(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window);

/* pattern: the mean (the return value) and the maximum of the voices' scores, over the voices with at least 3 attacks (counted in
   *out_voices). */
double standard_texture_feature_pattern(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                        double *out_max, size_t *out_voices);

/* voice pattern: the score of one voice (0 with fewer than 3 attacks). */
double standard_texture_feature_voice_pattern(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                              size_t voice);

/* consolidation: copies the events that sound in the window into `out` (room for `count`), with the voices consolidated as above;
   returns how many. */
size_t standard_texture_consolidate_voices(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           StandardTextureEvent *out);

/* unison: the share; *out_unison is the window's unison flag (with the synchrony). */
double standard_texture_feature_unison(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                       double synchrony, bool *out_unison);

/* confidence: the mean voice confidence over the attacking events. */
double standard_texture_feature_confidence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window);

/* All of the features of one window, from the events that overlap it (the caller passes them all, or only those that sound in it),
   after the voices are consolidated. */
void standard_texture_measure_features(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                       int measure, StandardTextureWindowFeatures *out);

typedef struct {
    StandardTextureWindowFeatures *items; /* one per measure from the first to the last that has a note, in order */
    size_t count;
} StandardTextureWindows;

/*
 * Measures every window of the input: one per measure from the first measure that has a note to the last, hop one measure (a pickup
 * measure is its own short window). `config` may be NULL for the defaults. *out is untouched on failure.
 */
StandardTextureAnalysisStatus standard_texture_measure_windows(const StandardTextureInput *input, const StandardTextureConfig *config,
                                                               StandardTextureWindows *out);
void standard_texture_windows_destroy(StandardTextureWindows *windows);

#endif
