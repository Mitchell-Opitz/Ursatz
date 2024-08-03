#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "time/meter_map.h"
#include "time/time_span.h"

/*
 * The standard-texture analysis adapter (L3.9): texture features measured per measure from separated voices, a texture per
 * window classified with standard-texture's rules, and windows merged into passages. standard-texture includes no other
 * framework and no kernel analysis (the architecture test enforces both): the input is the neutral structs below, which the
 * caller fills (tests/integration/ builds them from the rule-based voice separation and the import).
 *
 * THE INPUT is built with the functions below: init with the MeterMap, add notes, run an analysis, destroy. Ids are copied.
 * TimeSpans and the MeterMap are BORROWED: they must outlive the input and every result computed from it.
 *
 *   note   id; its TimeSpan (onset to release); its pitch as a PitchNumericValue's position (a semitone number); the `track` it
 *          comes from (a piano file's hand: voices are separated per track by default); the voice index the separation gave it
 *          inside that track (any whole number, 0 the top); and the voice's confidence in [0, 1]. A voice is the pair
 *          (track, voice index); notes of one voice may overlap only as chord members
 *
 * THE SNAP. Onsets and releases are read on a grid so a performed file's near-simultaneous attacks are simultaneous: an onset within
 * 1/onset_tolerance_denominator of a whole note of a multiple of 1/onset_grid_denominator is read as that multiple, a release within
 * 1/release_tolerance_denominator is too (releases get the wider tolerance: a performer lets a note run on or lifts early). The
 * release tolerance may not be wider than half a grid step. A time on another grid is not moved. Exact input is unchanged.
 *
 * THE THRESHOLDS turn the measured features into the yes-or-no features of standard-texture's classification rules (they are
 * library defaults, documented in docs/modules/texture-analysis.md, set before any piece was read and not tuned to one):
 *   rhythmic_unison_synchrony  onset synchrony at or above this is rhythmic unison (0.75)
 *   independence_min           mean pairwise rhythmic independence at or above this, with synchrony below
 *                              independence_max_synchrony (0.5), is independent lines (0.6)
 *   prominence_min             top-voice prominence at or above this, with the pitch gap below, is a predominant melody (1.5)
 *   prominence_gap_semitones   the mean pitch gap of the top voice to the next voice (3)
 */

#define STANDARD_TEXTURE_ID_MAX 64
#define STANDARD_TEXTURE_MAX_VOICES 64

typedef enum {
    STANDARD_TEXTURE_ANALYSIS_OK = 0,
    STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT,
    STANDARD_TEXTURE_ANALYSIS_ERR_INVALID,
    STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC
} StandardTextureAnalysisStatus;

typedef struct {
    char id[STANDARD_TEXTURE_ID_MAX];
    const TimeSpan *span;
    long pitch;
    size_t track;
    size_t voice;
    double voice_confidence;
} StandardTextureNote;

typedef struct {
    unsigned onset_grid_denominator;
    unsigned onset_tolerance_denominator;
    unsigned release_tolerance_denominator;
    double rhythmic_unison_synchrony;
    double independence_min;
    double independence_max_synchrony;
    double prominence_min;
    double prominence_gap_semitones;
} StandardTextureConfig;

typedef struct {
    const MeterMap *map;
    StandardTextureNote *notes;
    size_t note_count;
    size_t note_capacity;
} StandardTextureInput;

StandardTextureConfig standard_texture_config_default(void);
bool standard_texture_config_valid(const StandardTextureConfig *config);

StandardTextureAnalysisStatus standard_texture_input_init(StandardTextureInput *input, const MeterMap *map);

/* Adds a note. ERR_INVALID for an empty or too long id, a NULL span or one that is not ahead (end after start), or a confidence
   outside [0, 1]. */
StandardTextureAnalysisStatus standard_texture_input_add_note(StandardTextureInput *input, const char *id, const TimeSpan *span,
                                                              long pitch, size_t track, size_t voice, double voice_confidence);

void standard_texture_input_destroy(StandardTextureInput *input);

#endif
