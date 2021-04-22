#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TEXTURE_CLASSIFICATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TEXTURE_CLASSIFICATION_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "texture_rules.h"

/*
 * Texture classification rules for standard-texture-v1: which of the
 * textures (texture_rules.h) a passage has, from features measured by
 * the caller. Reading the features from events (are these voices
 * variants of one line? is there a predominant melody?) is analysis
 * (src/analysis), not a rule here, and the source gives no numeric
 * threshold for any of them, so each is a plain yes or no:
 *
 *   FEATURES    a passage has voice_count voices sounding (at least 1) and
 *               five yes-or-no features: unison (every voice has the same
 *               line), shared_melody (the voices are variants of one
 *               melodic line), rhythmic_unison (the voices move together at
 *               the same pace), predominant_melody (one voice is a clear
 *               melody and the rest support it), independent_lines (each
 *               voice has its own melodic line and rhythm). Features that
 *               contradict each other are not a passage: a single voice
 *               has no shared melody, melody and accompaniment, or
 *               independent lines; unison lines are not variants and have
 *               no predominant melody; independent lines are not
 *               rhythmically unified and not variants of one melody
 *   MATCHING    a passage can have more than one texture (a melody that
 *               stands out in a homorhythmic passage is both homorhythm
 *               and melody and accompaniment). The textures it has:
 *               monophony for one voice or unison; heterophony for
 *               several voices sharing a melody; polyphony for several
 *               voices with independent lines; homorhythm for several
 *               voices in rhythmic unison that are not in unison; melody
 *               and accompaniment for several voices with a predominant
 *               melody; homophony for either of the last two
 *   PRECEDENCE  one answer when one is wanted: the first that matches of
 *               monophony, heterophony, polyphony, homorhythm, melody and
 *               accompaniment. Homorhythm comes before melody and
 *               accompaniment, so a rhythmically unified passage with a
 *               melody standing out is homorhythm, as the source treats
 *               it. The general homophony is never the single answer: it
 *               is the category of the two subcategories
 *
 * A passage with none of the features and more than one voice has no
 * texture here.
 */

typedef struct {
    int voice_count;
    bool unison;
    bool shared_melody;
    bool rhythmic_unison;
    bool predominant_melody;
    bool independent_lines;
} StandardTextureFeatures;

/* A set of textures: bit n is the texture with enum value n. */
typedef unsigned int StandardTextureSet;

#define STANDARD_TEXTURE_BIT(texture) (1u << (unsigned int)(texture))

typedef enum {
    STANDARD_TEXTURE_CLASSIFICATION_RULE_FEATURES = 0,
    STANDARD_TEXTURE_CLASSIFICATION_RULE_MATCHING,
    STANDARD_TEXTURE_CLASSIFICATION_RULE_PRECEDENCE,
    STANDARD_TEXTURE_CLASSIFICATION_RULE_COUNT
} StandardTextureClassificationRule;

const Rule *standard_texture_classification_rule(StandardTextureClassificationRule which);
const Constraint *standard_texture_classification_constraint(StandardTextureClassificationRule which);

/* FEATURES: true when the features describe a passage: at least one voice
   and none of the contradictions above. False for NULL. */
bool standard_texture_features_valid(const StandardTextureFeatures *features);

/* MATCHING: every texture the passage has. 0 (the empty set) for NULL,
   invalid features, or a passage with no texture. */
StandardTextureSet standard_texture_matching(const StandardTextureFeatures *features);

/* True when the set holds the texture. False for an invalid texture. */
bool standard_texture_set_contains(StandardTextureSet set, StandardTexture texture);

/* PRECEDENCE: the single texture for the passage, never the general
   homophony. False (output untouched) for NULL, invalid features, a
   passage with no texture, or a NULL output. */
bool standard_texture_classify(const StandardTextureFeatures *features, StandardTexture *out);

#endif
