#ifndef URSATZ_COMPOSER_API_CONSTRUCTION_H
#define URSATZ_COMPOSER_API_CONSTRUCTION_H

#include <cJSON.h>

#include "api_grow.h"

/* Reading the brief of a theme or grow request, its construction and its accompaniment, and writing a construction. Private to the library and the command line. */

/* The words of the beginnings, the endings and the extension kinds, indexed by their enum values. */
extern const char *const API_BEGINNING_NAMES[3];
extern const char *const API_ENDING_NAMES[3];
extern const char *const API_EXTENSION_KINDS[4];

/* The beginning and ending of a hybrid written "antecedent+continuation"; false for any other text. */
bool api_parse_hybrid(const char *text, SectionPairing *pairing);

/* "any", "block", "arpeggiated", "alberti", "oom-pah", "motive". */
const char *api_accompaniment_name(PlanAccompanimentPattern pattern);

/* The accompaniment of the two texts (either may be NULL) for the meter. */
ComposerStatus api_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text,
                                      const PlanMeter *meter, PlanAccompaniment *out, ComposerError *error);

/*
 * The construction object itself ({"ideas": {...}, "extensions": [...]}, reason GIVEN) into *out, whose members are
 * refused at `base` ("/construction", or the path of a section's construction in a piece request).
 */
ComposerStatus api_grow_read_construction_object(const cJSON *item, const char *base, SectionConstruction *out, ComposerError *error);

/*
 * The construction member of a request (R-468): null or absent for none, else {"ideas": {"<idea name>": bars, ...},
 * "extensions": [{"kind", "idea", "bars", "varied"}]}, into out->construction with reason GIVEN. Run after
 * api_grow_read_brief (it needs the pairing): a request with a construction and no bars is as long as the construction.
 * Whether the ideas fit the pairing and add up to the bars is grow_build_plan_checked's to refuse, at /construction/...
 */
ComposerStatus api_grow_read_construction(const cJSON *request, GrowSetup *out, ComposerError *error);

/* The brief of the text: sentence, period or hybrid with its pairing, the key, the meter, the bars, the cadences, the
   accompaniment and the voicing, into *out (not out->style, out->motive...). Messages name the member. */
ComposerStatus api_grow_read_brief(const GrowBriefText *text, GrowSetup *out, ComposerError *error);

/*
 * A construction as a response gives it (R-468): {"halves", "ideas", "extensions", "reason", "text"}, `text` the sentence
 * that says why (grow_construction_describe). NULL when memory runs out.
 */
cJSON *api_grow_construction_json(const SectionConstruction *construction, const SectionPairing *pairing, const char *text);

/*
 * The refusal of a motif longer than the basic idea it fills, at path: "the motif is 3 bars; the basic idea of an 8-bar
 * theme is 2". The motif's bars are those it takes from its start, a pickup included.
 */
ComposerStatus api_fail_motif_long(ComposerError *error, const char *path, const Motive *motive, const PlanMeter *meter,
                                   size_t theme_bars, size_t idea_bars);

#endif
