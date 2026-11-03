#include <ursatz_analyzer/analysis_result.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_chain/analysis_chain.h>

#include "core/rational.h"
#include "interpretation/chord.h"
#include "interpretation/key.h"
#include "interpretation/texture.h"
#include "pipeline/analysis_result_internal.h"
#include "pipeline/chain.h"
#include "interpretation/claim_interpretation.h"
#include "standard-harmony/harmony_framework.h"
#include "standard-meter/framework.h"
#include "standard-meter/analysis/meter_analysis_check.h"

struct AnalyzerResult {
    AnalyzerPass pass;
    const Interpretation **claims[ANALYZER_AREA_COUNT]; /* borrowed from the chain, or owned below */
    size_t counts[ANALYZER_AREA_COUNT];
    size_t capacities[ANALYZER_AREA_COUNT];
    Interpretation **owned; /* the Interpretations made here: progressions, tuplets and the meter check */
    size_t owned_count;
    void *owner; /* what the pass borrows, released after it (a hearing's notes) */
    void (*owner_destroy)(void *);
    char *document_json; /* the rendered document of a hearing, or NULL */
};

/* The claim contents the Analyzer writes for its own results (no library owner builds them), through the kernel's
   claim helper (R-236). */
typedef struct {
    size_t length;      /* chords in the pattern */
    size_t occurrences; /* how often it occurs */
} ProgressionClaim;

typedef struct {
    unsigned actual;
    unsigned normal;
    int borrowed;
    int measure;
    unsigned attacks;
} TupletClaim;

#define PROGRESSION_CLAIM_TYPE "standard-harmony/progression"
#define TUPLET_CLAIM_TYPE "standard-meter/tuplet"

_Static_assert(ANALYZER_STAGE_ALL == (ANALYSIS_CHAIN_STAGE_ALL | ((1u << 20) - (1u << 12))),
               "ANALYZER_STAGE_ALL covers the chain stages and the Analyzer bits");

AnalyzerOptions analyzer_options_default(void) {
    AnalyzerOptions options = {ANALYZER_STAGE_ALL, NULL, NULL};
    return options;
}

static bool add_claim(AnalyzerResult *result, AnalyzerArea area, const Interpretation *claim) {
    if (!claim) {
        return true; /* a result without a claim (an unmatched chord, a segment without a numeral) */
    }
    if (result->counts[area] == result->capacities[area]) {
        size_t capacity = result->capacities[area] ? result->capacities[area] * 2 : 16;
        const Interpretation **grown = realloc(result->claims[area], capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        result->claims[area] = grown;
        result->capacities[area] = capacity;
    }
    result->claims[area][result->counts[area]++] = claim;
    return true;
}

/* Keeps an Interpretation the result made, and adds it as a claim. On false the interpretation is freed. */
static bool add_owned(AnalyzerResult *result, AnalyzerArea area, Interpretation *claim) {
    if (!claim) {
        return false;
    }
    Interpretation **grown = realloc(result->owned, (result->owned_count + 1) * sizeof(*grown));
    if (!grown) {
        interpretation_destroy(claim);
        return false;
    }
    result->owned = grown;
    result->owned[result->owned_count++] = claim;
    return add_claim(result, area, claim);
}

#define ADD_ALL(result, area, items, count, field)                        \
    do {                                                                  \
        for (size_t i_ = 0; (items) && i_ < (count); i_++) {              \
            if (!add_claim(result, area, (items)[i_].field)) {            \
                return false;                                             \
            }                                                             \
        }                                                                 \
    } while (0)

static bool collect_keys(AnalyzerResult *result, const AnalysisChain *chain) {
    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    for (size_t i = 0; regions && i < regions->region_count; i++) {
        if (!add_claim(result, ANALYZER_AREA_KEYS, key_interpretation(regions->regions[i].key))) {
            return false;
        }
    }
    return true;
}

static bool collect_chord_claims(AnalyzerResult *result, const AnalysisChain *chain) {
    const CommonPracticeWeightedResult *chords = analysis_chain_chords(chain);
    size_t count = analysis_chain_segment_count(chain);
    for (size_t i = 0; chords && i < count; i++) {
        if (chords[i].matched && chords[i].best_chord &&
            !add_claim(result, ANALYZER_AREA_CHORDS, chord_interpretation(chords[i].best_chord))) {
            return false;
        }
    }
    const StandardTonalityRomanResult *numerals = analysis_chain_numerals(chain);
    for (size_t i = 0; numerals && i < count; i++) {
        if (numerals[i].has_numeral && !add_claim(result, ANALYZER_AREA_NUMERALS, numerals[i].interpretation)) {
            return false;
        }
    }
    return true;
}

static bool collect_form(AnalyzerResult *result, const AnalysisChain *chain) {
    const HarmonyCadences *cadences = analysis_chain_cadences(chain);
    if (cadences) {
        ADD_ALL(result, ANALYZER_AREA_CADENCES, cadences->items, cadences->count, interpretation);
    }
    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    if (phrases) {
        ADD_ALL(result, ANALYZER_AREA_PHRASES, phrases->items, phrases->count, interpretation);
    }
    const StandardFormThemes *themes = analysis_chain_themes(chain);
    if (themes) {
        ADD_ALL(result, ANALYZER_AREA_THEMES, themes->items, themes->count, interpretation);
    }
    const StandardFormMotives *motives = analysis_chain_motives(chain);
    if (motives) {
        ADD_ALL(result, ANALYZER_AREA_MOTIVES, motives->families, motives->family_count, interpretation);
    }
    return true;
}

static bool collect_meter(AnalyzerResult *result, const AnalysisChain *chain) {
    const AnalysisChainMeter *meter = analysis_chain_meter(chain);
    if (!meter) {
        return true;
    }
    ADD_ALL(result, ANALYZER_AREA_METER, meter->syncopations.items, meter->syncopations.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_METER, meter->hemiolas.items, meter->hemiolas.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_METER, meter->hypermeter.hypermeasures, meter->hypermeter.hypermeasure_count,
            interpretation);
    ADD_ALL(result, ANALYZER_AREA_METER, meter->hypermeter.reinterpretations, meter->hypermeter.reinterpretation_count,
            interpretation);
    ADD_ALL(result, ANALYZER_AREA_METER, meter->densities.items, meter->densities.count, interpretation);
    return true;
}

static bool collect_texture(AnalyzerResult *result, const AnalysisChain *chain) {
    const StandardTextureAnalysis *texture = analysis_chain_texture(chain);
    for (size_t i = 0; texture && i < texture->passage_count; i++) {
        if (!add_claim(result, ANALYZER_AREA_TEXTURE, texture_interpretation(texture->passages[i].texture))) {
            return false;
        }
    }
    if (texture) {
        ADD_ALL(result, ANALYZER_AREA_TEXTURE, texture->changes, texture->change_count, interpretation);
    }
    return true;
}

static bool collect_voice_leading(AnalyzerResult *result, const AnalysisChain *chain) {
    const AnalysisChainVoiceLeading *leading = analysis_chain_voice_leading(chain);
    if (!leading) {
        return true;
    }
    ADD_ALL(result, ANALYZER_AREA_VOICE_LEADING, leading->motions.items, leading->motions.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_VOICE_LEADING, leading->perfects.items, leading->perfects.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_VOICE_LEADING, leading->resolutions.items, leading->resolutions.count,
            interpretation);
    return true;
}

static bool collect_melody(AnalyzerResult *result, const AnalysisChain *chain) {
    const AnalysisChainMelody *melody = analysis_chain_melody(chain);
    if (!melody) {
        return true;
    }
    ADD_ALL(result, ANALYZER_AREA_MELODY, melody->intervals.items, melody->intervals.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_MELODY, melody->summaries.items, melody->summaries.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_MELODY, melody->climaxes.items, melody->climaxes.count, interpretation);
    ADD_ALL(result, ANALYZER_AREA_MELODY, melody->recoveries.items, melody->recoveries.count, interpretation);
    return true;
}

static bool collect_schemas(AnalyzerResult *result, const AnalysisChain *chain) {
    const GalantSchemaMatches *schemas = analysis_chain_schemas(chain);
    if (schemas) {
        ADD_ALL(result, ANALYZER_AREA_SCHEMAS, schemas->items, schemas->count, interpretation);
    }
    const ScaleSequenceMatches *sequences = analysis_chain_sequences(chain);
    if (sequences) {
        ADD_ALL(result, ANALYZER_AREA_SCHEMAS, sequences->items, sequences->count, interpretation);
    }
    return true;
}

static StandardMeterReading meter_reading_of(const AnalysisMeterReading *reading) {
    StandardMeterReading made = {(unsigned)reading->beats, (unsigned)reading->beat_type, rational_ratio(1, 1),
                                 rational_ratio(0, 1), reading->score};
    made.scale = rational_ratio(reading->scale_numerator, (long long)reading->scale_denominator);
    made.pickup = rational_ratio(reading->pickup_numerator, (long long)reading->pickup_denominator);
    return made;
}

/* The meter check, as the library writes it, targeting the piece's first chord segment (the claim has no notes). */
static bool collect_meter_check(AnalyzerResult *result) {
    const AnalysisMeterCheck *found = &result->pass.report.meter_check;
    if (!found->present || result->pass.segmentation.segment_count == 0) {
        return true;
    }
    StandardMeterCheck check;
    memset(&check, 0, sizeof(check));
    check.present = true;
    check.effective = meter_reading_of(&found->effective);
    check.has_other = found->has_other;
    if (found->has_other) {
        check.other = meter_reading_of(&found->other);
    }
    check.has_alternative = found->has_alternative;
    check.uncertainty = found->uncertainty;
    return add_owned(result, ANALYZER_AREA_METER,
                     standard_meter_check_interpretation(&check, result->pass.segmentation.segment_ids[0]));
}

static bool collect_progressions(AnalyzerResult *result) {
    const AnalysisReport *report = &result->pass.report;
    const Segmentation *segmentation = &result->pass.segmentation;
    for (size_t i = 0; i < report->progression_count; i++) {
        const AnalysisProgression *progression = &report->progressions[i];
        if (progression->occurrence_count == 0 || progression->occurrences[0].first_entry >= segmentation->segment_count) {
            continue;
        }
        char id[64];
        snprintf(id, sizeof(id), "analyzer-progression-%zu", i);
        ProgressionClaim claim = {progression->length, progression->occurrence_count};
        /* The patterns are counted by the Analyzer over the library's numerals: the claim is the harmony framework's,
           with no uncertainty of its own. */
        if (!add_owned(result, ANALYZER_AREA_PROGRESSIONS,
                       claim_interpretation_create(
                           entity_id_value(framework_id(standard_harmony_framework())), "standard-harmony", id,
                           segmentation->segment_ids[progression->occurrences[0].first_entry], PROGRESSION_CLAIM_TYPE,
                           &claim, sizeof(claim), 0.0, "analyzer-progression-count"))) {
            return false;
        }
    }
    return true;
}

static bool collect_tuplets(AnalyzerResult *result) {
    const AnalysisReport *report = &result->pass.report;
    const ImportedNotes *notes = result->pass.notes;
    for (size_t i = 0; i < report->tuplet_count; i++) {
        const AnalysisTuplet *tuplet = &report->tuplets[i];
        if (tuplet->note_id_count == 0 || tuplet->note_ids[0] >= notes->count) {
            continue;
        }
        char id[64];
        snprintf(id, sizeof(id), "analyzer-tuplet-%zu", i);
        TupletClaim claim = {tuplet->actual, tuplet->normal, tuplet->borrowed ? 1 : 0, tuplet->measure,
                             tuplet->attacks};
        const char *target = entity_id_value(note_event_id(notes->events[tuplet->note_ids[0]]));
        if (!add_owned(result, ANALYZER_AREA_RHYTHM,
                       claim_interpretation_create(entity_id_value(framework_id(standard_meter_framework())),
                                                   "standard-meter", id, target, TUPLET_CLAIM_TYPE, &claim,
                                                   sizeof(claim), 0.0, "analyzer-tuplet-runs"))) {
            return false;
        }
    }
    return true;
}

static bool collect_claims(AnalyzerResult *result) {
    const AnalysisChain *chain = result->pass.library;
    if (!chain) {
        return true;
    }
    bool full = (result->pass.options.stages & ANALYSIS_CHAIN_STAGE_ALL) == ANALYSIS_CHAIN_STAGE_ALL;
    bool ok = collect_keys(result, chain) && collect_chord_claims(result, chain) && collect_form(result, chain) &&
              collect_meter(result, chain) && collect_texture(result, chain) &&
              collect_voice_leading(result, chain) && collect_melody(result, chain) &&
              collect_schemas(result, chain);
    if (ok && full) {
        ok = collect_meter_check(result) && collect_progressions(result) && collect_tuplets(result);
    }
    return ok;
}

void analyzer_result_destroy(AnalyzerResult *result) {
    if (!result) {
        return;
    }
    for (int area = 0; area < ANALYZER_AREA_COUNT; area++) {
        free(result->claims[area]);
    }
    for (size_t i = 0; i < result->owned_count; i++) {
        interpretation_destroy(result->owned[i]);
    }
    free(result->owned);
    analyzer_pass_destroy(&result->pass);
    if (result->owner_destroy) {
        result->owner_destroy(result->owner);
    }
    free(result->document_json);
    free(result);
}

AnalyzerStatus analyzer_analyze(const ImportedNotes *notes, const AnalyzerOptions *options, AnalyzerResult **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerOptions used = options ? *options : analyzer_options_default();
    if (!notes || (used.stages & ANALYSIS_CHAIN_STAGE_ALL) == 0) {
        return ANALYZER_ERR_ARGUMENT;
    }
    AnalyzerResult *result = calloc(1, sizeof(*result));
    if (!result) {
        return ANALYZER_ERR_ALLOC;
    }
    if (!analyzer_pass_run(&result->pass, notes, &used)) {
        AnalyzerStatus status = result->pass.cancelled ? ANALYZER_ERR_CANCELLED
                                                       : (result->pass.library || notes->count == 0
                                                              ? ANALYZER_ERR_ALLOC
                                                              : ANALYZER_ERR_LIBRARY);
        analyzer_result_destroy(result);
        return status;
    }
    if (!collect_claims(result)) {
        analyzer_result_destroy(result);
        return ANALYZER_ERR_ALLOC;
    }
    *out = result;
    return ANALYZER_OK;
}

const AnalysisChain *analyzer_result_chain(const AnalyzerResult *result) {
    return result ? result->pass.library : NULL;
}

size_t analyzer_result_claim_count(const AnalyzerResult *result, AnalyzerArea area) {
    return result && area >= 0 && area < ANALYZER_AREA_COUNT ? result->counts[area] : 0;
}

const Interpretation *analyzer_result_claim(const AnalyzerResult *result, AnalyzerArea area, size_t index) {
    if (!result || area < 0 || area >= ANALYZER_AREA_COUNT || index >= result->counts[area]) {
        return NULL;
    }
    return result->claims[area][index];
}

const AnalysisReport *analyzer_result_report(const AnalyzerResult *result) {
    return result ? &result->pass.report : NULL;
}

AnalysisReport *analyzer_result_report_mutable(AnalyzerResult *result) {
    return result ? &result->pass.report : NULL;
}

const ImportedNotes *analyzer_result_notes(const AnalyzerResult *result) {
    return result ? result->pass.notes : NULL;
}

bool analyzer_result_report_complete(const AnalyzerResult *result) {
    return result && (result->pass.options.stages & ANALYSIS_CHAIN_STAGE_ALL) == ANALYSIS_CHAIN_STAGE_ALL;
}

bool analyzer_result_take_report(AnalyzerResult *result, AnalysisReport *report) {
    if (!result) {
        return false;
    }
    *report = result->pass.report;
    memset(&result->pass.report, 0, sizeof(result->pass.report));
    result->pass.report.home_duration_denominator = 1;
    return true;
}

void analyzer_result_set_owner(AnalyzerResult *result, void *owner, void (*owner_destroy)(void *)) {
    result->owner = owner;
    result->owner_destroy = owner_destroy;
}

void analyzer_result_set_document(AnalyzerResult *result, char *document_json) {
    free(result->document_json);
    result->document_json = document_json;
}

const char *analyzer_result_document_json(const AnalyzerResult *result) {
    return result ? result->document_json : NULL;
}
