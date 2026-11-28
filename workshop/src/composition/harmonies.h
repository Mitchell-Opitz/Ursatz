#ifndef WORKSHOP_HARMONY_SERVICE_H
#define WORKSHOP_HARMONY_SERVICE_H

#include <common/status.h>

#include <stdbool.h>

#include <cJSON.h>

#include "base/last_error.h"
#include "compose_common.h"

/*
 * Harmony service (R-767): composes a harmony item into a new version with the Composer's progression request (R-766).
 *
 * A harmony version keeps its files beside its body: request.json (what the Composer read), plan.json, harmony.mid and harmony.musicxml. The body
 * is {uses, request} and, once composed, result {seed, harmony, measures, files}: the response's `harmony` (the chord texts and stages) and `measures`.
 * The Composer runs in process through composer_api_progression, on a job worker.
 */
UrsatzStatus workshop_harmony_compose(const char *id, bool has_seed, long long seed, bool update_uses, const UrsatzJob *job, cJSON **out_item,
                                      UrsatzError *error);

#endif
