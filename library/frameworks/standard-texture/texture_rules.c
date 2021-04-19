#include "texture_rules.h"

#include <stddef.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_TEXTURE_RULE_COUNT] = {
    "texture-monophony", "texture-heterophony", "texture-homophony", "texture-homorhythm",
    "texture-melody-and-accompaniment", "texture-polyphony"};

static StandardTextureRulePair g_pairs[STANDARD_TEXTURE_RULE_COUNT];
static bool g_built[STANDARD_TEXTURE_RULE_COUNT];

static bool valid_rule(StandardTextureRule which) {
    return which >= 0 && which < STANDARD_TEXTURE_RULE_COUNT;
}

static void ensure_built(StandardTextureRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_texture_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_texture_rule(StandardTextureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_texture_constraint(StandardTextureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool valid_texture(StandardTexture texture) {
    return texture >= 0 && texture < STANDARD_TEXTURE_COUNT;
}

static const char *const TEXTURE_NAMES[STANDARD_TEXTURE_COUNT] = {
    "monophony", "heterophony", "homophony", "homorhythm", "melody and accompaniment", "polyphony"};

const char *standard_texture_name(StandardTexture texture) {
    return valid_texture(texture) ? TEXTURE_NAMES[texture] : NULL;
}

bool standard_texture_from_name(const char *name, StandardTexture *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int t = 0; t < STANDARD_TEXTURE_COUNT; t++) {
        if (strcmp(name, TEXTURE_NAMES[t]) == 0) {
            *out = (StandardTexture)t;
            return true;
        }
    }
    return false;
}

bool standard_texture_is_category(StandardTexture texture) {
    return texture == STANDARD_TEXTURE_MONOPHONY || texture == STANDARD_TEXTURE_HETEROPHONY ||
           texture == STANDARD_TEXTURE_HOMOPHONY || texture == STANDARD_TEXTURE_POLYPHONY;
}

bool standard_texture_is_homophonic(StandardTexture texture) {
    return texture == STANDARD_TEXTURE_HOMOPHONY || texture == STANDARD_TEXTURE_HOMORHYTHM ||
           texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT;
}

bool standard_texture_category(StandardTexture texture, StandardTexture *category) {
    if (!valid_texture(texture) || category == NULL) {
        return false;
    }
    *category = standard_texture_is_homophonic(texture) ? STANDARD_TEXTURE_HOMOPHONY : texture;
    return true;
}

int standard_texture_minimum_voices(StandardTexture texture) {
    if (!valid_texture(texture)) {
        return 0;
    }
    return texture == STANDARD_TEXTURE_MONOPHONY ? 1 : 2;
}
