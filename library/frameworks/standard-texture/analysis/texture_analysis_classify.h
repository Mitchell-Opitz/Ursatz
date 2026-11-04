#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_CLASSIFY_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_CLASSIFY_H

#include "interpretation/interpretation.h"
#include "interpretation/texture.h"
#include "relationship/relationship.h"
#include "texture_analysis_features.h"
#include "standard-texture/texture_rules.h"
#include "standard-texture/texture_classification_rules.h"

/*
 * Classification of the windows with standard-texture's rules, and the passages they merge into. No category is added: a window's
 * texture is the single answer of standard_texture_classify (texture_classification_rules.h), one of monophony, heterophony,
 * polyphony, homorhythm, melody and accompaniment.
 *
 * FROM FEATURES TO THE RULES' YES-OR-NO FEATURES. standard-texture gives no numeric threshold, so the configuration's (texture_analysis_input.h):
 *   voice_count         the most voices sounding at once (voices_max)
 *   unison              the window's unison flag (every voice the same pitch class whenever two or more sound, attacking together)
 *   rhythmic_unison     two or more voices and synchrony >= rhythmic_unison_synchrony
 *   predominant_melody  not unison, two or more voices, the top voice's pitch gap >= prominence_gap_semitones, and either prominence >=
 *                       prominence_min (its attack density differs) or the top voice stands above an accompaniment figure: another voice
 *                       repeats a short cell (pattern score >= pattern_min) and the top voice does so less
 *   independent_lines   not unison and not a predominant melody (lines of equal prominence), a pair of voices that attack, mean independence
 *                       >= independence_min and synchrony < independence_max_synchrony
 *   shared_melody       never: the source gives no way to measure that voices are variants of one line, so heterophony is not found
 * A window with no note, or whose features fit none of these (a synchrony between the thresholds, say), has no texture.
 *
 * CONFIDENCE. The margin m (0 to 1) is how far the measurement that decided the texture is from its threshold, as a share of the room
 * it has (each clipped to 0 to 1; the least of them when several decided it):
 *   monophony           1 (one voice, or a unison, has no threshold)
 *   homorhythm          (synchrony - rhythmic_unison_synchrony) / (1 - rhythmic_unison_synchrony)
 *   polyphony           the least of (independence - independence_min) / (1 - independence_min),
 *                       (independence_max_synchrony - synchrony) / independence_max_synchrony, and how far the top voice is from being
 *                       prominent: the larger of (prominence_min - prominence) / (prominence_min - 1) and
 *                       (prominence_gap_semitones - gap) / prominence_gap_semitones
 *   melody and accomp.  the least of the larger of (prominence - prominence_min) / prominence_min and (pattern_support - pattern_min) /
 *                       (1 - pattern_min), and (gap - prominence_gap_semitones) / max(prominence_gap_semitones, 1)
 * The texture confidence is 0.5 + 0.5 m (a window right on a threshold is a coin flip), and the window's confidence is that times the
 * mean voice confidence of its attacking notes, so an uncertain voice separation lowers it.
 *
 * PASSAGES. A window with no texture takes the texture of the window before it (the first ones, of the first window that has one); a
 * piece with no window that has a texture has no passage. A passage is a maximal run of windows with the same texture, so a change
 * point is the first measure of a passage after the first, on a barline by construction. Its confidence is the mean of its windows'
 * confidence (the windows that were classified); its feature summary is the mean of its windows' features (the maximum for the voice count).
 *
 * OUTPUT, per passage, a Texture (interpretation/texture.h) labeled with the texture's name whose Interpretation (FrameworkReference
 * standard-texture, scalar confidence) targets the passage id "standard-texture-passage-<n>" with the claim "standard-texture/passage";
 * and, between consecutive passages, a Relationship "changes-to" with an Interpretation "standard-texture/change" targeting its id.
 */

#define STANDARD_TEXTURE_PASSAGE_CLAIM_TYPE "standard-texture/passage"
#define STANDARD_TEXTURE_CHANGE_CLAIM_TYPE "standard-texture/change"

typedef struct {
    bool has_texture;
    StandardTexture texture; /* meaningful only with has_texture */
    double margin;           /* m, 0 to 1 */
    double texture_confidence;
    double confidence;       /* with the voice confidence */
    StandardTextureFeatures features;
} StandardTextureWindowClass;

/* The yes-or-no features of a window. False for NULL (the features are then zeroed with one voice). */
bool standard_texture_features_of_window(const StandardTextureWindowFeatures *window, const StandardTextureConfig *config,
                                         StandardTextureFeatures *out);

/* The texture of a window and its confidence. `config` may be NULL for the defaults. False for NULL. */
bool standard_texture_classify_window(const StandardTextureWindowFeatures *window, const StandardTextureConfig *config,
                                      StandardTextureWindowClass *out);

typedef struct {
    int index;
    int first_measure;
    int last_measure;
    int texture;  /* StandardTexture */
    int category; /* StandardTexture: the category of the texture */
    int windows;
    int unclassified_windows; /* windows that took the texture of a neighbour */
    double confidence;
    double texture_confidence;
    double voice_confidence;
    long long start_n;
    long long start_d;
    long long end_n;
    long long end_d;
    int voices_max;
    double voices_mean;
    double synchrony;
    double independence;
    double prominence;
    double top_gap;
    double register_span;
    double spacing;
    double density;
    double pattern;
} StandardTexturePassageClaim;

typedef struct {
    StandardTexturePassageClaim claim;
    double uncertainty;
    TimeSpan *span;   /* owned: the passage's time span, [start of its first measure, end of its last) */
    Texture *texture; /* owned */
} StandardTexturePassage;

typedef struct {
    int from;
    int to;
    int boundary_measure; /* the first measure of the passage changed to */
    int from_texture;
    int to_texture;
    int category_changed; /* 1 when the category changes too (homorhythm to melody and accompaniment does not) */
} StandardTextureChangeClaim;

typedef struct {
    StandardTextureChangeClaim claim;
    double uncertainty;
    Relationship *relationship;     /* owned */
    Interpretation *interpretation; /* owned */
} StandardTextureChange;

typedef struct {
    StandardTextureWindows windows;     /* owned */
    StandardTextureWindowClass *classes; /* owned: one per window */
    int *window_texture;                /* owned: the texture each window ends up with (a StandardTexture), -1 for none */
    StandardTexturePassage *passages;   /* owned */
    size_t passage_count;
    StandardTextureChange *changes;     /* owned: passage_count - 1 of them */
    size_t change_count;
} StandardTextureAnalysis;

/* The whole analysis: windows, classes, passages and changes. `config` may be NULL for the defaults. *out is untouched on failure. */
StandardTextureAnalysisStatus standard_texture_analyze(const StandardTextureInput *input, const StandardTextureConfig *config,
                                                       StandardTextureAnalysis *out);

void standard_texture_analysis_destroy(StandardTextureAnalysis *analysis);

#endif
