#include "pipeline/texture_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "standard-meter/segmentation_beat_grid.h"
#include "standard-texture/framework.h"
#include "standard-texture/analysis/texture_analysis_classify.h"
#include "standard-texture/analysis/texture_analysis_input.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/* The library's input and what keeps it alive: the spans it borrows and the ImportedNotes index of each input note. */
typedef struct {
    StandardTextureInput input;
    TimeSpan **spans;
    size_t span_count;
    size_t *note_of_input; /* ImportedNotes index of each note of the input, in input order */
    VoiceAssignment assignment;
} TextureBuild;

static void build_destroy(TextureBuild *build) {
    standard_texture_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    free(build->spans);
    free(build->note_of_input);
    voice_assignment_destroy(&build->assignment);
    memset(build, 0, sizeof(*build));
}

/* The span [onset, onset + duration) of a note, or NULL when it has no length (or on allocation failure; *failed is then
   set). */
static TimeSpan *note_span(const NoteEvent *event, bool *failed) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
    MusicalTime *release = NULL;
    if (musical_time_add(onset, note_event_duration(event), &release) != MUSICAL_TIME_OK) {
        *failed = true;
        return NULL;
    }
    TimeSpan *span = NULL;
    TimeSpanResult result = time_span_create(onset, release, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span);
    musical_time_destroy(release);
    *failed = result == TIME_SPAN_ERR_ALLOC;
    return result == TIME_SPAN_OK ? span : NULL;
}

/* Builds the input. false only on allocation failure; *declined is set when the separation or the library cannot take the
   piece. */
static bool build_input(const ImportedNotes *notes, TextureBuild *build, bool *declined) {
    *declined = false;
    VoiceSeparationResult separated =
        voice_separate(notes->events, notes->pitch_values, notes->count, notes->streams, notes->meter.meter_map,
                       standard_meter_segmentation_beat_grid, NULL, NULL, &build->assignment);
    if (separated != VOICE_SEPARATION_OK) {
        *declined = separated != VOICE_SEPARATION_ERR_ALLOC;
        return *declined;
    }
    if (standard_texture_input_init(&build->input, notes->meter.meter_map) != STANDARD_TEXTURE_ANALYSIS_OK) {
        return false;
    }
    size_t capacity = notes->count > 0 ? notes->count : 1;
    build->spans = calloc(capacity, sizeof(TimeSpan *));
    build->note_of_input = calloc(capacity, sizeof(size_t));
    if (!build->spans || !build->note_of_input) {
        return false;
    }
    for (size_t i = 0; i < notes->count; i++) {
        bool failed = false;
        TimeSpan *span = note_span(notes->events[i], &failed);
        if (failed) {
            return false;
        }
        if (!span) {
            continue;
        }
        build->spans[build->span_count] = span;
        const VoiceAssignmentEntry *entry = &build->assignment.entries[i];
        StandardTextureAnalysisStatus added = standard_texture_input_add_note(
            &build->input, entity_id_value(note_event_id(notes->events[i])), span,
            pitch_numeric_value_position_numerator(notes->pitch_values[i]), notes->streams[i], entry->voice,
            entry->confidence);
        if (added == STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC) {
            time_span_destroy(span);
            return false;
        }
        if (added != STANDARD_TEXTURE_ANALYSIS_OK) {
            time_span_destroy(span);
            continue;
        }
        build->note_of_input[build->span_count++] = i;
    }
    return true;
}

/* a < b for the rationals na/da and nb/db (denominators positive). */
static bool before(long long na, long long da, long long nb, long long db) {
    return na * db < nb * da;
}

/* The notes attacked in [start, end), in time order. NULL with *count 0 on allocation failure or when there are none. */
static size_t *notes_attacked_in(const ImportedNotes *notes, const TextureBuild *build, const StandardTexturePassageClaim *claim,
                                 size_t *count) {
    size_t *ids = malloc((build->span_count > 0 ? build->span_count : 1) * sizeof(size_t));
    *count = 0;
    if (!ids) {
        return NULL;
    }
    for (size_t i = 0; i < build->span_count; i++) {
        size_t note = build->note_of_input[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[note]));
        long long n = (long long)musical_time_numerator(onset);
        long long d = (long long)musical_time_denominator(onset);
        if (!before(n, d, claim->start_n, claim->start_d) && before(n, d, claim->end_n, claim->end_d)) {
            ids[(*count)++] = note;
        }
    }
    return ids;
}

static bool copy_passage(const ImportedNotes *notes, const TextureBuild *build, const StandardTexturePassage *source,
                         const char *framework, AnalysisTexturePassage *out) {
    const StandardTexturePassageClaim *claim = &source->claim;
    out->framework = framework;
    out->first_measure = claim->first_measure;
    out->last_measure = claim->last_measure;
    out->start_numerator = (long)claim->start_n;
    out->start_denominator = (unsigned long)claim->start_d;
    out->end_numerator = (long)claim->end_n;
    out->end_denominator = (unsigned long)claim->end_d;
    out->texture = standard_texture_name((StandardTexture)claim->texture);
    out->category = standard_texture_name((StandardTexture)claim->category);
    out->windows = claim->windows;
    out->unclassified_windows = claim->unclassified_windows;
    out->note_ids = notes_attacked_in(notes, build, claim, &out->note_id_count);
    if (!out->note_ids) {
        return false;
    }
    out->confidence = claim->confidence;
    out->texture_confidence = claim->texture_confidence;
    out->voice_confidence = claim->voice_confidence;
    out->voices_max = claim->voices_max;
    out->voices_mean = claim->voices_mean;
    out->synchrony = claim->synchrony;
    out->independence = claim->independence;
    out->prominence = claim->prominence;
    out->top_gap = claim->top_gap;
    out->register_span = claim->register_span;
    out->spacing = claim->spacing;
    out->density = claim->density;
    out->pattern = claim->pattern;
    out->uncertainty = source->uncertainty;
    return true;
}

bool texture_stage_collect(const ImportedNotes *notes, AnalysisReport *report) {
    if (notes->count == 0 || !notes->streams || !notes->meter.meter_map) {
        return true;
    }
    TextureBuild build;
    memset(&build, 0, sizeof(build));
    bool declined = false;
    if (!build_input(notes, &build, &declined)) {
        build_destroy(&build);
        return false;
    }
    if (declined || build.span_count == 0) {
        build_destroy(&build);
        return true;
    }
    StandardTextureAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    StandardTextureAnalysisStatus status = standard_texture_analyze(&build.input, NULL, &analysis);
    if (status != STANDARD_TEXTURE_ANALYSIS_OK) {
        build_destroy(&build);
        return status != STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    const char *framework = framework_name(standard_texture_framework());
    bool ok = true;
    if (analysis.passage_count > 0) {
        report->texture_passages = calloc(analysis.passage_count, sizeof(AnalysisTexturePassage));
        report->texture_changes = calloc(analysis.change_count > 0 ? analysis.change_count : 1, sizeof(AnalysisTextureChange));
        ok = report->texture_passages && report->texture_changes;
    }
    for (size_t i = 0; ok && i < analysis.passage_count; i++) {
        ok = copy_passage(notes, &build, &analysis.passages[i], framework, &report->texture_passages[i]);
        report->texture_passage_count = i + 1;
    }
    for (size_t i = 0; ok && i < analysis.change_count; i++) {
        const StandardTextureChangeClaim *claim = &analysis.changes[i].claim;
        report->texture_changes[i] = (AnalysisTextureChange){
            framework,
            (size_t)claim->from,
            (size_t)claim->to,
            claim->boundary_measure,
            standard_texture_name((StandardTexture)claim->from_texture),
            standard_texture_name((StandardTexture)claim->to_texture),
            claim->category_changed != 0,
            analysis.changes[i].uncertainty,
        };
        report->texture_change_count = i + 1;
    }
    standard_texture_analysis_destroy(&analysis);
    build_destroy(&build);
    return ok;
}
