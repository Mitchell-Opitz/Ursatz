#include "pipeline/voice_leading_pipeline.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "core/rational.h"
#include "events/note_event.h"
#include "pipeline/spelling_util.h"
#include "pipeline/voicing_check.h"
#include <ursatz_chain/chain_inputs.h>

#include "pitch/pitch_numeric_value.h"
#include "standard-counterpoint/analysis/analysis_grid.h"
#include "standard-counterpoint/analysis/analysis_motion.h"
#include "standard-counterpoint/analysis/analysis_perfects.h"
#include "standard-counterpoint/framework.h"
#include "standard-harmony/analysis/analysis_resolution.h"
#include "standard-harmony/analysis/analysis_voices.h"
#include "standard-meter/segmentation_beat_grid.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "util/misc_util.h"

#define NO_NOTE SIZE_MAX

/* The chain's voice grid again, for the notes in its cells and the spellings of its rows (the chain keeps its grid to
   itself), and what keeps it alive. */
typedef struct {
    size_t voice_count;
    size_t row_count;
    ChainPitches pitches;
    ChainVoiceGrid grid;
    size_t *note_of_cell; /* ImportedNotes index of each cell's note, NO_NOTE where the voice rests */
    StandardCounterpointPreparedGrid prepared;
    bool have_pitches;
    bool have_grid;
    bool prepared_ok;
} VoiceGrid;

static void grid_destroy(VoiceGrid *build) {
    if (build->prepared_ok) {
        standard_counterpoint_grid_destroy(&build->prepared);
    }
    if (build->have_grid) {
        chain_voice_grid_destroy(&build->grid);
    }
    if (build->have_pitches) {
        chain_pitches_destroy(&build->pitches);
    }
    free(build->note_of_cell);
    memset(build, 0, sizeof(*build));
}

static const AnalysisVoiceCell *cell_of(const VoiceGrid *build, size_t row, size_t voice) {
    return &build->grid.grid.cells[row * build->voice_count + voice];
}

/* Builds the chain's grid again and the note of each cell. false only on allocation failure; *declined is set when the
   library cannot take the piece. */
static bool grid_build(const ImportedNotes *notes, const ChainInput *input, const AnalysisChain *library,
                       VoiceGrid *build, bool *declined) {
    *declined = false;
    const ChainSegments *segments = analysis_chain_segments(library);
    ChainInputStatus status = chain_build_pitches(&input->set, &build->pitches);
    if (status != CHAIN_INPUT_OK) {
        *declined = status != CHAIN_INPUT_ERR_ALLOC;
        return *declined;
    }
    build->have_pitches = true;
    status = chain_build_voice_grid(&input->set, &build->pitches, segments, analysis_chain_segment_count(library),
                                    &build->grid);
    if (status != CHAIN_INPUT_OK) {
        *declined = status != CHAIN_INPUT_ERR_ALLOC;
        return *declined;
    }
    build->have_grid = true;
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
        build->note_of_cell[i] = cell->present ? chain_input_import_index(input, cell->note_id) : NO_NOTE;
        if (cell->present && build->note_of_cell[i] == NO_NOTE) {
            return false;
        }
    }
    (void)notes;
    StandardCounterpointGridStatus prepared = standard_counterpoint_grid_prepare(&build->grid.grid, &build->prepared);
    if (prepared == STANDARD_COUNTERPOINT_GRID_ERR_ALLOC) {
        return false;
    }
    build->prepared_ok = prepared == STANDARD_COUNTERPOINT_GRID_OK;
    *declined = !build->prepared_ok;
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
    out->resolutions_checked = true;
    return true;
}

/* The semitone of a sounding cell. */
static long cell_semitone(const VoiceGrid *build, size_t row, size_t voice) {
    return (long)pitch_numeric_value_position_numerator(cell_of(build, row, voice)->pitch);
}

static int pitch_class(long semitone) {
    return (int)(((semitone % 12) + 12) % 12);
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
        const AnalysisVoiceCell *cell = cell_of(build, row, sounding[i]);
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

bool voice_leading_stage_collect(const ImportedNotes *notes, const ChainInput *input, const AnalysisChain *library,
                                 const HarmonyStage *harmony, AnalysisReport *report) {
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
    out->framework = framework_name(standard_counterpoint_framework());
    out->voice_count = build.voice_count;
    out->row_count = build.row_count;
    bool ok = collect_motions(&build, &found->motions, out) && collect_perfects(&build, &found->perfects, out) &&
              collect_resolutions(&build, harmony, &found->resolutions, out) && collect_chords(&build, harmony, out);
    grid_destroy(&build);
    return ok;
}
