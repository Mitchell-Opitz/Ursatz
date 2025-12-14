#include "pipeline/schema_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "galant_analysis_input.h"
#include "galant_analysis_match.h"
#include "galant_framework.h"
#include "schema_function.h"
#include "semantics/beat_strength.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_signature.h"
#include "time/time_span.h"

/* The segments of the library's input and what keeps them alive: the spans a merged segment borrows, and the range of
   chord entries each segment covers (parallel to list.items). */
typedef struct {
    GalantSegmentList list;
    TimeSpan **spans;
    size_t span_count;
    size_t *first_entry;
    size_t *last_entry;
} SchemaBuild;

static void build_destroy(SchemaBuild *build) {
    galant_segments_destroy(&build->list);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    free(build->spans);
    free(build->first_entry);
    free(build->last_entry);
    memset(build, 0, sizeof(*build));
}

/* A degree of a key by the offset in semitones above its tonic, with its alteration against the major scale
   (galant_degree.h): 0 1 2 3 4 5 6 7 8 9 10 11 are 1 2- 2 3- 3 4 4+ 5 6- 6 7- 7, the same in major and minor. */
static GalantPitchDegree degree_of_offset(int semitones) {
    static const GalantPitchDegree TABLE[12] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0},
                                                {4, 1}, {5, 0},  {6, -1}, {6, 0}, {7, -1}, {7, 0}};
    return TABLE[((semitones % 12) + 12) % 12];
}

static double clamp_unit(double x) {
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

/* The pitch class of an outer voice that is sounding, false when it is absent, bridged or guarded. */
static bool outer_pitch_class(const AnalysisOuterNote *note, int *out) {
    if (note->status != ANALYSIS_OUTER_PRESENT) {
        return false;
    }
    *out = ((note->pitch % 12) + 12) % 12;
    return true;
}

/* The length of a beat at a time, in whole notes: the meter's unit, three of them in a compound meter (6, 9 or 12
   over 8 or smaller). A time the meter cannot read gets a quarter. */
static void beat_length(const MeterMap *map, const MusicalTime *time, long *numerator, unsigned long *denominator) {
    *numerator = 1;
    *denominator = 4;
    const MeterMapRegion *region = NULL;
    if (meter_map_region_at(map, time, &region) != METER_MAP_OK) {
        return;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (!signature || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return;
    }
    long n = duration_numerator(unit);
    unsigned long d = duration_denominator(unit);
    duration_destroy(unit);
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        n *= 3;
    }
    *numerator = n;
    *denominator = d;
}

/* True when two consecutive segments are one harmony: the same key, outer voice degrees and numeral. */
static bool same_harmony(const GalantSegment *a, const GalantSegment *b) {
    return strcmp(a->key_id, b->key_id) == 0 && a->has_bass == b->has_bass && a->has_soprano == b->has_soprano &&
           (!a->has_bass || galant_degree_equal(a->bass, b->bass)) &&
           (!a->has_soprano || galant_degree_equal(a->soprano, b->soprano)) && a->has_numeral == b->has_numeral &&
           (!a->has_numeral || (galant_degree_equal(a->root, b->root) && a->inversion == b->inversion));
}

/* The library's numeral of a chord as the input's: the root as a degree of the local key (an applied chord's sounding
   root, from the tonicized degree and the chord's degree), the quality, inversion and uncertainty. */
static void fill_numeral(GalantSegment *segment, const StandardTonalityRomanResult *result, bool minor) {
    const StandardTonalityRomanClaim *claim = &result->claim;
    int lowered =
        minor && (claim->degree == 3 || claim->degree == 6 || claim->degree == 7) && !result->reading.raised_leading_tone
            ? -1
            : 0;
    segment->has_numeral = true;
    segment->root = (GalantPitchDegree){claim->degree, lowered};
    if (claim->applied) {
        int target_lowered = minor && (claim->applied_target_degree == 3 || claim->applied_target_degree == 6 ||
                                       claim->applied_target_degree == 7)
                                 ? -1
                                 : 0;
        segment->root = galant_degree_plus((GalantPitchDegree){claim->applied_target_degree, target_lowered},
                                           (GalantPitchDegree){claim->degree, 0});
        segment->applied_to = claim->applied_target_degree;
    }
    segment->quality = claim->quality;
    segment->seventh = claim->seventh;
    segment->inversion = claim->inversion;
    segment->uncertainty = clamp_unit(result->uncertainty);
}

/* Appends a span (kept until the build is destroyed). NULL on allocation failure. */
static const TimeSpan *keep_span(SchemaBuild *build, const MusicalTime *start, const MusicalTime *end) {
    TimeSpan **grown = realloc(build->spans, (build->span_count + 1) * sizeof(TimeSpan *));
    if (!grown) {
        return NULL;
    }
    build->spans = grown;
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

/* Builds the library's input from the chord segments. false only on allocation or library failure. */
static bool build_segments(const ImportedNotes *notes, const Segmentation *segmentation, const RomanNumerals *romans,
                           const AnalysisReport *report, SchemaBuild *build) {
    galant_segments_init(&build->list);
    size_t count = segmentation->segment_count;
    build->first_entry = calloc(count, sizeof(size_t));
    build->last_entry = calloc(count, sizeof(size_t));
    if (!build->first_entry || !build->last_entry) {
        return false;
    }
    int home_pc = report->key_regions[report->home_region].tonic_pitch_class;
    const MeterMap *map = notes->meter.meter_map;
    for (size_t i = 0; i < count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!entry->in_region) {
            continue;
        }
        const TimeSpan *window = sonority_time_window(segmentation->sonorities.items[i].sonority);
        int tonic_pc = romans->inputs[i].tonic_pitch_class;
        bool minor = romans->inputs[i].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
        GalantSegment segment;
        memset(&segment, 0, sizeof(segment));
        snprintf(segment.id, sizeof(segment.id), "seg-%zu", build->list.count);
        snprintf(segment.key_id, sizeof(segment.key_id), "key-%d%s", tonic_pc, minor ? "m" : "");
        segment.span = window;
        segment.strength = beat_strength_at(map, time_span_start(window));
        beat_length(map, time_span_start(window), &segment.beat_numerator, &segment.beat_denominator);
        segment.key = degree_of_offset(tonic_pc - home_pc);
        int pitch_class = 0;
        if (outer_pitch_class(&entry->bass, &pitch_class)) {
            segment.has_bass = true;
            segment.bass = degree_of_offset(pitch_class - tonic_pc);
        }
        if (outer_pitch_class(&entry->soprano, &pitch_class)) {
            segment.has_soprano = true;
            segment.soprano = degree_of_offset(pitch_class - tonic_pc);
        }
        segment.uncertainty = 0.5;
        if (romans->results[i].has_numeral) {
            fill_numeral(&segment, &romans->results[i], minor);
        }
        /* a held chord continues the segment before it */
        if (build->list.count > 0 && build->last_entry[build->list.count - 1] + 1 == i) {
            GalantSegment *last = &build->list.items[build->list.count - 1];
            if (same_harmony(last, &segment)) {
                const TimeSpan *joined = keep_span(build, time_span_start(last->span), time_span_end(window));
                if (!joined) {
                    return false;
                }
                last->span = joined;
                last->uncertainty = last->uncertainty > segment.uncertainty ? last->uncertainty : segment.uncertainty;
                build->last_entry[build->list.count - 1] = i;
                continue;
            }
        }
        build->first_entry[build->list.count] = i;
        build->last_entry[build->list.count] = i;
        if (galant_segments_add(&build->list, &segment) != GALANT_ANALYSIS_OK) {
            return false;
        }
    }
    return true;
}

/* The input segment a stage claim names (its id is "seg-<index>"). false when the id is not one of ours. */
static bool segment_of_id(const SchemaBuild *build, const char *id, size_t *out) {
    return sscanf(id, "seg-%zu", out) == 1 && *out < build->list.count;
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

bool schema_stage_collect(const ImportedNotes *notes, const Segmentation *segmentation, const RomanNumerals *romans,
                          AnalysisReport *report) {
    size_t count = segmentation->segment_count;
    if (count == 0 || romans->count != count || report->entry_count != count || report->key_region_count == 0 ||
        !notes->meter.meter_map) {
        return true;
    }
    SchemaBuild build;
    memset(&build, 0, sizeof(build));
    GalantSchemaMatches schemas = {NULL, 0};
    bool ok = build_segments(notes, segmentation, romans, report, &build);
    ok = ok && galant_schema_match(&build.list, NULL, &schemas) == GALANT_ANALYSIS_OK;
    if (ok && schemas.count > 0) {
        report->schemas = calloc(schemas.count, sizeof(AnalysisSchema));
        ok = report->schemas != NULL;
        const char *framework = framework_name(galant_schemas_framework());
        for (size_t i = 0; ok && i < schemas.count; i++) {
            ok = copy_schema(&build, &schemas.items[i], framework, &report->schemas[i]);
            report->schema_count++;
        }
    }
    galant_schema_matches_destroy(&schemas);
    build_destroy(&build);
    return ok;
}
