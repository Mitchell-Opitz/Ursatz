#include "pipeline/voice_leading_pipeline.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "analysis_grid.h"
#include "analysis_motion.h"
#include "analysis_perfects.h"
#include "analysis_resolution.h"
#include "analysis_voices.h"
#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"
#include "pipeline/spelling_util.h"
#include "pipeline/voicing_check.h"
#include "segmentation_beat_grid.h"
#include "standard-counterpoint/framework.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "util/misc_util.h"

#define NO_NOTE SIZE_MAX

/* The grid and what keeps it alive. */
typedef struct {
    size_t voice_count;
    size_t row_count;
    VoiceAssignment assignment;
    StandardCounterpointVoiceCell *cells; /* row_count * voice_count */
    size_t *note_of_cell;                 /* ImportedNotes index of each cell's note, NO_NOTE where the voice rests */
    char (*names)[32];                    /* the rows' segment ids */
    const char **ids;
    StandardCounterpointVoiceGrid grid;
    HarmonyVoiceGrid harmony_grid;
    StandardCounterpointPreparedGrid prepared;
    bool prepared_ok;
} VoiceGrid;

static void grid_destroy(VoiceGrid *build) {
    if (build->prepared_ok) {
        standard_counterpoint_grid_destroy(&build->prepared);
    }
    voice_assignment_destroy(&build->assignment);
    free(build->cells);
    free(build->note_of_cell);
    free(build->names);
    free(build->ids);
    memset(build, 0, sizeof(*build));
}

/* The first entry that starts at or after n/d (entries are in time order); entry_count when there is none. */
static size_t first_entry_from(const AnalysisReport *report, long long n, long long d) {
    size_t low = 0;
    size_t high = report->entry_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const AnalysisReportEntry *entry = &report->entries[middle];
        if (fraction_compare(entry->time_numerator, (long long)entry->time_denominator, n, d) < 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

/* Puts one note into the cells of the rows it sounds at the start of. false only on allocation failure. */
static bool place_note(const ImportedNotes *notes, const AnalysisReport *report, VoiceGrid *build, size_t note) {
    const VoiceAssignmentEntry *assigned = &build->assignment.entries[note];
    if (assigned->voice >= build->voice_count) {
        return true;
    }
    const NoteEvent *event = notes->events[note];
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
    MusicalTime *release = NULL;
    if (musical_time_add(onset, note_event_duration(event), &release) != MUSICAL_TIME_OK) {
        return false;
    }
    long long on_n = (long long)musical_time_numerator(onset);
    long long on_d = (long long)musical_time_denominator(onset);
    long long off_n = (long long)musical_time_numerator(release);
    long long off_d = (long long)musical_time_denominator(release);
    musical_time_destroy(release);
    char name[16];
    const PitchSpelling *spelling =
        notes->source.spelling_notated &&
                spelled_note_name(event, pitch_numeric_value_position_numerator(notes->pitch_values[note]), true, name)
            ? note_event_optional_pitch_spelling(event)
            : NULL;
    for (size_t row = first_entry_from(report, on_n, on_d); row < report->entry_count; row++) {
        const AnalysisReportEntry *entry = &report->entries[row];
        if (fraction_compare(entry->time_numerator, (long long)entry->time_denominator, off_n, off_d) >= 0) {
            break;
        }
        size_t index = row * build->voice_count + assigned->voice;
        if (build->cells[index].present) {
            continue; /* an extra note in the same voice: the first stands */
        }
        double confidence = assigned->confidence < 0 ? 0 : (assigned->confidence > 1 ? 1 : assigned->confidence);
        build->cells[index] = (StandardCounterpointVoiceCell){
            true,
            notes->pitch_values[note],
            spelling,
            entity_id_value(note_event_id(event)),
            fraction_compare(entry->time_numerator, (long long)entry->time_denominator, on_n, on_d) == 0,
            confidence,
        };
        build->note_of_cell[index] = note;
    }
    return true;
}

/* Builds and prepares the grid. false only on allocation failure; *declined is set when the separation or the library
   cannot take the piece. */
static bool grid_build(const ImportedNotes *notes, const AnalysisReport *report, VoiceGrid *build, bool *declined) {
    *declined = false;
    VoiceSeparationResult separated =
        voice_separate(notes->events, notes->pitch_values, notes->count, NULL, notes->meter.meter_map,
                       standard_meter_segmentation_beat_grid, NULL, NULL, &build->assignment);
    if (separated != VOICE_SEPARATION_OK) {
        *declined = separated != VOICE_SEPARATION_ERR_ALLOC;
        return *declined;
    }
    size_t voices = 0;
    for (size_t i = 0; i < notes->count; i++) {
        if (build->assignment.entries[i].voice + 1 > voices) {
            voices = build->assignment.entries[i].voice + 1;
        }
    }
    build->voice_count = voices > ANALYSIS_VOICE_LEADING_MAX_VOICES ? ANALYSIS_VOICE_LEADING_MAX_VOICES : voices;
    build->row_count = report->entry_count;
    if (build->voice_count == 0 || build->row_count == 0) {
        *declined = true;
        return true;
    }
    size_t cells = build->row_count * build->voice_count;
    build->cells = calloc(cells, sizeof(StandardCounterpointVoiceCell));
    build->note_of_cell = malloc(cells * sizeof(size_t));
    build->names = calloc(build->row_count, sizeof(build->names[0]));
    build->ids = calloc(build->row_count, sizeof(const char *));
    if (!build->cells || !build->note_of_cell || !build->names || !build->ids) {
        return false;
    }
    for (size_t i = 0; i < cells; i++) {
        build->note_of_cell[i] = NO_NOTE;
    }
    for (size_t row = 0; row < build->row_count; row++) {
        snprintf(build->names[row], sizeof(build->names[row]), "seg-%zu", row);
        build->ids[row] = build->names[row];
    }
    for (size_t note = 0; note < notes->count; note++) {
        if (!place_note(notes, report, build, note)) {
            return false;
        }
    }
    build->grid = (StandardCounterpointVoiceGrid){build->row_count, build->voice_count, build->ids, build->cells};
    build->harmony_grid = build->grid;
    StandardCounterpointGridStatus status = standard_counterpoint_grid_prepare(&build->grid, &build->prepared);
    if (status == STANDARD_COUNTERPOINT_GRID_ERR_ALLOC) {
        return false;
    }
    build->prepared_ok = status == STANDARD_COUNTERPOINT_GRID_OK;
    *declined = !build->prepared_ok;
    return true;
}

/* The motions, counted in total and per voice pair. false only on allocation failure. */
static bool collect_motions(const VoiceGrid *build, AnalysisVoiceLeading *out) {
    StandardCounterpointMotions motions;
    StandardCounterpointMotionStatus status = standard_counterpoint_classify_motions(&build->prepared, &motions);
    if (status != STANDARD_COUNTERPOINT_MOTION_OK) {
        return status != STANDARD_COUNTERPOINT_MOTION_ERR_ALLOC;
    }
    size_t voices = build->voice_count;
    out->pairs = calloc(voices * (voices > 0 ? voices - 1 : 0) / 2 + 1, sizeof(AnalysisVoicePairMotion));
    if (!out->pairs) {
        standard_counterpoint_motions_destroy(&motions);
        return false;
    }
    for (size_t upper = 0; upper < voices; upper++) {
        for (size_t lower = upper + 1; lower < voices; lower++) {
            out->pairs[out->pair_count++] = (AnalysisVoicePairMotion){.upper_voice = upper, .lower_voice = lower};
        }
    }
    double uncertainty = 0;
    for (size_t i = 0; i < motions.count; i++) {
        const StandardCounterpointMotion *motion = &motions.items[i];
        size_t pair = 0;
        for (size_t upper = 0; upper < motion->upper_voice; upper++) {
            pair += voices - 1 - upper;
        }
        pair += motion->lower_voice - motion->upper_voice - 1;
        out->pairs[pair].counts[motion->type]++;
        out->pairs[pair].total++;
        out->motion_totals[motion->type]++;
        uncertainty += motion->uncertainty;
    }
    out->motion_count = motions.count;
    out->motion_uncertainty = motions.count > 0 ? uncertainty / (double)motions.count : 0;
    standard_counterpoint_motions_destroy(&motions);
    return true;
}

/* The notes of a voice in two rows. */
static size_t note_at(const VoiceGrid *build, size_t row, size_t voice) {
    return build->note_of_cell[row * build->voice_count + voice];
}

/* The parallel and direct fifths and octaves. false only on allocation failure. */
static bool collect_perfects(const VoiceGrid *build, AnalysisVoiceLeading *out) {
    StandardCounterpointViolations perfects;
    StandardCounterpointPerfectsStatus status = standard_counterpoint_check_perfects(&build->prepared, &perfects);
    if (status != STANDARD_COUNTERPOINT_PERFECTS_OK) {
        return status != STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC;
    }
    out->violations = calloc(perfects.count > 0 ? perfects.count : 1, sizeof(AnalysisVoiceViolation));
    if (!out->violations) {
        standard_counterpoint_violations_destroy(&perfects);
        return false;
    }
    for (size_t i = 0; i < perfects.count; i++) {
        const StandardCounterpointViolation *found = &perfects.items[i];
        out->violations[i] = (AnalysisVoiceViolation){
            standard_counterpoint_violation_rule_name(found->rule),
            found->upper_voice,
            found->lower_voice,
            found->segment_from,
            found->segment_to,
            found->arrival_interval,
            {note_at(build, found->segment_from, found->upper_voice), note_at(build, found->segment_to, found->upper_voice),
             note_at(build, found->segment_from, found->lower_voice), note_at(build, found->segment_to, found->lower_voice)},
            found->uncertainty,
        };
    }
    out->violation_count = perfects.count;
    standard_counterpoint_violations_destroy(&perfects);
    return true;
}

/* The unresolved sevenths and leading tones, against the chords the harmony stage prepared (one per report entry).
   false only on allocation failure. */
static bool collect_resolutions(const VoiceGrid *build, const HarmonyStage *harmony, AnalysisVoiceLeading *out) {
    if (!harmony->segments || harmony->count != build->row_count) {
        return true;
    }
    HarmonyResolutions resolutions;
    HarmonyResolutionStatus status =
        harmony_analysis_check_resolutions(harmony->segments, harmony->count, &build->harmony_grid, NULL, &resolutions);
    if (status != HARMONY_RESOLUTION_OK) {
        return status != HARMONY_RESOLUTION_ERR_ALLOC;
    }
    out->resolutions = calloc(resolutions.count > 0 ? resolutions.count : 1, sizeof(AnalysisVoiceResolution));
    if (!out->resolutions) {
        harmony_resolutions_destroy(&resolutions);
        return false;
    }
    for (size_t i = 0; i < resolutions.count; i++) {
        const HarmonyResolutionResult *found = &resolutions.items[i];
        out->resolutions[i] = (AnalysisVoiceResolution){
            harmony_resolution_rule_name(found->rule),
            found->voice,
            found->segment_from,
            found->segment_to,
            {note_at(build, found->segment_from, found->voice), note_at(build, found->segment_to, found->voice)},
            found->uncertainty,
        };
    }
    out->resolution_count = resolutions.count;
    out->resolutions_checked = true;
    harmony_resolutions_destroy(&resolutions);
    return true;
}

/* The semitone of a sounding cell. */
static long cell_semitone(const VoiceGrid *build, size_t row, size_t voice) {
    const StandardCounterpointVoiceCell *cell = &build->cells[row * build->voice_count + voice];
    return (long)pitch_numeric_value_position_numerator(cell->pitch);
}

static int pitch_class(long semitone) {
    return (int)(((semitone % 12) + 12) % 12);
}

/* Fills the doubling and spacing of one row; false when the row has fewer than three voices or is not a chord the harmony
   stage read. */
static bool fill_chord(const VoiceGrid *build, const HarmonyStage *harmony, size_t row, AnalysisVoiceChord *chord) {
    const HarmonySegmentInput *input = &harmony->inputs[row];
    size_t sounding[ANALYSIS_VOICE_LEADING_MAX_VOICES];
    size_t count = 0;
    for (size_t voice = 0; voice < build->voice_count; voice++) {
        if (build->cells[row * build->voice_count + voice].present) {
            sounding[count++] = voice;
        }
    }
    if (count < 3 || input->degree <= 0 || input->interval_count == 0 || input->inversion < 0 ||
        (size_t)input->inversion >= input->interval_count || input->interval_count > ANALYSIS_CHORD_MEMBERS) {
        return false;
    }
    size_t bass = sounding[0];
    for (size_t i = 1; i < count; i++) {
        if (cell_semitone(build, row, sounding[i]) < cell_semitone(build, row, bass)) {
            bass = sounding[i];
        }
    }
    int root = pitch_class(pitch_class(cell_semitone(build, row, bass)) - input->intervals[input->inversion]);
    double least_confidence = 1;
    chord->entry = row;
    chord->voices_sounding = count;
    chord->doubled_member = -1;
    for (size_t i = 0; i < count; i++) {
        const StandardCounterpointVoiceCell *cell = &build->cells[row * build->voice_count + sounding[i]];
        int member = -1;
        for (size_t m = 0; m < input->interval_count; m++) {
            if (pitch_class(input->intervals[m]) == pitch_class(pitch_class(cell_semitone(build, row, sounding[i])) - root)) {
                member = (int)m;
                break;
            }
        }
        if (member < 0) {
            chord->non_chord_voices++;
        } else {
            chord->member_voices[member]++;
        }
        if (i + 1 < count) {
            chord->gaps[chord->gap_count++] =
                cell_semitone(build, row, sounding[i]) - cell_semitone(build, row, sounding[i + 1]);
        }
        chord->note_ids[chord->note_id_count++] = build->note_of_cell[row * build->voice_count + sounding[i]];
        least_confidence = cell->confidence < least_confidence ? cell->confidence : least_confidence;
    }
    size_t most = 1;
    for (size_t m = 0; m < ANALYSIS_CHORD_MEMBERS; m++) {
        if (chord->member_voices[m] > most) {
            most = chord->member_voices[m];
            chord->doubled_member = (int)m;
        }
    }
    chord->uncertainty = 1 - least_confidence;
    if (build->voice_count == 4 && count == 4) {
        const PitchSpelling *spellings[4];
        for (size_t voice = 0; voice < 4; voice++) {
            spellings[voice] = standard_counterpoint_grid_spelling(&build->prepared, row, voice);
        }
        int wide = voicing_first_wide_spacing(spellings);
        chord->spacing_judged = wide != VOICING_SPACING_UNJUDGED;
        chord->wide_voice = wide;
    } else {
        chord->wide_voice = VOICING_SPACING_VALID;
    }
    return true;
}

/* The doubling and spacing of every chord segment three or more voices sound. false only on allocation failure. */
static bool collect_chords(const VoiceGrid *build, const HarmonyStage *harmony, AnalysisVoiceLeading *out) {
    if (!harmony->inputs || harmony->count != build->row_count) {
        return true;
    }
    out->chords = calloc(build->row_count, sizeof(AnalysisVoiceChord));
    if (!out->chords) {
        return false;
    }
    for (size_t row = 0; row < build->row_count; row++) {
        AnalysisVoiceChord chord;
        memset(&chord, 0, sizeof(chord));
        if (fill_chord(build, harmony, row, &chord)) {
            out->chords[out->chord_count++] = chord;
        }
    }
    return true;
}

bool voice_leading_stage_collect(const ImportedNotes *notes, const HarmonyStage *harmony, AnalysisReport *report) {
    if (notes->count == 0 || report->entry_count == 0 || !notes->meter.meter_map) {
        return true;
    }
    VoiceGrid build;
    memset(&build, 0, sizeof(build));
    bool declined = false;
    if (!grid_build(notes, report, &build, &declined)) {
        grid_destroy(&build);
        return false;
    }
    if (declined) {
        grid_destroy(&build);
        return true;
    }
    AnalysisVoiceLeading *out = &report->voice_leading;
    out->present = true;
    out->framework = framework_name(standard_counterpoint_framework());
    out->voice_count = build.voice_count;
    out->row_count = build.row_count;
    bool ok = collect_motions(&build, out) && collect_perfects(&build, out) &&
               collect_resolutions(&build, harmony, out) && collect_chords(&build, harmony, out);
    grid_destroy(&build);
    return ok;
}
