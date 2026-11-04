#include "pipeline/rhythm_pipeline.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pipeline/harmonic_rhythm_pipeline.h"
#include "pitch/pitch_numeric_value.h"
#include "standard-meter/analysis/meter_analysis_density.h"
#include "standard-meter/analysis/meter_analysis_hemiola.h"
#include "standard-meter/analysis/meter_analysis_hypermeter.h"
#include "standard-meter/analysis/meter_analysis_check.h"
#include "standard-meter/analysis/meter_analysis_input.h"
#include "standard-meter/analysis/meter_analysis_tuplet.h"
#include "standard-meter/analysis/meter_analysis_syncopation.h"
#include "standard-meter/borrowed_rules.h"
#include "semantics/analysis_note.h"
#include "standard-meter/framework.h"
#include "structure/structure.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/score_time.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "time/time_signature.h"
#include "time/time_span.h"
#include "time/tuplet.h"
#include "util/misc_util.h"

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
    bool *change = NULL;
    if (!harmonic_rhythm_change_flags(report->entries, report->entry_count, &change)) {
        return false;
    }
    for (size_t i = 1; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!change[i]) {
            continue;
        }
        const MusicalTime *time = keep_time(build, entry->time_numerator, entry->time_denominator);
        char id[48];
        snprintf(id, sizeof(id), "chord-change-%zu", i);
        if (!time || standard_meter_input_add_chord_change(&build->input, id, time) != STANDARD_METER_ANALYSIS_OK) {
            free(change);
            return false;
        }
    }
    free(change);
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
            const TimeSpan *chord = form->built.input.cadences[phrase->claim.cadence_index].arrival;
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
    Rational start;
    Rational end;
    *count = 0;
    size_t *ids = malloc((build->input.note_count + 1) * sizeof(size_t));
    if (!ids || !score_time_span(build->input.phrases[phrase].span, &start, &end)) {
        free(ids);
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] < 0 || (line >= 0 && build->lines[i] != line)) {
            continue;
        }
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        Rational at;
        if (rational_of_musical_time(onset, &at) != RATIONAL_OK ||
            !score_time_snap(at, config.onset_grid_denominator, config.onset_tolerance_denominator, &at, NULL)) {
            free(ids);
            *count = 0;
            return NULL;
        }
        if (rational_cmp(at, start) >= 0 && rational_cmp(at, end) < 0) {
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

/* ---- Syncopation ---- */

/* The library's StandardMeterLevel names. */
static const char *level_name(int level) {
    static const char *const NAMES[] = {"downbeat", "secondary-beat", "beat", "offbeat", "finer"};
    return level >= 0 && level < 5 ? NAMES[level] : "finer";
}

static bool collect_syncopations(const RhythmBuild *build, AnalysisReport *report) {
    StandardMeterSyncopations found = {NULL, 0};
    if (standard_meter_find_syncopations(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisSyncopation *items = calloc(count > 0 ? count : 1, sizeof(AnalysisSyncopation));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterSyncopationClaim *claim = &found.items[i].claim;
        AnalysisSyncopation *item = &items[i];
        item->framework = framework;
        item->note = note_of_id(build, claim->note_id);
        ok = item->note != SIZE_MAX;
        item->line = claim->line;
        item->measure = claim->measure;
        item->beat = claim->beat;
        item->attack_level = level_name(claim->attack_level);
        item->through_level = level_name(claim->through_level);
        item->strength = claim->strength;
        item->through_measure = claim->through_measure;
        item->through_beat = claim->through_beat;
        item->uncertainty = found.items[i].uncertainty;
    }
    standard_meter_syncopations_destroy(&found);
    if (!ok) {
        free(items);
        return false;
    }
    report->syncopations = items;
    report->syncopation_count = count;
    return true;
}

/* ---- Hemiola ---- */

static void free_hemiolas(AnalysisHemiola *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i].note_ids);
    }
    free(items);
}

static bool collect_hemiolas(const RhythmBuild *build, AnalysisReport *report) {
    StandardMeterHemiolas found = {NULL, 0};
    if (standard_meter_find_hemiolas(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisHemiola *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHemiola));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterHemiola *hemiola = &found.items[i];
        AnalysisHemiola *item = &items[i];
        item->framework = framework;
        item->first_measure = hemiola->claim.first_measure;
        item->last_measure = hemiola->claim.last_measure;
        item->duration_evidence = hemiola->claim.duration_evidence;
        item->chord_evidence = hemiola->claim.chord_evidence;
        item->accent_evidence = hemiola->claim.accent_evidence;
        item->agreeing = hemiola->claim.agreeing;
        item->available = hemiola->claim.available;
        item->uncertainty = hemiola->uncertainty;
        size_t ids = structure_event_id_count(hemiola->shell);
        item->note_ids = malloc((ids > 0 ? ids : 1) * sizeof(size_t));
        ok = item->note_ids != NULL;
        for (size_t n = 0; ok && n < ids; n++) {
            size_t note = note_of_id(build, entity_id_value(structure_event_id_at(hemiola->shell, n)));
            if (note != SIZE_MAX) {
                item->note_ids[item->note_id_count++] = note;
            }
        }
        if (ok && item->note_id_count > 1) {
            qsort(item->note_ids, item->note_id_count, sizeof(size_t), size_compare);
        }
    }
    standard_meter_hemiolas_destroy(&found);
    if (!ok) {
        free_hemiolas(items, count);
        return false;
    }
    report->hemiolas = items;
    report->hemiola_count = count;
    return true;
}

/* ---- Hypermeter ---- */

static const char *hypermeasure_kind_name(int kind) {
    switch (kind) {
        case STANDARD_METER_HYPERMEASURE_SHORT:
            return "short";
        case STANDARD_METER_HYPERMEASURE_EXPANDED:
            return "expanded";
        case STANDARD_METER_HYPERMEASURE_EXTENSION:
            return "extension";
        case STANDARD_METER_HYPERMEASURE_CONTRACTED:
            return "contracted";
        default:
            return "regular";
    }
}

static void free_hypermeters(AnalysisHypermeter *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i].hypermeasures);
        free(items[i].note_ids);
    }
    free(items);
}

static bool collect_hypermeter(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    if (standard_meter_find_hypermeter(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.phrase_count;
    AnalysisHypermeter *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHypermeter));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterPhraseHypermeter *phrase = &found.phrases[i];
        AnalysisHypermeter *item = &items[i];
        item->framework = framework;
        item->phrase = (size_t)phrase->phrase;
        item->first_measure = phrase->first_measure;
        item->measures = phrase->measures;
        item->length = phrase->length;
        item->score = phrase->score;
        item->has_arrival = phrase->arrival_measure >= 0;
        item->arrival_measure = phrase->arrival_measure;
        item->hypermeasures = calloc(phrase->hypermeasure_count > 0 ? phrase->hypermeasure_count : 1,
                                     sizeof(AnalysisHypermeasure));
        ok = item->hypermeasures != NULL;
        for (size_t h = 0; ok && h < phrase->hypermeasure_count; h++) {
            const StandardMeterHypermeasure *source = &found.hypermeasures[phrase->hypermeasure_first + h];
            AnalysisHypermeasure *target = &item->hypermeasures[h];
            target->index = source->claim.hypermeasure;
            target->first_measure = source->claim.first_measure;
            target->last_measure = source->claim.last_measure;
            target->measures = source->claim.measures;
            target->kind = hypermeasure_kind_name(source->claim.kind);
            target->extra_hyperbeats = source->claim.extra_hyperbeats;
            target->contains_cadence = source->claim.contains_cadence != 0;
            target->score = source->claim.score;
            target->uncertainty = source->uncertainty;
        }
        item->hypermeasure_count = phrase->hypermeasure_count;
        if (ok) {
            item->note_ids = phrase_notes(build, notes, item->phrase, -1, &item->note_id_count);
            ok = item->note_ids != NULL;
        }
    }
    standard_meter_hypermeter_destroy(&found);
    if (!ok) {
        free_hypermeters(items, count);
        return false;
    }
    report->hypermeters = items;
    report->hypermeter_count = count;
    return true;
}

/* ---- Tuplets and borrowed divisions ---- */

static const char *meter_type_name(MeterType type) {
    return type == METER_TYPE_SIMPLE ? "simple" : (type == METER_TYPE_COMPOUND ? "compound" : "unclassified");
}

static const char *tuplet_level_name(TupletLevel level) {
    return level == TUPLET_LEVEL_BEAT ? "beat" : (level == TUPLET_LEVEL_SUBDIVISION ? "subdivision" : "multi-beat");
}

/* Every note of the input, in import order, as the library's note set, and the tuplets the library finds in it. The
   library orders the notes of equal onset itself and does not need the set sorted; import order keeps the order of
   the notes of a tuplet that the report has always had. It reads all notes as one stream: a tuplet is a figure of the
   music, not of a line. */
static bool collect_tuplets(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    (void)notes;
    size_t count = build->input.note_count;
    AnalysisNote *set_notes = calloc(count + 1, sizeof(AnalysisNote));
    StandardMeterTuplets found = {NULL, 0};
    AnalysisTuplet *items = NULL;
    bool ok = set_notes != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterNote *note = &build->input.notes[i];
        set_notes[i] = (AnalysisNote){.note_id = note->id,
                                      .span = note->span,
                                      .midi = note->pitch >= 0 && note->pitch <= 127 ? (int)note->pitch
                                                                                     : ANALYSIS_PITCH_UNKNOWN,
                                      .track = ANALYSIS_INDEX_NONE,
                                      .voice = ANALYSIS_INDEX_NONE,
                                      .voice_confidence = 1.0};
    }
    if (ok) {
        AnalysisNoteSet set = {set_notes, count, build->input.map};
        ok = standard_meter_find_tuplets(&set, NULL, &found) == STANDARD_METER_ANALYSIS_OK;
    }
    if (ok && found.count > 0) {
        items = calloc(found.count, sizeof(AnalysisTuplet));
        ok = items != NULL;
    }
    for (size_t i = 0; ok && i < found.count; i++) {
        const StandardMeterTuplet *tuplet = &found.items[i];
        AnalysisTuplet *item = &items[i];
        item->note_ids = malloc((tuplet->note_count > 0 ? tuplet->note_count : 1) * sizeof(size_t));
        if (!item->note_ids) {
            ok = false;
            break;
        }
        for (size_t k = 0; k < tuplet->note_count; k++) {
            item->note_ids[k] = note_of_id(build, set_notes[tuplet->notes[k]].note_id);
        }
        item->note_id_count = tuplet->note_count;
        item->framework = framework_name(standard_meter_framework());
        item->actual = tuplet->actual;
        item->normal = tuplet->normal;
        item->name = tuplet->name;
        item->meter = meter_type_name(tuplet->meter);
        item->borrowed = tuplet->borrowed;
        item->has_level = tuplet->has_level;
        if (tuplet->has_level) {
            item->level = tuplet_level_name(tuplet->level);
            item->level_typical = tuplet->level_typical;
        }
        item->measure = tuplet->measure;
        item->beat = tuplet->beat;
        item->attacks = tuplet->attacks;
    }
    size_t tuplet_count = found.count;
    standard_meter_tuplets_destroy(&found);
    free(set_notes);
    if (!ok) {
        for (size_t i = 0; items && i < tuplet_count; i++) {
            free(items[i].note_ids);
        }
        free(items);
        return false;
    }
    report->tuplets = items;
    report->tuplet_count = tuplet_count;
    return true;
}

bool rhythm_stage_collect(const ImportedNotes *notes, const OuterVoiceLines *lines, const FormStage *form,
                          AnalysisReport *report) {
    const MeterMap *map = notes->meter.meter_map;
    if (!map || notes->count == 0 || !form->have_input || form->phrases.count == 0 ||
        form->phrases.count != report->phrase_count || lines->count == 0) {
        return true;
    }
    RhythmBuild build;
    memset(&build, 0, sizeof(build));
    if (standard_meter_input_init(&build.input, map) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    bool ok = build_notes(&build, notes, lines) && build_chord_changes(&build, report) && build_phrases(&build, form) &&
              collect_densities(&build, notes, report) && collect_syncopations(&build, report) &&
              collect_hemiolas(&build, report) && collect_hypermeter(&build, notes, report) &&
              collect_tuplets(&build, notes, report);
    build_destroy(&build);
    return ok;
}
