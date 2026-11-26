#ifndef URSATZ_COMPOSER_API_STYLE_H
#define URSATZ_COMPOSER_API_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/style.h>

#include <ursatz_composer/grow_types.h>

/* The style members of the API entries: the blend, the categories a style is read without and the distance from it. Private to the library and the command line. */

/* The blend weights from --blend-weight / --blend-weights texts (any NULL). */
UrsatzStatus api_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                              ComposerStyleWeights *out, UrsatzError *error);

/* Loads the style at path, blended with the one at blend_path when that is not NULL. */
UrsatzStatus api_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                              const char *const *without, size_t without_count, ComposerStyle **out,
                              UrsatzError *error);

/* The Analyzer's distance from the style as {dimension: value, ..., overall}, values to four places. */
cJSON *api_style_distance_json(const StyleComparison *comparison);

/* URSATZ_OK when name is a style category a style can be read without, else a request error at /style_without. */
UrsatzStatus api_style_category_check(const char *name, UrsatzError *error);

/* The names of the style categories a style can be read without: adds name to names (at most capacity; a repeat is ignored). */
UrsatzStatus api_style_without_add(const char **names, size_t *count, size_t capacity, const char *name,
                                     UrsatzError *error);

/* What the brief of a run was. */
typedef struct {
    Rational tempo;      /* a denominator of 0 for none */
    const char *tempo_from;   /* "option", "style" or "none" */
    const PitchMaterial *material; /* the key's material */
    const char *mode_from;    /* "request" or "style" */
    bool has_density;         /* the style's density (attacks per beat) */
    Rational density;
    const char *const *without;
    size_t without_count;
    const char *development;  /* "melody", "balanced", "theme" or "blueprint" for a theme; NULL for none (not echoed) */
    const char *preset;       /* the preset a theme request names (R-754); NULL for none (not echoed) */
} GrowBrief;

/* "melody", "balanced" or "theme". */
const char *api_development_name(GrowDevelopment development);
/* {"tempo":..., "mode":..., "density":..., "without": [...]} of a brief. */
cJSON *api_brief_json(const GrowBrief *brief);

#endif
