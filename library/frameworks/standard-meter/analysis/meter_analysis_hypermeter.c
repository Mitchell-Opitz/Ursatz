#include "meter_analysis_hypermeter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hypermeter_rules.h"
#include "identity/entity_id.h"
#include "meter_analysis_claim.h"
#include "meter_analysis_position.h"
#include "time/meter_arithmetic.h"
#include "time/score_time.h"

typedef struct {
    int measures;
    int kind;
} Seg;

static void measure_entity(int measure, char *out, size_t size) {
    snprintf(out, size, "standard-meter-measure-%d", measure);
}

/* The hypermeasures of a phrase read over `base` measures in lengths of `length`, then changed by the expansion. */
static size_t build_segments(unsigned base, unsigned length, StandardMeterExpansionKind expansion, int added, Seg *segments) {
    size_t count = 0;
    unsigned full = base / length;
    unsigned rest = base % length;
    for (unsigned k = 0; k < full; k++) {
        segments[count].measures = (int)length;
        segments[count].kind = STANDARD_METER_HYPERMEASURE_REGULAR;
        count++;
    }
    if (rest > 0) {
        segments[count].measures = (int)rest;
        segments[count].kind = STANDARD_METER_HYPERMEASURE_SHORT;
        count++;
    }
    if (count == 0 || added <= 0) {
        return count;
    }
    unsigned changed = 0;
    if (expansion == STANDARD_METER_EXPANSION_INTERNAL &&
        standard_meter_hypermeasure_extended_length((unsigned)segments[count - 1].measures, added, &changed)) {
        segments[count - 1].measures = (int)changed;
        segments[count - 1].kind = STANDARD_METER_HYPERMEASURE_EXPANDED;
    } else if (expansion == STANDARD_METER_EXPANSION_EXTERNAL) {
        segments[count].measures = added;
        segments[count].kind = STANDARD_METER_HYPERMEASURE_EXTENSION;
        count++;
    } else if (expansion == STANDARD_METER_EXPANSION_CONTRACTION &&
               standard_meter_hypermeasure_extended_length((unsigned)segments[count - 1].measures, -added, &changed)) {
        segments[count - 1].measures = (int)changed;
        segments[count - 1].kind = STANDARD_METER_HYPERMEASURE_CONTRACTED;
    }
    return count;
}

static bool grow_array(void **items, size_t *capacity, size_t count, size_t size) {
    if (count < *capacity) {
        return true;
    }
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    void *grown = realloc(*items, next * size);
    if (grown == NULL) {
        return false;
    }
    *items = grown;
    *capacity = next;
    return true;
}

typedef struct {
    size_t phrase_capacity;
    size_t hypermeasure_capacity;
    size_t reinterpretation_capacity;
} Capacities;

static StandardMeterAnalysisStatus emit_hypermeasure(StandardMeterHypermeter *found, Capacities *capacities,
                                                     const StandardMeterHypermeasureClaim *claim) {
    if (!grow_array((void **)&found->hypermeasures, &capacities->hypermeasure_capacity, found->hypermeasure_count,
                    sizeof(StandardMeterHypermeasure))) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterHypermeasure *item = &found->hypermeasures[found->hypermeasure_count];
    memset(item, 0, sizeof(*item));
    item->claim = *claim;
    item->uncertainty = 1.0 - claim->score;
    if (item->uncertainty < 0.0) {
        item->uncertainty = 0.0;
    }
    char key[64];
    char source[64];
    char target[64];
    char interpretation_id[160];
    snprintf(key, sizeof(key), "p%d-h%d", claim->phrase, claim->hypermeasure);
    measure_entity(claim->first_measure, source, sizeof(source));
    measure_entity(claim->last_measure, target, sizeof(target));
    item->relationship = standard_meter_analysis_relationship("hypermeasure", key, source, target, "hypermeasure");
    if (item->relationship == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    found->hypermeasure_count++; /* counted now: destroy frees the relationship even if the interpretation fails */
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-meter-hypermeasure-claim-%s", key);
    item->interpretation = standard_meter_analysis_interpretation(
        interpretation_id, entity_id_value(relationship_id(item->relationship)), STANDARD_METER_HYPERMEASURE_CLAIM_TYPE, claim,
        sizeof(*claim), item->uncertainty, "hypermeter-inference");
    return item->interpretation == NULL ? STANDARD_METER_ANALYSIS_ERR_ALLOC : STANDARD_METER_ANALYSIS_OK;
}

static StandardMeterAnalysisStatus read_phrase(const StandardMeterInput *input, size_t index, StandardMeterHypermeter *found,
                                               Capacities *capacities) {
    const StandardMeterPhrase *phrase = &input->phrases[index];
    Rational start;
    Rational end;
    int first = 0;
    int last = 0;
    int counted = meter_map_has_pickup(input->map) ? 1 : meter_map_first_measure_number(input->map);
    if (!score_time_span(phrase->span, &start, &end) || meter_map_measure_of(input->map, start, &first) != METER_MAP_OK ||
        meter_map_last_measure_touched(input->map, end, &last) != METER_MAP_OK) {
        return STANDARD_METER_ANALYSIS_OK;
    }
    if (first < counted) {
        first = counted;
    }
    if (last < first) {
        return STANDARD_METER_ANALYSIS_OK;
    }
    unsigned measures = (unsigned)(last - first + 1);

    bool elided = phrase->elided_with_next && index + 1 < input->phrase_count;
    int arrival = -1;
    if (!elided && phrase->cadence_arrival != NULL) {
        Rational arrival_start;
        int arrival_measure = 0;
        if (score_time_span(phrase->cadence_arrival, &arrival_start, NULL) &&
            meter_map_measure_of(input->map, arrival_start, &arrival_measure) == METER_MAP_OK && arrival_measure >= first &&
            arrival_measure <= last) {
            arrival = arrival_measure - first;
        }
    }

    StandardMeterExpansionKind expansion = phrase->expansion;
    int added = phrase->expansion_measures;
    unsigned base = measures;
    if (expansion == STANDARD_METER_EXPANSION_INTERNAL || expansion == STANDARD_METER_EXPANSION_EXTERNAL) {
        base = added > 0 && (unsigned)added < measures ? measures - (unsigned)added : measures;
        if (base == measures) {
            expansion = STANDARD_METER_EXPANSION_NONE;
        }
    } else if (expansion == STANDARD_METER_EXPANSION_CONTRACTION) {
        base = added > 0 ? measures + (unsigned)added : measures;
        if (added <= 0) {
            expansion = STANDARD_METER_EXPANSION_NONE;
        }
    }
    if (arrival >= (int)base) {
        arrival = (int)base - 1;
    }
    unsigned length = 0;
    double score = 0.0;
    if (!standard_meter_hypermeasure_choose(base, arrival, elided, &length, &score)) {
        return STANDARD_METER_ANALYSIS_OK;
    }

    Seg *segments = (Seg *)malloc((measures + 2) * sizeof(Seg));
    if (segments == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t count = build_segments(base, length, expansion, added, segments);
    if (!grow_array((void **)&found->phrases, &capacities->phrase_capacity, found->phrase_count,
                    sizeof(StandardMeterPhraseHypermeter))) {
        free(segments);
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterPhraseHypermeter *entry = &found->phrases[found->phrase_count++];
    entry->phrase = (int)index;
    entry->first_measure = first;
    entry->measures = (int)measures;
    entry->length = length;
    entry->score = score;
    entry->arrival_measure = arrival >= 0 ? first + arrival : -1;
    entry->hypermeasure_first = found->hypermeasure_count;
    entry->hypermeasure_count = 0;

    StandardMeterAnalysisStatus status = STANDARD_METER_ANALYSIS_OK;
    int at = first;
    for (size_t k = 0; status == STANDARD_METER_ANALYSIS_OK && k < count; k++) {
        StandardMeterHypermeasureClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.phrase = (int)index;
        claim.hypermeasure = (int)k;
        claim.first_measure = at;
        claim.measures = segments[k].measures;
        claim.last_measure = at + segments[k].measures - 1;
        claim.nominal_length = length;
        claim.kind = segments[k].kind;
        claim.extra_hyperbeats = segments[k].measures - (int)length;
        claim.contains_cadence = entry->arrival_measure >= claim.first_measure && entry->arrival_measure <= claim.last_measure;
        claim.score = score;
        status = emit_hypermeasure(found, capacities, &claim);
        at += segments[k].measures;
    }
    entry->hypermeasure_count = found->hypermeasure_count - entry->hypermeasure_first;
    free(segments);
    return status;
}

static StandardMeterAnalysisStatus read_elisions(const StandardMeterInput *input, StandardMeterHypermeter *found,
                                                 Capacities *capacities) {
    for (size_t p = 0; p + 1 < found->phrase_count; p++) {
        const StandardMeterPhraseHypermeter *ending = &found->phrases[p];
        const StandardMeterPhraseHypermeter *next = &found->phrases[p + 1];
        if (!input->phrases[ending->phrase].elided_with_next || next->phrase != ending->phrase + 1 ||
            ending->hypermeasure_count == 0) {
            continue;
        }
        const StandardMeterHypermeasure *last_hypermeasure =
            &found->hypermeasures[ending->hypermeasure_first + ending->hypermeasure_count - 1];
        StandardMeterReinterpretationClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.phrase = ending->phrase;
        claim.next_phrase = next->phrase;
        claim.measure = next->first_measure;
        if (!standard_meter_hypermetric_elision((unsigned)last_hypermeasure->claim.measures, &claim.count_in_ending,
                                                &claim.count_in_next)) {
            continue;
        }
        if (!grow_array((void **)&found->reinterpretations, &capacities->reinterpretation_capacity,
                        found->reinterpretation_count, sizeof(StandardMeterReinterpretation))) {
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
        StandardMeterReinterpretation *item = &found->reinterpretations[found->reinterpretation_count];
        memset(item, 0, sizeof(*item));
        item->claim = claim;
        item->uncertainty = 1.0 - ending->score;
        char key[64];
        snprintf(key, sizeof(key), "p%d-p%d", claim.phrase, claim.next_phrase);
        item->relationship = standard_meter_analysis_relationship("reinterpretation", key, input->phrases[claim.phrase].id,
                                                                  input->phrases[claim.next_phrase].id,
                                                                  "hypermetric-reinterpretation");
        if (item->relationship == NULL) {
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
        found->reinterpretation_count++;
        char interpretation_id[160];
        snprintf(interpretation_id, sizeof(interpretation_id), "standard-meter-reinterpretation-claim-%s", key);
        item->interpretation = standard_meter_analysis_interpretation(
            interpretation_id, entity_id_value(relationship_id(item->relationship)), STANDARD_METER_REINTERPRETATION_CLAIM_TYPE,
            &claim, sizeof(claim), item->uncertainty, "hypermeter-inference");
        if (item->interpretation == NULL) {
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
    }
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_find_hypermeter(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                           StandardMeterHypermeter *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (config != NULL && !standard_meter_config_valid(config)) {
        return STANDARD_METER_ANALYSIS_ERR_INVALID;
    }
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    Capacities capacities = {0, 0, 0};
    StandardMeterAnalysisStatus status = STANDARD_METER_ANALYSIS_OK;
    for (size_t i = 0; status == STANDARD_METER_ANALYSIS_OK && i < input->phrase_count; i++) {
        status = read_phrase(input, i, &found, &capacities);
    }
    if (status == STANDARD_METER_ANALYSIS_OK) {
        status = read_elisions(input, &found, &capacities);
    }
    if (status != STANDARD_METER_ANALYSIS_OK) {
        standard_meter_hypermeter_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_hypermeter_destroy(StandardMeterHypermeter *hypermeter) {
    if (hypermeter == NULL) {
        return;
    }
    for (size_t i = 0; i < hypermeter->hypermeasure_count; i++) {
        interpretation_destroy(hypermeter->hypermeasures[i].interpretation);
        relationship_destroy(hypermeter->hypermeasures[i].relationship);
    }
    for (size_t i = 0; i < hypermeter->reinterpretation_count; i++) {
        interpretation_destroy(hypermeter->reinterpretations[i].interpretation);
        relationship_destroy(hypermeter->reinterpretations[i].relationship);
    }
    free(hypermeter->phrases);
    free(hypermeter->hypermeasures);
    free(hypermeter->reinterpretations);
    memset(hypermeter, 0, sizeof(*hypermeter));
}
