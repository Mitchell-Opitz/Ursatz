#include "chain/chain_result_voice_leading.h"
#include "chain/analysis_chain_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voices/voice_separation.h"
#include "model/core/rational.h"
#include "model/events/note_event.h"
#include "chain/chain_result_spelling.h"
#include "chain/chain_result_voicing_check.h"
#include <chain/chain_inputs.h>

#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_numeric_value.h"
#include "analysis/counterpoint/analysis_grid.h"
#include "analysis/counterpoint/analysis_motion.h"
#include "analysis/counterpoint/analysis_perfects.h"
#include "analysis/harmony/analysis_resolution.h"
#include "theory/voicing/doubling_reading.h"
#include "analysis/harmony/analysis_voices.h"
#include "theory/meter/segmentation_beat_grid.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "theory/vocabulary/framework_names.h"

#define NO_NOTE SIZE_MAX

/* The chain's voice grid (borrowed), for the notes in its cells and the spellings of its rows, and what the stage
   builds on it. */
typedef struct {
    size_t voice_count;
    size_t row_count;
    ChainVoiceGrid grid;
    size_t *note_of_cell; /* ImportedScore index of each cell's note, NO_NOTE where the voice rests */
    const StandardCounterpointPreparedGrid *prepared; /* the chain's own (R-615), borrowed */
} VoiceGrid;

static void grid_destroy(VoiceGrid *build) {
    free(build->note_of_cell);
    memset(build, 0, sizeof(*build));
}

static const AnalysisVoiceCell *cell_of(const VoiceGrid *build, size_t row, size_t voice) {
    return &build->grid.grid.cells[row * build->voice_count + voice];
}

/* Takes the chain's grid and finds the note of each cell. false only on allocation failure; *declined is set when the
   library cannot take the piece. */
static bool grid_build(const ImportedScore *notes, const ChainScoreInput *input, const AnalysisChain *library,
                       VoiceGrid *build, bool *declined) {
    *declined = false;
    const ChainVoiceGrid *grid = analysis_chain_voice_grid(library);
    if (!grid) {
        *declined = true;
        return true;
    }
    build->grid = *grid;
    build->voice_count = build->grid.grid.voice_count;
    build->row_count = build->grid.grid.segment_count;
    size_t cells = build->row_count * build->voice_count;
    if (cells == 0) {
        *declined = true;
        return true;
    }
    build->note_of_cell = malloc(cells * sizeof(size_t));
    if (!build->note_of_cell) {
        return false;
    }
    for (size_t i = 0; i < cells; i++) {
        const AnalysisVoiceCell *cell = &build->grid.grid.cells[i];
        build->note_of_cell[i] = cell->present ? chain_score_input_import_index(input, cell->note_id) : NO_NOTE;
        if (cell->present && build->note_of_cell[i] == NO_NOTE) {
            return false;
        }
    }
    (void)notes;
    /* The chain prepared the grid for its own motions and perfects (analysis_chain.c): the stage reads it, it does not
       prepare it again. A run that had it declined would have failed there. */
    *declined = !library->have_prepared_grid;
    build->prepared = &library->prepared_grid;
    return true;
}

/* The motions, counted in total and per voice pair. false only on allocation failure. */
static bool collect_motions(const VoiceGrid *build, const StandardCounterpointMotions *motions,
                            AnalysisVoiceLeading *out) {
    size_t voices = build->voice_count;
    out->pairs = calloc(voices * (voices > 0 ? voices - 1 : 0) / 2 + 1, sizeof(AnalysisVoicePairMotion));
    if (!out->pairs) {
        return false;
    }
    for (size_t upper = 0; upper < voices; upper++) {
        for (size_t lower = upper + 1; lower < voices; lower++) {
            out->pairs[out->pair_count++] = (AnalysisVoicePairMotion){.upper_voice = upper, .lower_voice = lower};
        }
    }
    double uncertainty = 0;
    for (size_t i = 0; i < motions->count; i++) {
        const StandardCounterpointMotion *motion = &motions->items[i];
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
    out->motion_count = motions->count;
    out->motion_uncertainty = motions->count > 0 ? uncertainty / (double)motions->count : 0;
    return true;
}

/* The notes of a voice in two rows. */
static size_t note_at(const VoiceGrid *build, size_t row, size_t voice) {
    return build->note_of_cell[row * build->voice_count + voice];
}

/* The parallel and direct fifths and octaves. false only on allocation failure. */
static bool collect_perfects(const VoiceGrid *build, const StandardCounterpointViolations *perfects,
                             AnalysisVoiceLeading *out) {
    out->violations = calloc(perfects->count > 0 ? perfects->count : 1, sizeof(AnalysisVoiceViolation));
    if (!out->violations) {
        return false;
    }
    for (size_t i = 0; i < perfects->count; i++) {
        const StandardCounterpointViolation *found = &perfects->items[i];
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
    out->violation_count = perfects->count;
    return true;
}

/* The unresolved sevenths and leading tones, against the chords the chain prepared (one per report entry). false only
   on allocation failure. */
static bool collect_resolutions(const VoiceGrid *build, const HarmonyStage *harmony, const HarmonyResolutions *resolutions,
                                AnalysisVoiceLeading *out) {
    if (!harmony->segments || harmony->count != build->row_count) {
        return true;
    }
    out->resolutions = calloc(resolutions->count > 0 ? resolutions->count : 1, sizeof(AnalysisVoiceResolution));
    if (!out->resolutions) {
        return false;
    }
    for (size_t i = 0; i < resolutions->count; i++) {
        const HarmonyResolutionResult *found = &resolutions->items[i];
        out->resolutions[i] = (AnalysisVoiceResolution){
            harmony_resolution_rule_name(found->rule),
            found->voice,
            found->segment_from,
            found->segment_to,
            {note_at(build, found->segment_from, found->voice), note_at(build, found->segment_to, found->voice)},
            found->uncertainty,
        };
    }
    out->resolution_count = resolutions->count;
    return true;
}

/* The semitone of a sounding cell. */
static long cell_semitone(const VoiceGrid *build, size_t row, size_t voice) {
    return (long)pitch_numeric_value_position_numerator(cell_of(build, row, voice)->pitch);
}

/* Fills the doubling and spacing of one row; false when the row has fewer than three voices or is not a chord the harmony
   stage read. */
static bool fill_chord(const VoiceGrid *build, const HarmonyStage *harmony, size_t row, AnalysisVoiceChord *chord) {
    const HarmonySegmentInput *input = &harmony->inputs.inputs[row];
    size_t sounding[ANALYSIS_VOICE_LEADING_MAX_VOICES];
    size_t count = 0;
    for (size_t voice = 0; voice < build->voice_count && count < ANALYSIS_VOICE_LEADING_MAX_VOICES; voice++) {
        if (cell_of(build, row, voice)->present) {
            sounding[count++] = voice;
        }
    }
    if (count < 3 || input->degree <= 0 || input->interval_count == 0 || input->inversion < 0 ||
        (size_t)input->inversion >= input->interval_count || input->interval_count > ANALYSIS_CHORD_MEMBERS) {
        return false;
    }
    long semitones[ANALYSIS_VOICE_LEADING_MAX_VOICES];
    for (size_t i = 0; i < count; i++) {
        semitones[i] = cell_semitone(build, row, sounding[i]);
    }
    StandardVoicingDoublingReading reading;
    if (!standard_voicing_read_doubling(semitones, count, input->intervals, input->interval_count, input->inversion,
                                        &reading)) {
        return false;
    }
    double least_confidence = 1;
    chord->entry = row;
    chord->voices_sounding = reading.voices_sounding;
    chord->doubled_member = reading.doubled_member;
    chord->non_chord_voices = reading.non_chord_voices;
    memcpy(chord->member_voices, reading.member_voices, sizeof(chord->member_voices));
    chord->gap_count = reading.gap_count;
    memcpy(chord->gaps, reading.gaps, sizeof(reading.gaps));
    for (size_t i = 0; i < count; i++) {
        const AnalysisVoiceCell *cell = cell_of(build, row, sounding[i]);
        chord->note_ids[chord->note_id_count++] = build->note_of_cell[row * build->voice_count + sounding[i]];
        least_confidence = cell->confidence < least_confidence ? cell->confidence : least_confidence;
    }
    chord->uncertainty = 1 - least_confidence;
    if (build->voice_count == 4 && count == 4) {
        const PitchSpelling *spellings[4];
        for (size_t voice = 0; voice < 4; voice++) {
            spellings[voice] = standard_counterpoint_grid_spelling(build->prepared, row, voice);
        }
        int wide = chain_voicing_first_wide_spacing(spellings);
        chord->spacing_judged = wide != VOICING_SPACING_UNJUDGED;
        chord->wide_voice = wide;
    } else {
        chord->wide_voice = VOICING_SPACING_VALID;
    }
    return true;
}

/* The doubling and spacing of every chord segment three or more voices sound. false only on allocation failure. */
static bool collect_chords(const VoiceGrid *build, const HarmonyStage *harmony, AnalysisVoiceLeading *out) {
    if (!harmony->inputs.inputs || harmony->count != build->row_count) {
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

bool voice_leading_stage_collect(const ImportedScore *notes, const ChainScoreInput *input, const AnalysisChain *library,
                                 const HarmonyStage *harmony, ChainResult *report) {
    const AnalysisChainVoiceLeading *found = analysis_chain_voice_leading(library);
    if (notes->count == 0 || report->entry_count == 0 || !notes->meter.meter_map || !found ||
        found->voice_count == 0 || analysis_chain_segment_count(library) != report->entry_count) {
        return true;
    }
    VoiceGrid build;
    memset(&build, 0, sizeof(build));
    bool declined = false;
    if (!grid_build(notes, input, library, &build, &declined)) {
        grid_destroy(&build);
        return false;
    }
    if (declined) {
        grid_destroy(&build);
        return true;
    }
    AnalysisVoiceLeading *out = &report->voice_leading;
    out->present = true;
    out->framework = FRAMEWORK_STANDARD_COUNTERPOINT;
    out->voice_count = build.voice_count;
    out->row_count = build.row_count;
    bool ok = collect_motions(&build, &found->motions, out) && collect_perfects(&build, &found->perfects, out) &&
              collect_resolutions(&build, harmony, &found->resolutions, out) && collect_chords(&build, harmony, out);
    grid_destroy(&build);
    return ok;
}
