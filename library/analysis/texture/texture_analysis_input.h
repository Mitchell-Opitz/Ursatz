#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>


/*
 * The standard-texture analysis adapter (L3.9): texture features measured per measure from separated voices, a texture per
 * window classified with standard-texture's rules, and windows merged into passages. standard-texture includes no other
 * framework and no kernel analysis (the architecture test enforces both): the input is the neutral AnalysisNoteSet
 * (analysis/notes/analysis_note.h), in any order, which the caller fills (tests/integration/ builds it from the rule-based
 * voice separation and the import). Everything in it is BORROWED: it must outlive every result computed from it.
 *
 *   note   note_id (non-empty, under STANDARD_TEXTURE_ID_MAX); its TimeSpan (onset to release, valid); its midi (a semitone
 *          number, not unknown); the `track` it comes from (a piano file's hand: voices are separated per track by default);
 *          the `voice` index the separation gave it inside that track (any whole number, 0 the top); and the voice's
 *          confidence in [0, 1]. A voice is the pair (track, voice index); notes of one voice may overlap only as chord
 *          members. A note the analysis cannot read is ERR_INVALID
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
 *   pattern_min                a voice whose repeated-pattern score is at least this is an accompaniment figure; a top voice above one
 *                              (its own score lower) is a predominant melody whatever its rhythm (0.5)
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
    unsigned onset_grid_denominator;
    unsigned onset_tolerance_denominator;
    unsigned release_tolerance_denominator;
    double rhythmic_unison_synchrony;
    double independence_min;
    double independence_max_synchrony;
    double prominence_min;
    double prominence_gap_semitones;
    double pattern_min;
} StandardTextureConfig;

StandardTextureConfig standard_texture_config_default(void);
bool standard_texture_config_valid(const StandardTextureConfig *config);

#endif
