#include <stdlib.h>
#include <string.h>

#include "chain/analysis_chain.h"
#include "chain/chain_result.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/metrical_position.h"
#include "model/time/musical_time.h"
#include "model/time/time_signature.h"

/* The measure and beat of a time in whole notes on the score's meter; 0 and 0.0 when it cannot be placed. */
static void place(const MeterMap *map, const Rational *at, int *measure, double *beat) {
    *measure = 0;
    *beat = 0.0;
    MusicalTime time;
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    Duration unit;
    if (map && musical_time_make(at->num, at->den, &time) == MUSICAL_TIME_OK &&
        metrical_position_from_time(map, &time, &position) == METRICAL_POSITION_OK) {
        int m = metrical_position_measure(position);
        const Duration *offset = metrical_position_offset(position);
        if (metrical_position_reading_signature(map, m, &signature) == METRICAL_POSITION_OK &&
            time_signature_unit_duration(signature, &unit) == DURATION_OK && unit.value.num > 0) {
            *measure = m;
            *beat = 1.0 + ((double)offset->value.num / (double)offset->value.den) /
                              ((double)unit.value.num / (double)unit.value.den);
        }
    }
    metrical_position_destroy(position);
}

/* Appends one fault; the rule and the note ids are copied. false on allocation failure. */
static bool add_fault(const MeterMap *map, ChainVoiceFault **list, size_t *count, size_t *capacity, const char *rule,
                      bool hard, const Rational *at, const size_t *note_ids, size_t note_id_count) {
    if (*count == *capacity) {
        size_t grown = *capacity ? *capacity * 2 : 8;
        ChainVoiceFault *bigger = realloc(*list, grown * sizeof **list);
        if (!bigger) {
            return false;
        }
        *list = bigger;
        *capacity = grown;
    }
    ChainVoiceFault *fault = &(*list)[*count];
    *fault = (ChainVoiceFault){0};
    fault->rule = strdup(rule);
    fault->note_ids = malloc((note_id_count > 0 ? note_id_count : 1) * sizeof *fault->note_ids);
    if (!fault->rule || !fault->note_ids) {
        free(fault->rule);
        free(fault->note_ids);
        return false;
    }
    memcpy(fault->note_ids, note_ids, note_id_count * sizeof *note_ids);
    fault->note_id_count = note_id_count;
    fault->hard = hard;
    place(map, at, &fault->measure, &fault->beat);
    (*count)++;
    return true;
}

static const char *voice_rule_id(const AnalysisVoiceViolation *fault) {
    if (strcmp(fault->rule, "parallel-fifths") == 0 || strcmp(fault->rule, "parallel-octaves") == 0) {
        return fault->rule;
    }
    return fault->arrival_interval == 5 ? "direct-fifths" : "direct-octaves";
}

void chain_voice_faults_free(ChainVoiceFault *faults, size_t count) {
    for (size_t i = 0; faults && i < count; i++) {
        free(faults[i].rule);
        free(faults[i].note_ids);
    }
    free(faults);
}

bool chain_result_voice_faults(const AnalysisChain *chain, ChainVoiceFault **out, size_t *count) {
    if (!out || !count) {
        return false;
    }
    *out = NULL;
    *count = 0;
    const ImportedScore *score = chain ? analysis_chain_score(chain) : NULL;
    if (!score) {
        return false;
    }
    const ChainResult *report = analysis_chain_result(chain);
    const MeterMap *map = score->meter.meter_map;
    const AnalysisVoiceLeading *leading = &report->voice_leading;
    ChainVoiceFault *list = NULL;
    size_t used = 0, capacity = 0;
    bool ok = true;
    for (size_t i = 0; ok && leading->present && i < leading->violation_count; i++) {
        const AnalysisVoiceViolation *fault = &leading->violations[i];
        const ChainEntry *arrival = &report->entries[fault->entry_to];
        bool parallel = fault->rule[0] == 'p';
        ok = add_fault(map, &list, &used, &capacity, voice_rule_id(fault), parallel, &arrival->time, fault->note_ids, 4);
    }
    for (int pass = 0; ok && pass < 2; pass++) {
        const char *wanted = pass == 0 ? "seventh-resolves-down" : "leading-tone-resolves";
        for (size_t i = 0; ok && leading->present && i < leading->resolution_count; i++) {
            const AnalysisVoiceResolution *fault = &leading->resolutions[i];
            if (strcmp(fault->rule, wanted) != 0) {
                continue;
            }
            const ChainEntry *arrival = &report->entries[fault->entry_to];
            ok = add_fault(map, &list, &used, &capacity,
                           pass == 0 ? "unresolved-seventh" : "unresolved-leading-tone", false, &arrival->time,
                           fault->note_ids, 2);
        }
    }
    if (!ok) {
        chain_voice_faults_free(list, used);
        return false;
    }
    *out = list;
    *count = used;
    return true;
}
