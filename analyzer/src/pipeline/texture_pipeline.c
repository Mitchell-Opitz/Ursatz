#include "pipeline/texture_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "standard-texture/framework.h"
#include "standard-texture/analysis/texture_analysis_classify.h"
#include "theory/framework.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/* a < b for the rationals na/da and nb/db (denominators positive). */
static bool before(long long na, long long da, long long nb, long long db) {
    return na * db < nb * da;
}

/* The notes attacked in [start, end), in time order. NULL with *count 0 on allocation failure or when there are none. */
static size_t *notes_attacked_in(const ImportedNotes *notes, const StandardTexturePassageClaim *claim, size_t *count) {
    size_t *ids = malloc((notes->count > 0 ? notes->count : 1) * sizeof(size_t));
    *count = 0;
    if (!ids) {
        return NULL;
    }
    for (size_t note = 0; note < notes->count; note++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[note]));
        long long n = (long long)musical_time_numerator(onset);
        long long d = (long long)musical_time_denominator(onset);
        if (!before(n, d, claim->start_n, claim->start_d) && before(n, d, claim->end_n, claim->end_d)) {
            ids[(*count)++] = note;
        }
    }
    return ids;
}

static bool copy_passage(const ImportedNotes *notes, const StandardTexturePassage *source, const char *framework,
                         AnalysisTexturePassage *out) {
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
    out->note_ids = notes_attacked_in(notes, claim, &out->note_id_count);
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

bool texture_stage_collect(const ImportedNotes *notes, const AnalysisChain *library, AnalysisReport *report) {
    const StandardTextureAnalysis *analysis = analysis_chain_texture(library);
    if (notes->count == 0 || !analysis) {
        return true;
    }
    const char *framework = framework_name(standard_texture_framework());
    bool ok = true;
    if (analysis->passage_count > 0) {
        report->texture_passages = calloc(analysis->passage_count, sizeof(AnalysisTexturePassage));
        report->texture_changes =
            calloc(analysis->change_count > 0 ? analysis->change_count : 1, sizeof(AnalysisTextureChange));
        ok = report->texture_passages && report->texture_changes;
    }
    for (size_t i = 0; ok && i < analysis->passage_count; i++) {
        ok = copy_passage(notes, &analysis->passages[i], framework, &report->texture_passages[i]);
        report->texture_passage_count = i + 1;
    }
    for (size_t i = 0; ok && i < analysis->change_count; i++) {
        const StandardTextureChangeClaim *claim = &analysis->changes[i].claim;
        report->texture_changes[i] = (AnalysisTextureChange){
            framework,
            (size_t)claim->from,
            (size_t)claim->to,
            claim->boundary_measure,
            standard_texture_name((StandardTexture)claim->from_texture),
            standard_texture_name((StandardTexture)claim->to_texture),
            claim->category_changed != 0,
            analysis->changes[i].uncertainty,
        };
        report->texture_change_count = i + 1;
    }
    return ok;
}
