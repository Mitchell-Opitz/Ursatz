#include "pipeline/rhythm_pipeline.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/harmonic_rhythm_pipeline.h"
#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"
#include "identity/entity_id.h"
#include "meter_analysis_density.h"
#include "meter_analysis_input.h"
#include "meter_analysis_time.h"
#include "theory/framework.h"
#include "standard-meter/framework.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/* The line of a note in the library's input: 0 the outer-voice soprano, 1 the bass, 2 any other. */
#define LINE_SOPRANO 0
#define LINE_BASS 1
#define LINE_OTHER 2

/* An id of the input and the ImportedNotes index of its note. */
typedef struct {
    const char *id;
    size_t note;
} IdIndex;

/* The library's input and what keeps it alive: the spans and times it borrows, and the index of each id. */
typedef struct {
    StandardMeterInput input;
    TimeSpan **spans;
    size_t span_count;
    size_t span_capacity;
    MusicalTime **times;
    size_t time_count;
    size_t time_capacity;
    IdIndex *ids; /* sorted by id; one per input note */
    size_t id_count;
    int *lines;   /* per ImportedNotes index: its line, or -1 for a note left out of the input */
} RhythmBuild;

static void build_destroy(RhythmBuild *build) {
    standard_meter_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    for (size_t i = 0; i < build->time_count; i++) {
        musical_time_destroy(build->times[i]);
    }
    free(build->spans);
    free(build->times);
    free(build->ids);
    free(build->lines);
    memset(build, 0, sizeof(*build));
}

/* A span [start, end) kept by the build, or NULL on failure. */
static const TimeSpan *keep_span(RhythmBuild *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        size_t capacity = build->span_capacity == 0 ? 512 : build->span_capacity * 2;
        TimeSpan **grown = realloc(build->spans, capacity * sizeof(TimeSpan *));
        if (!grown) {
            return NULL;
        }
        build->spans = grown;
        build->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

static int compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The ids of the notes the line takes, sorted. NULL with count 0 on failure; *out_count is the number of ids. */
static const char **line_ids(const OuterVoiceEntry *entries, size_t count, size_t *out_count) {
    const char **ids = malloc((count == 0 ? 1 : count) * sizeof(char *));
    if (!ids) {
        return NULL;
    }
    size_t n = 0;
    for (size_t s = 0; s < count; s++) {
        if (entries[s].present && entries[s].note_id != NULL) {
            ids[n++] = entity_id_value(entries[s].note_id);
        }
    }
    qsort(ids, n, sizeof(char *), compare_ids);
    *out_count = n;
    return ids;
}

static bool in_ids(const char **ids, size_t count, const char *id) {
    return bsearch(&id, ids, count, sizeof(char *), compare_ids) != NULL;
}

static int compare_id_index(const void *a, const void *b) {
    return strcmp(((const IdIndex *)a)->id, ((const IdIndex *)b)->id);
}

/* The input's notes: every imported note with a length, in import order. A note the library refuses as invalid is left
   out (its line stays -1). */
static bool build_notes(RhythmBuild *build, const ImportedNotes *notes, const OuterVoiceLines *lines) {
    size_t soprano_count = 0;
    size_t bass_count = 0;
    const char **soprano = line_ids(lines->soprano, lines->count, &soprano_count);
    const char **bass = line_ids(lines->bass, lines->count, &bass_count);
    bool ok = soprano && bass && (build->lines = malloc((notes->count + 1) * sizeof(int))) != NULL;
    for (size_t i = 0; ok && i < notes->count; i++) {
        build->lines[i] = -1;
        const NoteEvent *event = notes->events[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
        MusicalTime *release = NULL;
        if (musical_time_add(onset, note_event_duration(event), &release) != MUSICAL_TIME_OK) {
            ok = false;
            break;
        }
        const TimeSpan *span = keep_span(build, onset, release);
        musical_time_destroy(release);
        if (!span) {
            ok = false;
            break;
        }
        const char *id = entity_id_value(note_event_id(event));
        int line = in_ids(soprano, soprano_count, id) ? LINE_SOPRANO : (in_ids(bass, bass_count, id) ? LINE_BASS : LINE_OTHER);
        StandardMeterAnalysisStatus status = standard_meter_input_add_note(
            &build->input, id, span, pitch_numeric_value_position_numerator(notes->pitch_values[i]), line);
        if (status == STANDARD_METER_ANALYSIS_OK) {
            build->lines[i] = line;
        } else if (status != STANDARD_METER_ANALYSIS_ERR_INVALID) {
            ok = false;
        }
    }
    free(soprano);
    free(bass);
    if (!ok) {
        return false;
    }
    build->ids = malloc((build->input.note_count + 1) * sizeof(IdIndex));
    if (!build->ids) {
        return false;
    }
    size_t next = 0;
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] >= 0) {
            build->ids[next].id = build->input.notes[next].id;
            build->ids[next].note = i;
            next++;
        }
    }
    build->id_count = next;
    qsort(build->ids, build->id_count, sizeof(IdIndex), compare_id_index);
    return true;
}

/* The ImportedNotes index of the input note with the id, or SIZE_MAX with none. */
static size_t note_of_id(const RhythmBuild *build, const char *id) {
    IdIndex key = {id, 0};
    const IdIndex *found = bsearch(&key, build->ids, build->id_count, sizeof(IdIndex), compare_id_index);
    return found ? found->note : SIZE_MAX;
}

/* A copy of the time kept by the build, or NULL on failure. */
static const MusicalTime *keep_time(RhythmBuild *build, long numerator, unsigned long denominator) {
    if (build->time_count == build->time_capacity) {
        size_t capacity = build->time_capacity == 0 ? 128 : build->time_capacity * 2;
        MusicalTime **grown = realloc(build->times, capacity * sizeof(MusicalTime *));
        if (!grown) {
            return NULL;
        }
        build->times = grown;
        build->time_capacity = capacity;
    }
    MusicalTime *time = NULL;
    if (musical_time_create(numerator, denominator, &time) != MUSICAL_TIME_OK) {
        return NULL;
    }
    build->times[build->time_count++] = time;
    return time;
}

/* A chord change at the start of every chord segment whose harmony differs from the segment before it (the first
   segment is not one: nothing changes into it). */
static bool build_chord_changes(RhythmBuild *build, const AnalysisReport *report) {
    for (size_t i = 1; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!harmonic_rhythm_is_change(&report->entries[i - 1], entry)) {
            continue;
        }
        const MusicalTime *time = keep_time(build, entry->time_numerator, entry->time_denominator);
        char id[48];
        snprintf(id, sizeof(id), "chord-change-%zu", i);
        if (!time || standard_meter_input_add_chord_change(&build->input, id, time) != STANDARD_METER_ANALYSIS_OK) {
            return false;
        }
    }
    return true;
}

/* The form stage's phrases as the library's phrases: the span with its extension, the arrival chord of the cadence and
   the length change of the expansion that concerns the phrase. */
static bool build_phrases(RhythmBuild *build, const FormStage *form) {
    const StandardFormPhrases *phrases = &form->phrases;
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        const TimeSpan *span = keep_span(build, time_span_start(phrase->span), time_span_end(phrase->span));
        const TimeSpan *arrival = NULL;
        if (phrase->claim.cadence_index >= 0) {
            const TimeSpan *chord = form->input.cadences[phrase->claim.cadence_index].arrival;
            arrival = keep_span(build, time_span_start(chord), time_span_end(chord));
            if (!arrival) {
                return false;
            }
        }
        StandardMeterExpansionKind expansion = STANDARD_METER_EXPANSION_NONE;
        int measures = 0;
        for (size_t e = 0; e < form->expansions.count; e++) {
            const StandardFormExpansionClaim *claim = &form->expansions.items[e].claim;
            if (claim->phrase != phrase->claim.index) {
                continue;
            }
            switch (claim->kind) {
                case STANDARD_FORM_EXPANSION_INTERNAL:
                    expansion = STANDARD_METER_EXPANSION_INTERNAL;
                    break;
                case STANDARD_FORM_EXPANSION_EXTERNAL:
                    expansion = STANDARD_METER_EXPANSION_EXTERNAL;
                    break;
                default:
                    expansion = STANDARD_METER_EXPANSION_CONTRACTION;
                    break;
            }
            measures = claim->measures_changed;
            break;
        }
        char id[48];
        snprintf(id, sizeof(id), "phrase-%zu", i);
        if (!span || standard_meter_input_add_phrase(&build->input, id, span, arrival, expansion, measures, false) !=
                         STANDARD_METER_ANALYSIS_OK) {
            return false;
        }
    }
    return true;
}

/* ---- Density ---- */

/* The notes of phrase p on the line (-1: all), in time order: the notes whose onset, snapped as the library snaps it,
   is in the phrase's span. */
static size_t *phrase_notes(const RhythmBuild *build, const ImportedNotes *notes, size_t phrase, int line, size_t *count) {
    StandardMeterConfig config = standard_meter_config_default();
    StandardMeterQ start;
    StandardMeterQ end;
    *count = 0;
    size_t *ids = malloc((build->input.note_count + 1) * sizeof(size_t));
    if (!ids || !standard_meter_span_q(build->input.phrases[phrase].span, &start, &end)) {
        free(ids);
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] < 0 || (line >= 0 && build->lines[i] != line)) {
            continue;
        }
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        StandardMeterQ at = standard_meter_snap(&config, standard_meter_q_of_time(onset), false, NULL);
        if (standard_meter_q_compare(at, start) >= 0 && standard_meter_q_compare(at, end) < 0) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

static bool collect_densities(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    StandardMeterDensities densities = {NULL, 0};
    if (standard_meter_find_densities(&build->input, NULL, &densities) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = densities.count;
    AnalysisDensity *items = calloc(count > 0 ? count : 1, sizeof(AnalysisDensity));
    bool ok = items != NULL;
    const char *framework = framework_name(standard_meter_framework());
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterDensityClaim *claim = &densities.items[i].claim;
        AnalysisDensity *item = &items[i];
        item->phrase = (size_t)claim->phrase;
        item->line = claim->line;
        item->framework = framework;
        item->notes = claim->notes;
        item->attacks = claim->attacks;
        item->measures = claim->measures;
        item->attacks_per_beat_mean = claim->attacks_per_beat_mean;
        item->attacks_per_beat_max = claim->attacks_per_beat_max;
        item->mean_inter_onset = claim->mean_inter_onset;
        item->on_beat_share = claim->on_beat_share;
        item->off_beat_share = claim->off_beat_share;
        item->bin_count = claim->bin_count;
        item->bins_truncated = claim->bins_truncated != 0;
        for (unsigned b = 0; b < claim->bin_count; b++) {
            item->bins[b].numerator = claim->bins[b].numerator;
            item->bins[b].denominator = claim->bins[b].denominator;
            item->bins[b].count = claim->bins[b].count;
        }
        item->note_ids = phrase_notes(build, notes, item->phrase, item->line, &item->note_id_count);
        ok = item->note_ids != NULL;
    }
    standard_meter_densities_destroy(&densities);
    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free(items[i].note_ids);
        }
        free(items);
        return false;
    }
    report->densities = items;
    report->density_count = count;
    return true;
}

bool rhythm_stage_collect(const ImportedNotes *notes, const OuterVoiceLines *lines, const FormStage *form,
                          AnalysisReport *report) {
    const MeterMap *map = notes->meter.meter_map;
    if (!map || notes->count == 0 || !form->built || form->phrases.count == 0 ||
        form->phrases.count != report->phrase_count || lines->count == 0) {
        return true;
    }
    RhythmBuild build;
    memset(&build, 0, sizeof(build));
    if (standard_meter_input_init(&build.input, map) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    bool ok = build_notes(&build, notes, lines) && build_chord_changes(&build, report) && build_phrases(&build, form) &&
              collect_densities(&build, notes, report);
    build_destroy(&build);
    return ok;
}
