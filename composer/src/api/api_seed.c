#include "api_grow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/vocabulary/texture_rules.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/theme_shape.h>

#include "api_internal.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"

#include "api_seed.h"

cJSON *api_seed_json(const CorpusSeed *seed) {
    char score[48];
    snprintf(score, sizeof score, "%lu/%lu", seed->score_numerator, seed->score_denominator);
    cJSON *object = cJSON_CreateObject();
    cJSON *start = cJSON_CreateObject();
    cJSON *end = cJSON_CreateObject();
    if (!object || !start || !end || !cJSON_AddStringToObject(object, "piece", seed->piece) ||
        !cJSON_AddNumberToObject(start, "measure", seed->start_measure) ||
        !cJSON_AddItemToObject(start, "beat", api_number_g(seed->start_beat)) ||
        !cJSON_AddNumberToObject(end, "measure", seed->end_measure) ||
        !cJSON_AddItemToObject(end, "beat", api_number_g(seed->end_beat)) || !cJSON_AddItemToObject(object, "start", start) ||
        !cJSON_AddItemToObject(object, "end", end) || !cJSON_AddStringToObject(object, "score", score)) {
        /* start and end are owned by the object once added; free what is not. */
        if (object && !cJSON_GetObjectItem(object, "start")) {
            cJSON_Delete(start);
        }
        if (object && !cJSON_GetObjectItem(object, "end")) {
            cJSON_Delete(end);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

UrsatzStatus api_seed_source(const CorpusSeed *seed, PlanSource *out, UrsatzError *error) {
    memset(out, 0, sizeof(*out));
    if (snprintf(out->piece, PLAN_SOURCE_PIECE_MAX, "%s", seed->piece) >= PLAN_SOURCE_PIECE_MAX) {
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "", "the source piece's path is too long to record");
    }
    out->start_measure = seed->start_measure;
    out->start_beat = seed->start_beat;
    out->end_measure = seed->end_measure;
    out->end_beat = seed->end_beat;
    out->score_numerator = seed->score_numerator;
    out->score_denominator = seed->score_denominator;
    return URSATZ_OK;
}

UrsatzStatus api_seed_motive(const KnowledgeSnapshot *knowledge, const CorpusPassages *passages, const PlanMeter *meter,
                               const SpelledKey *key, unsigned long seed, CorpusSeed *out, UrsatzError *error) {
    CorpusSeedResult result = corpus_seed_motive(knowledge, passages->items, passages->count, meter, key, seed, out);
    return result == CORPUS_SEED_OK ? URSATZ_OK
                                    : api_seed_failure(result, "a motive (it needs two or more notes in its first bar)", "/corpus_passages", error);
}

void api_seed_stale_notice(const CorpusSeed *seed, char *out, size_t size) {
    out[0] = '\0';
    if (seed->stale_documents > 0) {
        snprintf(out, size, "%zu stale documents (analyzed by another version; run the corpus again)", seed->stale_documents);
    }
}
