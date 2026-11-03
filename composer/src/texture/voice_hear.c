#include <ursatz_composer/voice_hear.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-counterpoint/analysis/analysis_grid.h>
#include <standard-counterpoint/analysis/analysis_perfects.h>
#include <standard-harmony/analysis/analysis_resolution.h>
#include <standard-harmony/analysis/analysis_voices.h>
#include <identity/entity_id.h>
#include <pitch/pitch_numeric_value.h>
#include <pitch/tuning_system.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "util/util.h"
#include "texture/texture_internal.h"

/* The voices of the grid, top to bottom, as the lanes they are heard from. */
#define VOICES 4
static const PlanLaneKind VOICE_LANE[VOICES] = {PLAN_LANE_MELODY, PLAN_LANE_INNER_VOICE, PLAN_LANE_INNER_VOICE,
                                                PLAN_LANE_BASS};
static const int VOICE_NUMBER[VOICES] = {0, 1, 2, 0};

/* Everything the two analyses borrow, kept until they are done. */
typedef struct {
    TuningSystem *tuning;
    PitchNumericValue *values[HARMONY_MAX_CHORDS * VOICES];
    PitchSpelling *spellings[HARMONY_MAX_CHORDS * VOICES];
    char row_ids[HARMONY_MAX_CHORDS][12];
    const char *row_id_ptrs[HARMONY_MAX_CHORDS];
    char segment_ids[HARMONY_MAX_CHORDS][12];
    const char *segment_id_ptrs[HARMONY_MAX_CHORDS];
    char note_ids[HARMONY_MAX_CHORDS * VOICES][16];
    StandardCounterpointVoiceCell counterpoint_cells[HARMONY_MAX_CHORDS * VOICES];
    HarmonyVoiceCell harmony_cells[HARMONY_MAX_CHORDS * VOICES];
} Grid;

const char *voice_hear_result_name(VoiceHearResult result) {
    switch (result) {
    case VOICE_HEAR_OK:
        return "ok";
    case VOICE_HEAR_ERR_NULL:
        return "null argument";
    case VOICE_HEAR_ERR_ALLOC:
        return "out of memory";
    case VOICE_HEAR_ERR_NOT_WRITTEN:
        return "lane not written";
    case VOICE_HEAR_ERR_UNREADABLE:
        return "lane not readable";
    case VOICE_HEAR_ERR_ANALYSIS:
        return "analysis failed";
    }
    return "unknown";
}

static void grid_free(Grid *grid) {
    for (size_t i = 0; i < HARMONY_MAX_CHORDS * VOICES; i++) {
        pitch_numeric_value_destroy(grid->values[i]);
        pitch_spelling_destroy(grid->spellings[i]);
    }
    tuning_system_destroy(grid->tuning);
    free(grid);
}

/* True when a span of the lane and voice over range is Constrained. */
static bool lane_open(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan range) {
    LaneSpanId ids[ACCOMP_MAX_LINE];
    size_t found = plan_lane_find(plan, lane, voice, range, ids, ACCOMP_MAX_LINE);
    for (size_t i = 0; i < found && i < ACCOMP_MAX_LINE; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return true;
        }
    }
    return false;
}

/* The scale degree a pitch is spelled as in the key; the altered degrees share the degree of the natural one. */
static int scale_degree(const AnalysisKey *key, int pitch) {
    int degree = 1;
    int alteration;
    util_scale_degree(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, pitch, &degree,
                      &alteration);
    return degree;
}

/* The spelling of a pitch over a chord: as the tone of the chord it is, else as a degree of the key. */
static PitchSpelling *spell_over(const AnalysisKey *key, const PhraseChord *chord, int pitch) {
    int pcs[4];
    size_t count;
    int root_degree;
    if (accomp_chord_tones(key, chord, pcs, &count)) {
        root_degree = chord->applied ? harmony_applied_root_degree(harmony_mode_of(key->mode), &chord->applied_chord)
                                     : chord->chord.degree;
        for (size_t k = 0; root_degree >= 1 && root_degree <= 7 && k < count; k++) {
            if (pcs[k] == pitch % 12) {
                return harmony_spell_pitch(key, (root_degree - 1 + 2 * (int)k) % 7 + 1, pitch);
            }
        }
    }
    return harmony_spell_pitch(key, scale_degree(key, pitch), pitch);
}

/* The pitch of the note sounding at a position, or -1 for a rest. */
static int sounding_at(const AccompLine *lines, size_t count, Rational position, bool *attacked) {
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(lines[i].onset, position) <= 0 && rational_cmp(position, lines[i].end) < 0) {
            *attacked = rational_cmp(lines[i].onset, position) == 0;
            return lines[i].pitch;
        }
    }
    return -1;
}

static VoiceHearResult count_violations(const StandardCounterpointVoiceGrid *counterpoint, const HarmonyVoiceGrid *harmony,
                                        const HeardPhrase *phrase, VoiceHeard *out) {
    StandardCounterpointPreparedGrid prepared;
    StandardCounterpointViolations perfects = {NULL, 0};
    HarmonyResolutions resolutions = {NULL, 0};
    SegmentSet *set;
    VoiceHearResult result = VOICE_HEAR_ERR_ANALYSIS;
    StandardCounterpointGridStatus status = standard_counterpoint_grid_prepare(counterpoint, &prepared);
    if (status != STANDARD_COUNTERPOINT_GRID_OK) {
        return status == STANDARD_COUNTERPOINT_GRID_ERR_ALLOC ? VOICE_HEAR_ERR_ALLOC : VOICE_HEAR_ERR_UNREADABLE;
    }
    if (standard_counterpoint_check_perfects(&prepared, &perfects) == STANDARD_COUNTERPOINT_PERFECTS_OK) {
        for (size_t i = 0; i < perfects.count; i++) {
            out->parallel_fifths += perfects.items[i].rule == STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_FIFTHS ? 1 : 0;
            out->parallel_octaves += perfects.items[i].rule == STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_OCTAVES ? 1 : 0;
        }
        set = harmony_segments_create(phrase);
        if (!set) {
            result = VOICE_HEAR_ERR_UNREADABLE;
        } else {
            size_t count;
            const HarmonySegment *segments = harmony_segments_get(set, &count);
            if (harmony_analysis_check_resolutions(segments, count, harmony, NULL, &resolutions) == HARMONY_RESOLUTION_OK) {
                for (size_t i = 0; i < resolutions.count; i++) {
                    const HarmonyResolutionResult *item = &resolutions.items[i];
                    out->unresolved_sevenths += item->rule == HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN ? 1 : 0;
                    out->unresolved_leading_tones += item->rule == HARMONY_RESOLUTION_LEADING_TONE_RESOLVES ? 1 : 0;
                }
                result = VOICE_HEAR_OK;
            }
            harmony_segments_destroy(set);
        }
    }
    harmony_resolutions_destroy(&resolutions);
    standard_counterpoint_violations_destroy(&perfects);
    standard_counterpoint_grid_destroy(&prepared);
    return result;
}

VoiceHearResult voice_hear(const PiecePlan *plan, PlanSpan span, VoiceHeard *out) {
    PhraseChord chords[HARMONY_MAX_CHORDS];
    PhraseMetre metre;
    LaneSpanId harmony[HARMONY_MAX_CHORDS];
    AccompLine lines[VOICES][ACCOMP_MAX_LINE];
    size_t line_count[VOICES];
    AnalysisKey key;
    size_t count;
    Rational origin = span.start;
    Rational length = span.length;
    Grid *grid;
    EntityID *id = NULL;
    VoiceHearResult result = VOICE_HEAR_OK;
    if (!plan || !out) {
        return VOICE_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    for (size_t voice = 0; voice < VOICES; voice++) {
        if (lane_open(plan, VOICE_LANE[voice], VOICE_NUMBER[voice], span)) {
            return VOICE_HEAR_ERR_NOT_WRITTEN;
        }
    }
    if (lane_open(plan, PLAN_LANE_HARMONY, 0, span)) {
        return VOICE_HEAR_ERR_NOT_WRITTEN;
    }
    if (!harmony_find_key(plan, PLAN_LANE_SPAN_NONE, PLAN_NODE_NONE, span, &key) ||
        !harmony_read_lane(plan, span, key.mode, chords, &count, &metre) || count == 0 ||
        plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, harmony, HARMONY_MAX_CHORDS) != count) {
        return VOICE_HEAR_ERR_UNREADABLE;
    }
    for (size_t voice = 0; voice < VOICES; voice++) {
        if (!accomp_read_line(plan, VOICE_LANE[voice], VOICE_NUMBER[voice], span, origin, length, lines[voice],
                              &line_count[voice])) {
            return VOICE_HEAR_ERR_UNREADABLE;
        }
    }
    grid = calloc(1, sizeof(*grid));
    if (!grid) {
        return VOICE_HEAR_ERR_ALLOC;
    }
    if (entity_id_create("composer-voice-hear", &id) != ENTITY_ID_OK ||
        tuning_system_create(id, "MIDI note number", &grid->tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(id);
        grid_free(grid);
        return VOICE_HEAR_ERR_ALLOC;
    }
    for (size_t row = 0; result == VOICE_HEAR_OK && row < count; row++) {
        Rational start;
        snprintf(grid->row_ids[row], sizeof(grid->row_ids[row]), "r%zu", row);
        grid->row_id_ptrs[row] = grid->row_ids[row];
        snprintf(grid->segment_ids[row], sizeof(grid->segment_ids[row]), "c%zu", row);
        grid->segment_id_ptrs[row] = grid->segment_ids[row];
        start = position_sub(plan_lane_span_span(plan, harmony[row]).start, origin);
        for (size_t voice = 0; voice < VOICES; voice++) {
            size_t cell = row * VOICES + voice;
            bool attacked = false;
            int pitch = sounding_at(lines[voice], line_count[voice], start, &attacked);
            if (pitch < 0) {
                continue;
            }
            snprintf(grid->note_ids[cell], sizeof(grid->note_ids[cell]), "r%zuv%zu", row, voice);
            if (pitch_numeric_value_create(grid->tuning, pitch, 1, &grid->values[cell]) != PITCH_NUMERIC_VALUE_OK) {
                result = VOICE_HEAR_ERR_ALLOC;
                break;
            }
            grid->spellings[cell] = spell_over(&key, &chords[row], pitch);
            grid->counterpoint_cells[cell] = (StandardCounterpointVoiceCell){true,           grid->values[cell],
                                                                             grid->spellings[cell], grid->note_ids[cell],
                                                                             attacked,        1.0};
            grid->harmony_cells[cell] = (HarmonyVoiceCell){true,     grid->values[cell], grid->spellings[cell],
                                                           grid->note_ids[cell], attacked,        1.0};
        }
    }
    if (result == VOICE_HEAR_OK) {
        StandardCounterpointVoiceGrid counterpoint = {count, VOICES, grid->row_id_ptrs, grid->counterpoint_cells};
        HarmonyVoiceGrid harmony_grid = {count, VOICES, grid->segment_id_ptrs, grid->harmony_cells};
        HeardPhrase phrase = {chords, count, key.mode, CADENCE_TYPE_PERFECT_AUTHENTIC, metre.cells_per_bar, metre.cell_length, false, NULL,
                              false,  false, NULL, NULL, false, &metre};
        result = count_violations(&counterpoint, &harmony_grid, &phrase, out);
        out->rows = count;
    }
    if (result != VOICE_HEAR_OK) {
        memset(out, 0, sizeof(*out));
    }
    grid_free(grid);
    return result;
}
