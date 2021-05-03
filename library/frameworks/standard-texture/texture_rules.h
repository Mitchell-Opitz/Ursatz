#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TEXTURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TEXTURE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Texture rules for standard-texture-v1. Texture is the density of and
 * interaction between a work's different voices. There are many types;
 * the four main categories scholars use are:
 *
 *   MONOPHONY   a single unaccompanied melodic line; all instruments
 *               playing or singing in unison. The simplest and most
 *               exposed texture
 *   HETEROPHONY multiple variations of one melodic line heard
 *               simultaneously in different voices. The variations run
 *               from small embellishing tones to longer runs in one
 *               voice, as long as the melodic material stays relatively
 *               constant
 *   HOMOPHONY   multiple voices moving together harmonically at the same
 *               pace. Very common; often one melody predominates while
 *               the other voices fill out the harmonies. Sometimes
 *               divided into two subcategories:
 *   HOMORHYTHM  all voices move in an extremely similar or completely
 *               unison rhythm; most often chorale-like block chords. A
 *               melody may still stand out
 *   MELODY_AND_ACCOMPANIMENT
 *               a clear melody distinct from supporting voices (the
 *               accompaniment); often the melody has a different rhythm.
 *               Perhaps the most common homophony
 *   POLYPHONY   multiple voices with separate melodic lines and rhythms:
 *               each voice independent, and together they make harmonies.
 *               Heard in fugues
 *
 * Most music does not keep to one texture; it moves between them
 * (mixed_texture_rules.h). Assigning a texture from the measured
 * features of a passage is texture_classification_rules.h.
 *
 * Homorhythm and melody and accompaniment are kinds of homophony, so
 * there are four categories but six textures here: the generic
 * HOMOPHONY is the category, the other two its subcategories. The names
 * are the labels a Texture (src/interpretation/texture.h) carries.
 */

typedef enum {
    STANDARD_TEXTURE_MONOPHONY = 0,
    STANDARD_TEXTURE_HETEROPHONY,
    STANDARD_TEXTURE_HOMOPHONY,
    STANDARD_TEXTURE_HOMORHYTHM,
    STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT,
    STANDARD_TEXTURE_POLYPHONY,
    STANDARD_TEXTURE_COUNT
} StandardTexture;

typedef enum {
    STANDARD_TEXTURE_RULE_MONOPHONY = 0,
    STANDARD_TEXTURE_RULE_HETEROPHONY,
    STANDARD_TEXTURE_RULE_HOMOPHONY,
    STANDARD_TEXTURE_RULE_HOMORHYTHM,
    STANDARD_TEXTURE_RULE_MELODY_AND_ACCOMPANIMENT,
    STANDARD_TEXTURE_RULE_POLYPHONY,
    STANDARD_TEXTURE_RULE_COUNT
} StandardTextureRule;

const Rule *standard_texture_rule(StandardTextureRule which);
const Constraint *standard_texture_constraint(StandardTextureRule which);

/* The label: "monophony", "heterophony", "homophony", "homorhythm",
   "melody and accompaniment", "polyphony"; NULL for an invalid texture. */
const char *standard_texture_name(StandardTexture texture);

/* Reads a label back (exact, lowercase). False (output untouched) for
   NULL, an unknown name, or a NULL output. */
bool standard_texture_from_name(const char *name, StandardTexture *out);

/* True for the four main categories: monophony, heterophony, homophony,
   polyphony. False for the two homophony subcategories and for an
   invalid texture. */
bool standard_texture_is_category(StandardTexture texture);

/* True for homophony and its two subcategories. */
bool standard_texture_is_homophonic(StandardTexture texture);

/* The main category of a texture: itself for a category, homophony for
   homorhythm and melody and accompaniment. False (output untouched) for
   an invalid texture or a NULL output. */
bool standard_texture_category(StandardTexture texture, StandardTexture *category);

/* The fewest voices a passage of the texture has: 1 for monophony, 2 for
   the others (heterophony, homophony, and polyphony are about voices
   together). 0 for an invalid texture. */
int standard_texture_minimum_voices(StandardTexture texture);

#endif
