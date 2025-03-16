#include "pipeline/form_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony_cadence_rules.h"
#include "pipeline/form_boundary.h"
#include "pitch/pitch_numeric_value.h"
#include "time/musical_time.h"

#define CADENCE_ID_PREFIX "chain-"

/* A span [start, end) kept by the stage; NULL on failure. */
static const TimeSpan *keep_span(FormStage *stage, const MusicalTime *start, const MusicalTime *end) {
    if (stage->span_count == stage->span_capacity) {
        size_t capacity = stage->span_capacity == 0 ? 256 : stage->span_capacity * 2;
        TimeSpan **grown = realloc(stage->spans, capacity * sizeof(TimeSpan *));
        if (!grown) {
            return NULL;
        }
        stage->spans = grown;
        stage->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    stage->spans[stage->span_count++] = span;
    return span;
}

/* The position of the voice segment's member that is the soprano's note, or member_count when it is none. */
static size_t member_of(const Segment *segment, const OuterVoiceEntry *entry) {
    const char *id = entity_id_value(entry->note_id);
    for (size_t m = 0; m < segment->member_count; m++) {
        if (strcmp(segment->members[m].note_id, id) == 0) {
            return m;
        }
    }
    return segment->member_count;
}

static const TimeSpan *window_of(const Segmentation *segmentation, size_t i) {
    return sonority_time_window(segmentation->sonorities.items[i].sonority);
}

/* The soprano as notes of the input: consecutive voice segments with the same soprano note are one note. */
static bool add_melody(FormStage *stage, const ImportedNotes *notes, const Segmentation *voices,
                       const OuterVoiceLines *lines) {
    const char *current_id = NULL;
    const MusicalTime *current_start = NULL;
    const MusicalTime *current_end = NULL;
    const NoteEvent *current_event = NULL;
    long current_semitone = 0;
    for (size_t v = 0; v <= voices->segment_count; v++) {
        const OuterVoiceEntry *entry = v < voices->segment_count && v < lines->count ? &lines->soprano[v] : NULL;
        const Segment *segment = entry ? &voices->segments[v] : NULL;
        size_t member = entry && entry->present ? member_of(segment, entry) : 0;
        const char *id = entry && entry->present && member < segment->member_count ? entity_id_value(entry->note_id)
                                                                                   : NULL;
        if (current_id != NULL && (id == NULL || strcmp(id, current_id) != 0)) {
            const TimeSpan *span = keep_span(stage, current_start, current_end);
            if (!span || standard_form_input_add_note(&stage->input, current_id, current_event, span, NULL,
                                                      current_semitone) == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
                return false;
            }
            current_id = NULL;
        }
        if (id == NULL) {
            continue;
        }
        const TimeSpan *window = window_of(voices, v);
        if (current_id == NULL) {
            current_id = id;
            current_start = time_span_start(window);
            current_event = notes->events[segment->note_indices[member]];
            current_semitone = pitch_numeric_value_position_numerator(entry->pitch);
        }
        current_end = time_span_end(window);
    }
    return true;
}

static bool add_segments(FormStage *stage, const Segmentation *segmentation) {
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        StandardFormAnalysisStatus status = standard_form_input_add_segment(
            &stage->input, segmentation->segment_ids[i], window_of(segmentation, i), 0, 0);
        if (status == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
            return false;
        }
    }
    return true;
}

static bool add_cadences(FormStage *stage, const Segmentation *segmentation, const HarmonyStage *harmony) {
    for (size_t i = 0; i < harmony->cadences.count; i++) {
        const HarmonyCadenceResult *found = &harmony->cadences.items[i];
        const TimeSpan *first = window_of(segmentation, found->arrival_first);
        const TimeSpan *last = window_of(segmentation, found->arrival_last);
        const TimeSpan *span = keep_span(stage, time_span_start(first), time_span_end(last));
        char id[32];
        snprintf(id, sizeof(id), CADENCE_ID_PREFIX "%zu", i);
        if (!span || standard_form_input_add_cadence(&stage->input, id, span, found->type,
                                                     harmony_cadence_strength(found->type), found->post_cadential,
                                                     found->uncertainty) == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
            return false;
        }
    }
    return true;
}

bool form_stage_run(FormStage *stage, const ImportedNotes *notes, const Segmentation *voices,
                    const Segmentation *segmentation, const OuterVoiceLines *lines, const HarmonyStage *harmony) {
    memset(stage, 0, sizeof(*stage));
    if (!notes->meter.meter_map || segmentation->segment_count == 0 || lines->count == 0 ||
        lines->count != voices->segment_count) {
        return true; /* nothing to read phrases from */
    }
    standard_form_input_init(&stage->input, notes->meter.meter_map);
    stage->built = true;
    if (!add_melody(stage, notes, voices, lines) || !add_segments(stage, segmentation) ||
        !add_cadences(stage, segmentation, harmony)) {
        return false;
    }
    StandardFormAnalysisStatus status = standard_form_input_finish(&stage->input);
    if (status == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
        return false;
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        return true; /* the library declined the input */
    }
    status = standard_form_segment_phrases(&stage->input, NULL, form_boundary_detect, NULL, &stage->phrases);
    if (status == STANDARD_FORM_ANALYSIS_ERR_ALLOC) {
        return false;
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        return true; /* a time the meter cannot place, or the detector declined: no phrases */
    }
    status = standard_form_segment_ideas(&stage->input, &stage->phrases, NULL, &stage->segmentation);
    return status != STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

long form_stage_phrase_cadence(const FormStage *stage, size_t phrase) {
    if (phrase >= stage->phrases.count) {
        return -1;
    }
    int index = stage->phrases.items[phrase].claim.cadence_index;
    if (index < 0 || (size_t)index >= stage->input.cadence_count) {
        return -1;
    }
    /* The input sorts its cadences by arrival; the id names the harmony stage's cadence. */
    const char *id = stage->input.cadences[index].id;
    return strncmp(id, CADENCE_ID_PREFIX, strlen(CADENCE_ID_PREFIX)) == 0 ? atol(id + strlen(CADENCE_ID_PREFIX)) : -1;
}

/* a/b < c/d for positive denominators. */
static bool rational_less(long a, unsigned long b, long c, unsigned long d) {
    return (long long)a * (long long)d < (long long)c * (long long)b;
}

static const char *signal_name(int signal) {
    switch (signal) {
        case STANDARD_FORM_SIGNAL_CADENCE:
            return "cadence";
        case STANDARD_FORM_SIGNAL_DISCONTINUITY:
            return "discontinuity";
        default:
            return "none";
    }
}

bool form_stage_collect_phrases(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->phrases.count;
    if (count == 0) {
        return true;
    }
    report->phrases = calloc(count, sizeof(AnalysisPhrase));
    if (!report->phrases) {
        return false;
    }
    report->phrase_count = count;
    size_t next_entry = 0;
    for (size_t i = 0; i < count; i++) {
        const StandardFormPhrase *found = &stage->phrases.items[i];
        const StandardFormPhraseClaim *claim = &found->claim;
        AnalysisPhrase *phrase = &report->phrases[i];
        const MusicalTime *start = time_span_start(found->span);
        const MusicalTime *end = time_span_end(found->span);
        phrase->start_numerator = musical_time_numerator(start);
        phrase->start_denominator = musical_time_denominator(start);
        phrase->end_numerator = musical_time_numerator(end);
        phrase->end_denominator = musical_time_denominator(end);
        /* The chords starting inside the span: the entries are in time order and the phrases tile. */
        while (next_entry < report->entry_count &&
               rational_less(report->entries[next_entry].time_numerator, report->entries[next_entry].time_denominator,
                             phrase->start_numerator, phrase->start_denominator)) {
            next_entry++;
        }
        phrase->first_entry = next_entry;
        while (next_entry < report->entry_count &&
               rational_less(report->entries[next_entry].time_numerator, report->entries[next_entry].time_denominator,
                             phrase->end_numerator, phrase->end_denominator)) {
            next_entry++;
        }
        phrase->entry_count = next_entry - phrase->first_entry;
        phrase->first_measure = claim->first_measure;
        phrase->end_measure = claim->end_measure;
        phrase->last_measure = claim->last_measure;
        phrase->length_measures = claim->length_measures;
        phrase->extension_measures = claim->extension_measures;
        long cadence = form_stage_phrase_cadence(stage, i);
        phrase->has_cadence = cadence >= 0;
        phrase->cadence = cadence >= 0 ? (size_t)cadence : 0;
        phrase->signal = signal_name(claim->signal);
        phrase->uncertainty = found->uncertainty;
    }
    return true;
}

void form_stage_destroy(FormStage *stage) {
    if (!stage) {
        return;
    }
    standard_form_segmentation_destroy(&stage->segmentation);
    standard_form_phrases_destroy(&stage->phrases);
    if (stage->built) {
        standard_form_input_destroy(&stage->input);
    }
    for (size_t i = 0; i < stage->span_count; i++) {
        time_span_destroy(stage->spans[i]);
    }
    free(stage->spans);
    memset(stage, 0, sizeof(*stage));
}
