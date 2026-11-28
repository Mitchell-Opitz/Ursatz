#ifndef URSATZ_COMPOSER_API_PROGRESSION_LAB_H
#define URSATZ_COMPOSER_API_PROGRESSION_LAB_H

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "util/job.h"

/*
 * The progression request with a `palette` (R-766, composer/docs/harmony-lab.md): pools of chords from one or two modes, spans, K5 progression
 * schemas placed and looped, given chords, brightness and tension shapes, a harmony-only accompaniment, and `measures` and `harmony` in the response.
 * composer_api_progression calls it when the request has a `palette`; the request is parsed and the knowledge checked already, and `out` is zeroed.
 */
UrsatzStatus api_progression_lab(const ComposerContext *context, const cJSON *request, const UrsatzJob *job, ComposerResponse *out);

#endif
