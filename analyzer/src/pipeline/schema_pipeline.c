#include "pipeline/schema_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_chain/analysis_chain.h>
#include <ursatz_chain/chain_inputs.h>

#include "galant-schemas/analysis/galant_analysis_input.h"
#include "galant-schemas/analysis/galant_analysis_match.h"
#include "galant-schemas/galant_framework.h"
#include "scale-harmonization/analysis/scale_analysis_detect.h"
#include "scale-harmonization/scale_direction.h"
#include "scale-harmonization/scale_framework.h"
#include "scale-harmonization/scale_patterns.h"
#include "galant-schemas/schema_function.h"
#include "scale-harmonization/sequence_kind.h"
#include "semantics/beat_strength.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_rational.h"
#include "time/time_signature.h"
#include "time/time_span.h"

/* The segments of the library's input (the chain's galant segments, built again by its own builder) and the range of
   chord entries each segment covers (parallel to list.items). */
typedef struct {
    ChainGalantSegments galant;
    size_t *first_entry;
    size_t *last_entry;
    bool have_galant;
} SchemaBuild;

static void build_destroy(SchemaBuild *build) {
    if (build->have_galant) {
        chain_galant_segments_destroy(&build->galant);
    }
    free(build->first_entry);
    free(build->last_entry);
    memset(build, 0, sizeof(*build));
}

/* a/b < c/d for positive denominators. */
static bool rational_less(long a, unsigned long b, long c, unsigned long d) {
    return (long long)a * (long long)d < (long long)c * (long long)b;
}

/* The chord entries each merged segment covers: those starting inside its span. entries are in time order. */
static bool entry_ranges(const AnalysisReport *report, SchemaBuild *build) {
    const GalantSegmentList *list = &build->galant.list;
    build->first_entry = calloc(list->count > 0 ? list->count : 1, sizeof(size_t));
    build->last_entry = calloc(list->count > 0 ? list->count : 1, sizeof(size_t));
    if (!build->first_entry || !build->last_entry) {
        return false;
    }
    size_t next = 0;
    for (size_t s = 0; s < list->count; s++) {
        const MusicalTime *start = time_span_start(list->items[s].span);
        const MusicalTime *end = time_span_end(list->items[s].span);
        long start_n = musical_time_numerator(start);
        unsigned long start_d = musical_time_denominator(start);
        long end_n = musical_time_numerator(end);
        unsigned long end_d = musical_time_denominator(end);
        while (next < report->entry_count &&
               rational_less(report->entries[next].time_numerator, report->entries[next].time_denominator, start_n,
                             start_d)) {
            next++;
        }
        if (next >= report->entry_count) {
            return false;
        }
        build->first_entry[s] = next;
        size_t last = next;
        while (last + 1 < report->entry_count &&
               rational_less(report->entries[last + 1].time_numerator, report->entries[last + 1].time_denominator,
                             end_n, end_d)) {
            last++;
        }
        build->last_entry[s] = last;
        next = last + 1;
    }
    return true;
}

/* The input segment a stage claim names (its id is "seg-<index>"). false when the id is not one of ours. */
static bool segment_of_id(const SchemaBuild *build, const char *id, size_t *out) {
    return sscanf(id, "seg-%zu", out) == 1 && *out < build->galant.list.count;
}

static bool copy_stage(const SchemaBuild *build, const GalantStageClaim *claim, AnalysisSchemaStage *out) {
    size_t segment = 0;
    if (!segment_of_id(build, claim->segment_id, &segment)) {
        return false;
    }
    out->first_entry = build->first_entry[segment];
    out->last_entry = build->last_entry[segment];
    out->has_melody = claim->has_melody != 0;
    out->melody_degree = claim->melody_degree;
    out->melody_alteration = claim->melody_alteration;
    out->bass_degree = claim->bass_degree;
    out->bass_alteration = claim->bass_alteration;
    out->bass_only = claim->bass_only != 0;
    out->credit = claim->credit;
    if (claim->has_second) {
        size_t second = 0;
        if (!segment_of_id(build, claim->second_segment_id, &second)) {
            return false;
        }
        out->has_second = true;
        out->second_first_entry = build->first_entry[second];
        out->second_last_entry = build->last_entry[second];
        out->second_degree = claim->second_degree;
        out->second_alteration = claim->second_alteration;
    }
    return true;
}

static bool copy_schema(const SchemaBuild *build, const GalantSchemaMatch *match, const char *framework,
                        AnalysisSchema *out) {
    const GalantSchemaClaim *claim = &match->claim;
    out->framework = framework;
    out->name = strdup(claim->name);
    out->function = schema_function_name((SchemaFunction)claim->function);
    out->stages = calloc(claim->stage_count > 0 ? (size_t)claim->stage_count : 1, sizeof(AnalysisSchemaStage));
    if (!out->name || !out->function || !out->stages) {
        return false;
    }
    for (int k = 0; k < claim->stage_count; k++) {
        if (!copy_stage(build, &claim->stages[k], &out->stages[k])) {
            return false;
        }
        out->stage_count++;
    }
    out->frame_degree = claim->frame_degree;
    out->frame_alteration = claim->frame_alteration;
    out->bass_only_stages = (size_t)claim->bass_only_stages;
    out->first_entry = build->first_entry[claim->first_segment];
    out->last_entry = build->last_entry[claim->last_segment];
    out->pair_share = claim->pair_share;
    out->metric_fit = claim->metric_fit;
    out->mean_uncertainty = claim->mean_uncertainty;
    out->confidence = claim->confidence;
    out->uncertainty = match->uncertainty;
    return true;
}

static bool copy_sequence_stage(const SchemaBuild *build, const ScaleStageClaim *claim, AnalysisSequenceStage *out) {
    size_t segment = 0;
    if (!segment_of_id(build, claim->segment_id, &segment)) {
        return false;
    }
    out->first_entry = build->first_entry[segment];
    out->last_entry = build->last_entry[segment];
    out->bass_degree = claim->bass_degree;
    out->bass_alteration = claim->bass_alteration;
    out->has_soprano = claim->has_soprano != 0;
    out->soprano_degree = claim->soprano_degree;
    out->soprano_alteration = claim->soprano_alteration;
    out->has_root = claim->has_root != 0;
    out->root_degree = claim->root_degree;
    out->root_alteration = claim->root_alteration;
    return true;
}

static bool copy_sequence(const SchemaBuild *build, const ScaleSequenceMatch *match, const char *framework,
                          AnalysisSequence *out) {
    const ScaleSequenceClaim *claim = &match->claim;
    out->framework = framework;
    out->kind = sequence_kind_name((SequenceKind)claim->kind);
    out->pattern = claim->pattern >= 0 ? scale_pattern_name((ScalePatternId)claim->pattern) : NULL;
    out->direction = claim->direction >= 0 ? scale_direction_name((ScaleDirection)claim->direction) : NULL;
    out->stages = calloc(claim->stage_count > 0 ? (size_t)claim->stage_count : 1, sizeof(AnalysisSequenceStage));
    if (!out->kind || !out->stages) {
        return false;
    }
    for (int k = 0; k < claim->stage_count; k++) {
        if (!copy_sequence_stage(build, &claim->stages[k], &out->stages[k])) {
            return false;
        }
        out->stage_count++;
    }
    out->rule_backed = claim->rule_backed != 0;
    out->unit_length = claim->unit_length;
    out->units = claim->units;
    out->transposition_steps = claim->transposition_steps;
    out->first_entry = build->first_entry[claim->first_segment];
    out->last_entry = build->last_entry[claim->last_segment];
    out->unit_share = claim->unit_share;
    out->soprano_bonus = claim->soprano_bonus;
    out->mean_uncertainty = claim->mean_uncertainty;
    out->confidence = claim->confidence;
    out->uncertainty = match->uncertainty;
    return true;
}

bool schema_stage_collect(const ImportedNotes *notes, const AnalysisChain *library, AnalysisReport *report) {
    size_t count = analysis_chain_segment_count(library);
    const GalantSchemaMatches *schemas = analysis_chain_schemas(library);
    const ScaleSequenceMatches *sequences = analysis_chain_sequences(library);
    const ChainSegments *segments = analysis_chain_segments(library);
    if (count == 0 || report->entry_count != count || report->key_region_count == 0 || !notes->meter.meter_map ||
        !schemas || !sequences || !segments || !analysis_chain_outer_voices(library) ||
        !analysis_chain_chord_inputs(library) || !analysis_chain_numerals(library)) {
        return true;
    }
    /* The matches name the chain's merged segments ("seg-<n>"); its builder gives them again, for their spans. */
    SchemaBuild build;
    memset(&build, 0, sizeof(build));
    ChainInputStatus built = chain_build_galant_segments(
        segments, count, notes->meter.meter_map, analysis_chain_outer_voices(library),
        analysis_chain_home_key(library), analysis_chain_chord_inputs(library), analysis_chain_numerals(library),
        &build.galant);
    if (built != CHAIN_INPUT_OK) {
        chain_galant_segments_destroy(&build.galant);
        return built != CHAIN_INPUT_ERR_ALLOC;
    }
    build.have_galant = true;
    bool ok = entry_ranges(report, &build);
    if (ok && schemas->count > 0) {
        report->schemas = calloc(schemas->count, sizeof(AnalysisSchema));
        ok = report->schemas != NULL;
        const char *framework = framework_name(galant_schemas_framework());
        for (size_t i = 0; ok && i < schemas->count; i++) {
            ok = copy_schema(&build, &schemas->items[i], framework, &report->schemas[i]);
            report->schema_count++;
        }
    }
    if (ok && sequences->count > 0) {
        report->sequences = calloc(sequences->count, sizeof(AnalysisSequence));
        ok = report->sequences != NULL;
        const char *framework = framework_name(scale_harmonization_framework());
        for (size_t i = 0; ok && i < sequences->count; i++) {
            ok = copy_sequence(&build, &sequences->items[i], framework, &report->sequences[i]);
            report->sequence_count++;
        }
    }
    build_destroy(&build);
    return ok;
}
