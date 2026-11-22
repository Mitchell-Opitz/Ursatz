#include "chain/chain_inputs.h"

#include "chain_test_piece.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/tuning_system.h"
#include "theory/harmony/harmony_cadence_rules.h"
#include "analysis/harmony/outer_scale_degrees.h"
#include "theory/meter/segmentation_beat_grid.h"
#include "chain/analysis_chain.h"
#include "ursatz_test_knowledge.h"

/* The default config with the snapshot's materials: the key regions estimate has none of its own. */
static const CommonPracticeKeyRegionsConfig *default_config(void) {
    static CommonPracticeKeyRegionsConfig config;
    config = common_practice_key_regions_config_default(test_materials());
    return &config;
}

/*
 * The chain's input builders, each on a small constructed piece (chain_test_piece.h): the results, the error paths (a NULL argument, an
 * invalid or incomplete note set, a count out of range; the output is left untouched), and destroying after a partial failure. The stage
 * inputs (harmony, form, meter, texture, voice grid, melody, galant) are built from the results of a chain run over the same piece, and
 * checked against them.
 */

/* Same tuning system (by identity) and the same position. */
static bool same_pitch_value(const PitchNumericValue *a, const PitchNumericValue *b) {
    return entity_id_equals(tuning_system_id(pitch_numeric_value_tuning_system(a)),
                            tuning_system_id(pitch_numeric_value_tuning_system(b))) &&
           pitch_numeric_value_position_numerator(a) == pitch_numeric_value_position_numerator(b) &&
           pitch_numeric_value_position_denominator(a) == pitch_numeric_value_position_denominator(b);
}

/* The cell of a segment and voice, or NULL when either is out of range or the grid has no cells. */
static const AnalysisVoiceCell *grid_cell(const AnalysisVoiceGrid *grid, size_t segment, size_t voice) {
    if (grid->cells == NULL || segment >= grid->segment_count || voice >= grid->voice_count) {
        return NULL;
    }
    return &grid->cells[segment * grid->voice_count + voice];
}

static MusicalTime time_at(long numerator, unsigned long denominator) {
    MusicalTime time;
    assert(musical_time_make(numerator, (long long)denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

static void test_segments_and_members(TestPiece *piece) {
    ChainSegments segments;
    memset(&segments, 0, sizeof(segments));
    assert(chain_build_segments(NULL, NULL, &segments) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_segments(&piece->set, NULL, NULL) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    AnalysisNoteSet empty = {NULL, 0, piece->import.meter_map};
    assert(chain_build_segments(&empty, NULL, &segments) == CHAIN_INPUT_ERR_BAD_NOTES);

    AnalysisNote *no_event = malloc(piece->count * sizeof(AnalysisNote));
    assert(no_event != NULL);
    memcpy(no_event, piece->notes, piece->count * sizeof(AnalysisNote));
    no_event[3].event = NULL;
    AnalysisNoteSet incomplete = {no_event, piece->count, piece->import.meter_map};
    assert(chain_build_segments(&incomplete, NULL, &segments) == CHAIN_INPUT_ERR_NO_EVENT);
    assert(segments.sonorities.items == NULL && segments.ids == NULL && segments.count == 0);

    /* by default a melody over unchanged harmony merges into the segment before: one segment per chord */
    assert(chain_build_segments(&piece->set, NULL, &segments) == CHAIN_INPUT_OK);
    assert(segments.count == 4 && segments.members == NULL && segments.segments == NULL);
    assert(strcmp(segments.ids[0], "seg-0") == 0 && rational_equals(segments.starts[0], rational_ratio(0, 1)) && rational_equals(segments.ends[3], rational_ratio(4, 1)));
    assert(chain_build_members(&piece->set, &segments) == CHAIN_INPUT_OK);
    assert(segments.segments[0].member_count == 6 && segments.segments[1].member_count == 6);
    chain_segments_destroy(&segments);

    /* without the merge there is a segment per change of the sounding pitch classes and bass (a repeated D, or an E over a C and E, adds none) */
    AdaptiveSegmentationConfig beats = adaptive_segmentation_config_default();
    beats.merge_unchanged_harmony = false;
    assert(chain_build_segments(&piece->set, &beats, &segments) == CHAIN_INPUT_OK);
    assert(segments.count == 14 && segments.members == NULL && segments.segments == NULL);
    assert(strcmp(segments.ids[13], "seg-13") == 0);
    for (size_t s = 0; s < segments.count; s++) {
        assert(rational_cmp(segments.ends[s], segments.starts[s]) > 0);
        assert(s == 0 || rational_cmp(segments.starts[s], segments.ends[s - 1]) >= 0);
    }
    assert(rational_equals(segments.ends[13], rational_ratio(4, 1)));

    /* the members: a set missing a note a segment names fails and leaves the segments without members */
    assert(chain_build_members(&piece->set, NULL) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    AnalysisNoteSet missing = {piece->notes + 1, piece->count - 1, piece->import.meter_map};
    assert(chain_build_members(&missing, &segments) == CHAIN_INPUT_ERR_UNKNOWN_NOTE);
    assert(segments.members == NULL && segments.segments == NULL);

    assert(chain_build_members(&piece->set, &segments) == CHAIN_INPUT_OK);
    assert(segments.members != NULL && segments.segments != NULL);
    const CommonPracticeSegment *first = &segments.segments[0];
    assert(first->member_count == 3 && strcmp(first->id, "seg-0") == 0);
    for (size_t j = 0; j < first->member_count; j++) {
        assert(!first->members[j].held && first->members[j].strength == BEAT_STRENGTH_STRONG && rational_is_zero(first->members[j].onset));
        assert(rational_cmp(first->members[j].duration, rational_ratio(0, 1)) > 0 && first->members[j].note_id != NULL && first->members[j].voice_id != NULL);
    }
    /* the second beat holds the alto and the bass, and attacks the soprano */
    const CommonPracticeSegment *second = &segments.segments[1];
    size_t held = 0;
    for (size_t j = 0; j < second->member_count; j++) {
        held += second->members[j].held ? 1 : 0;
        assert(!second->members[j].held || second->members[j].strength == BEAT_STRENGTH_STRONG);
        assert(second->members[j].held || rational_equals(second->members[j].onset, rational_ratio(1, 4)));
    }
    assert(held == 2 && second->member_count == 3);
    /* the third beat's soprano is a medium attack */
    for (size_t j = 0; j < segments.segments[2].member_count; j++) {
        assert(segments.segments[2].members[j].held || segments.segments[2].members[j].strength == BEAT_STRENGTH_MEDIUM);
    }
    chain_segments_destroy(&segments);
    assert(segments.count == 0 && segments.members == NULL);
    chain_segments_destroy(NULL);
    free(no_event);
}

static void test_attack_strength(TestPiece *piece) {
    const MeterMap *map = piece->import.meter_map;
    MusicalTime beat_one = time_at(0, 1);
    MusicalTime beat_two = time_at(1, 4);
    MusicalTime beat_three = time_at(1, 2);
    MusicalTime offbeat = time_at(1, 8);
    assert(chain_attack_strength(map, &beat_one) == BEAT_STRENGTH_STRONG);
    assert(chain_attack_strength(map, &beat_two) == BEAT_STRENGTH_WEAK);
    assert(chain_attack_strength(map, &beat_three) == BEAT_STRENGTH_MEDIUM);
    assert(chain_attack_strength(map, &offbeat) == BEAT_STRENGTH_WEAK);
    assert(chain_attack_strength(NULL, &beat_one) == BEAT_STRENGTH_WEAK && chain_attack_strength(map, NULL) == BEAT_STRENGTH_WEAK);
}

static void test_keys(TestPiece *piece) {
    ChainSegments segments;
    memset(&segments, 0, sizeof(segments));
    assert(chain_build_segments(&piece->set, NULL, &segments) == CHAIN_INPUT_OK);
    CommonPracticeKeySegment *key_segments = (CommonPracticeKeySegment *)0x1;
    assert(chain_build_key_segments(&segments, segments.count, NULL, &key_segments) == CHAIN_INPUT_ERR_BAD_NOTES); /* no members yet */
    assert(key_segments == (CommonPracticeKeySegment *)0x1);
    assert(chain_build_members(&piece->set, &segments) == CHAIN_INPUT_OK);
    assert(chain_build_key_segments(NULL, 1, NULL, &key_segments) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_key_segments(&segments, 0, NULL, &key_segments) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_key_segments(&segments, segments.count + 1, NULL, &key_segments) == CHAIN_INPUT_ERR_BAD_NOTES);
    CommonPracticeWeightedConfig bad = common_practice_weighted_config_default();
    bad.held_factor = -1.0;
    assert(chain_build_key_segments(&segments, segments.count, &bad, &key_segments) == CHAIN_INPUT_ERR_WEIGHTS);
    assert(key_segments == (CommonPracticeKeySegment *)0x1);

    key_segments = NULL;
    assert(chain_build_key_segments(&segments, segments.count, NULL, &key_segments) == CHAIN_INPUT_OK);
    assert(key_segments[0].weights.total > 0.0 && key_segments[0].source == &segments.segments[0]);
    assert(musical_time_same(key_segments[0].start, sonority_time_window(segments.sonorities.items[0].sonority)->start));

    CommonPracticeKeyRegions regions;
    memset(&regions, 0, sizeof(regions));
    const MeterMap *map = piece->import.meter_map;
    assert(chain_estimate_key_regions(NULL, segments.count, map, NULL, 0, default_config(), &regions) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_estimate_key_regions(key_segments, segments.count, map, NULL, 1, default_config(), &regions) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    MusicalTime before;
    assert(musical_time_make(-1, 1, &before) == MUSICAL_TIME_OK);
    SpelledKeyEvent no_time = {before, {NULL, PITCH_LETTER_C, 0, test_materials()->major}}; /* before the piece starts */
    assert(chain_estimate_key_regions(key_segments, segments.count, map, &no_time, 1, default_config(), &regions) == CHAIN_INPUT_ERR_KEYS);
    assert(regions.regions == NULL);

    MusicalTime zero;
    assert(musical_time_make(0, 1, &zero) == MUSICAL_TIME_OK);
    SpelledKeyEvent signature = {zero, {NULL, PITCH_LETTER_C, 0, test_materials()->major}};
    assert(chain_estimate_key_regions(key_segments, segments.count, map, &signature, 1, default_config(), &regions) == CHAIN_INPUT_OK);
    assert(regions.region_count >= 1 && regions.regions[0].key_index == 0); /* C major */
    common_practice_key_regions_destroy(&regions);
    assert(chain_estimate_key_regions(key_segments, segments.count, map, NULL, 0, default_config(), &regions) == CHAIN_INPUT_OK);
    assert(regions.region_count >= 1 && regions.regions[0].key_index == 0);
    common_practice_key_regions_destroy(&regions);
    chain_key_segments_destroy(key_segments);
    chain_key_segments_destroy(NULL);
    chain_segments_destroy(&segments);
}

static void test_pitches(TestPiece *piece) {
    ChainPitches pitches;
    memset(&pitches, 0, sizeof(pitches));
    assert(chain_build_pitches(NULL, &pitches) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_pitches(&piece->set, &pitches) == CHAIN_INPUT_OK);
    assert(pitches.count == piece->count);
    for (size_t i = 0; i < pitches.count; i++) {
        assert(pitches.events[i] == piece->notes[i].event);
        assert(pitch_numeric_value_position_numerator(pitches.values[i]) == piece->notes[i].midi);
    }
    chain_pitches_destroy(&pitches);
    assert(pitches.count == 0 && pitches.values == NULL);

    /* a failure part-way destroys what was made and leaves the output untouched */
    AnalysisNote *copy = malloc(piece->count * sizeof(AnalysisNote));
    assert(copy != NULL);
    memcpy(copy, piece->notes, piece->count * sizeof(AnalysisNote));
    copy[piece->count - 2].midi = ANALYSIS_PITCH_UNKNOWN;
    AnalysisNoteSet unknown = {copy, piece->count, piece->import.meter_map};
    assert(chain_build_pitches(&unknown, &pitches) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(pitches.values == NULL && pitches.tuning == NULL);
    copy[piece->count - 2] = piece->notes[piece->count - 2];
    copy[piece->count - 3].event = NULL;
    assert(chain_build_pitches(&unknown, &pitches) == CHAIN_INPUT_ERR_NO_EVENT);
    free(copy);
}

/* The stage inputs, from a chain run over the piece. */
static void test_stage_inputs(TestPiece *piece) {
    AnalysisChainConfig config = analysis_chain_config_default();
    config.knowledge = test_knowledge();
    config.stages = ANALYSIS_CHAIN_STAGE_CADENCES | ANALYSIS_CHAIN_STAGE_MELODY | ANALYSIS_CHAIN_STAGE_SCHEMAS;
    config.refine_keys = false;
    MusicalTime zero;
    assert(musical_time_make(0, 1, &zero) == MUSICAL_TIME_OK);
    SpelledKeyEvent signature = {zero, {NULL, PITCH_LETTER_C, 0, test_materials()->major}};
    AnalysisChain *chain = NULL;
    AnalysisChainStatus status = analysis_chain_run(&piece->set, &signature, 1, &config, &chain);
    assert(status == ANALYSIS_CHAIN_OK);
    size_t count = analysis_chain_segment_count(chain);
    const ChainSegments *segments = analysis_chain_segments(chain);
    const StandardTonalityChordInput *chords = analysis_chain_chord_inputs(chain);
    const StandardTonalityRomanResult *romans = analysis_chain_numerals(chain);
    const OuterVoiceLines *lines = analysis_chain_outer_voices(chain);
    const MeterMap *map = piece->import.meter_map;
    assert(count == segments->count && lines != NULL && chords != NULL && romans != NULL);

    /* the segment lines: one entry per chord segment, the voice segment sounding at its start (here the chord segments merge: 4 over 14) */
    const ChainSegments *voice_segments = analysis_chain_voice_segments(chain);
    OuterVoiceLines built_lines;
    memset(&built_lines, 0, sizeof(built_lines));
    assert(chain_build_segment_lines(voice_segments, lines, segments, count, &built_lines) == CHAIN_INPUT_OK);
    const OuterVoiceLines *segment_lines = &built_lines;
    assert(segment_lines != NULL && segment_lines->count == count && lines->count == voice_segments->count && lines->count > count);
    for (size_t i = 0; i < count; i++) {
        size_t v = 0;
        while (v + 1 < voice_segments->count && rational_cmp(voice_segments->starts[v + 1], segments->starts[i]) <= 0) {
            v++;
        }
        assert(segment_lines->soprano[i].present == lines->soprano[v].present && segment_lines->bass[i].present == lines->bass[v].present);
        assert(strcmp(entity_id_value(segment_lines->soprano[i].note_id), entity_id_value(lines->soprano[v].note_id)) == 0);
        assert(strcmp(entity_id_value(segment_lines->bass[i].note_id), entity_id_value(lines->bass[v].note_id)) == 0);
    }
    outer_voice_lines_destroy(&built_lines);

    /* harmony and cadences */
    ChainHarmonyInputs harmony;
    memset(&harmony, 0, sizeof(harmony));
    assert(chain_build_harmony_segments(NULL, count, map, chords, romans, lines, &harmony) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_harmony_segments(segments, 0, map, chords, romans, lines, &harmony) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_harmony_segments(segments, count + 1, map, chords, romans, lines, &harmony) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(harmony.inputs == NULL);
    assert(chain_build_harmony_segments(segments, count, map, chords, romans, lines, &harmony) == CHAIN_INPUT_OK);
    assert(harmony.count == count);
    for (size_t s = 0; s < count; s++) {
        const HarmonySegmentInput *input = &harmony.inputs[s];
        assert(strcmp(input->segment.id, segments->ids[s]) == 0 && input->segment.position == harmony.positions[s]);
        assert(input->has_beat_length && duration_same(input->beat_length, harmony.beat_length) && strcmp(input->key_ref, "key") == 0);
        if (romans[s].has_numeral) {
            assert(input->degree == romans[s].claim.degree && input->inversion == romans[s].claim.inversion);
            assert(input->interval_count == 3 || input->interval_count == 4);
            assert(input->uncertainty == romans[s].uncertainty);
        } else {
            assert(input->degree == 0 && input->interval_count == 3);
        }
    }
    assert(harmony.inputs[0].segment.strength == BEAT_STRENGTH_STRONG);
    HarmonySegment *prepared = NULL;
    HarmonyCadences cadences = {0};
    assert(harmony_analysis_prepare(harmony.inputs, count, &prepared) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_find_cadences(prepared, count, NULL, &cadences) == HARMONY_CADENCE_ANALYSIS_OK);
    size_t found = cadences.count;
    assert(chain_add_boundary_cadences(NULL, count, analysis_chain_key_regions(chain), 0, &cadences) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_add_boundary_cadences(prepared, count, analysis_chain_key_regions(chain), 0, &cadences) == CHAIN_INPUT_OK);
    assert(cadences.count >= found); /* a piece that stays in C has no section start: nothing is added */
    assert(chain_confirm_key_changes(NULL, segments->starts, &cadences) == CHAIN_INPUT_ERR_NULL_ARGUMENT);

    /* the form input: the soprano as notes (consecutive segments on one id are one note), the segments and the cadences. The outer
       lines are read on the segmentation the form input walks, so the lines here come from a segmentation without the merge. */
    AdaptiveSegmentationConfig beats = adaptive_segmentation_config_default();
    beats.merge_unchanged_harmony = false;
    ChainSegments aligned;
    memset(&aligned, 0, sizeof(aligned));
    assert(chain_build_segments(&piece->set, &beats, &aligned) == CHAIN_INPUT_OK);
    ChainPitches line_pitches;
    memset(&line_pitches, 0, sizeof(line_pitches));
    assert(chain_build_pitches(&piece->set, &line_pitches) == CHAIN_INPUT_OK);
    OuterVoiceLines aligned_lines = {0};
    assert(outer_voice_extract(&aligned.sonorities, line_pitches.events, line_pitches.values, line_pitches.count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &aligned_lines) == OUTER_VOICE_OK);
    ChainFormInput form;
    memset(&form, 0, sizeof(form));
    assert(chain_build_form_input(&piece->set, NULL, &aligned, &aligned_lines, 1, 4, &cadences, &form) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_form_input(&piece->set, &aligned, NULL, &aligned_lines, 1, 4, &cadences, &form) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_form_input(&piece->set, &aligned, &aligned, &aligned_lines, 1, 4, &cadences, &form) == CHAIN_INPUT_OK);
    assert(form.input.finished && form.input.note_count > 0 && form.input.note_count <= 16);
    assert(form.input.cadence_count == cadences.count);
    assert(form.input.notes[0].midi == 76); /* E5 */
    chain_form_input_destroy(&form);
    assert(chain_build_form_input(&piece->set, &aligned, &aligned, &aligned_lines, 3, 3, NULL, &form) == CHAIN_INPUT_OK);
    assert(form.input.cadence_count == 0 && form.input.note_count > 0 && form.input.note_count <= 4);
    assert(form.input.notes[0].midi == 74); /* D5, the first soprano note of measure 3 */
    chain_form_input_destroy(&form);
    chain_form_input_destroy(NULL);
    outer_voice_lines_destroy(&aligned_lines);
    chain_pitches_destroy(&line_pitches);
    chain_segments_destroy(&aligned);

    /* the motive input: a note-level soprano with its degrees */
    assert(chain_build_motive_input(&piece->set, segments, 1, 4, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), &cadences, &form) == CHAIN_INPUT_OK);
    assert(form.input.note_count == 16 && form.input.degrees[0] == 3 && form.input.degrees[1] == 2 &&
           form.input.degrees[2] == 1);
    chain_form_input_destroy(&form);

    /* the meter input: every note of the measures, a chord change where the label changes */
    bool labels[64] = {false};
    assert(count <= 64);
    const CommonPracticeWeightedResult *results = analysis_chain_chords(chain);
    for (size_t s = 1; s < count; s++) {
        labels[s] = results[s].matched && results[s - 1].matched &&
                    strcmp(results[s].best_chord.label, results[s - 1].best_chord.label) != 0;
    }
    ChainMeterInput meter;
    memset(&meter, 0, sizeof(meter));
    assert(chain_build_meter_input(&piece->set, NULL, segments, labels, 1, 4, NULL, NULL, NULL, &meter) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_meter_input(&piece->set, lines, segments, labels, 1, 4, NULL, NULL, NULL, &meter) == CHAIN_INPUT_OK);
    assert(meter.input.note_count == piece->count);
    size_t soprano_notes = 0;
    size_t bass_notes = 0;
    for (size_t i = 0; i < meter.input.note_count; i++) {
        soprano_notes += standard_meter_note_line(&meter.input.notes[i]) == 0 ? 1 : 0;
        bass_notes += standard_meter_note_line(&meter.input.notes[i]) == 1 ? 1 : 0;
    }
    assert(soprano_notes > 0 && bass_notes > 0);
    assert(meter.input.chord_change_count > 0 && meter.input.phrase_count == 0);
    chain_meter_input_destroy(&meter);
    assert(chain_build_meter_input(&piece->set, lines, segments, labels, 3, 3, NULL, NULL, NULL, &meter) == CHAIN_INPUT_OK);
    assert(meter.input.note_count == 6);
    chain_meter_input_destroy(&meter);
    chain_meter_input_destroy(NULL);

    /* the texture input and the voice grid share the separation of the notes into voices */
    ChainPitches pitches;
    memset(&pitches, 0, sizeof(pitches));
    assert(chain_build_pitches(&piece->set, &pitches) == CHAIN_INPUT_OK);
    ChainTextureInput texture;
    memset(&texture, 0, sizeof(texture));
    assert(chain_build_texture_input(&piece->set, NULL, &texture) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_texture_input(&piece->set, &pitches, &texture) == CHAIN_INPUT_OK);
    assert(texture.set.count == piece->count && texture.assignment.count == piece->count && texture.stream_count == 1);
    chain_texture_input_destroy(&texture);
    chain_texture_input_destroy(NULL);

    ChainVoiceGrid grid;
    memset(&grid, 0, sizeof(grid));
    assert(chain_build_voice_grid(&piece->set, &pitches, segments, 0, &grid) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_voice_grid(&piece->set, &pitches, segments, count, &grid) == CHAIN_INPUT_OK);
    assert(grid.grid.segment_count == count && grid.grid.voice_count >= 3 && strcmp(grid.grid.segment_ids[0], "seg-0") == 0);
    size_t present = 0;
    for (size_t v = 0; v < grid.grid.voice_count; v++) {
        const AnalysisVoiceCell *cell = grid_cell(&grid.grid, 0, v);
        present += cell->present ? 1 : 0;
        assert(!cell->present || (cell->attacked && cell->pitch != NULL && cell->note_id != NULL));
    }
    assert(present == 3);
    chain_voice_grid_destroy(&grid);
    chain_voice_grid_destroy(NULL);
    chain_pitches_destroy(&pitches);

    /* the melody line: one note per soprano note, degrees in C major, a chord tone on the downbeat */
    ChainMelodyLine melody;
    memset(&melody, 0, sizeof(melody));
    assert(chain_build_melody_line(segments, count, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), NULL) ==
           CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_melody_line(segments, 0, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), &melody) ==
           CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_melody_line(segments, count, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), &melody) == CHAIN_INPUT_OK);
    assert(melody.line.count > 0 && melody.line.notes[0].present && melody.line.notes[0].degree == 3 &&
           melody.line.notes[0].alteration == 0 && melody.segment[0] == 0);
    assert(melody.line.notes[0].has_strength && melody.line.notes[0].strength == BEAT_STRENGTH_STRONG);
    chain_melody_line_destroy(&melody);
    chain_melody_line_destroy(NULL);

    /* the melody line on the voice lines (R-377): one note per run of soprano note ids over all voice segments, not one per chord segment,
       each on the chord segment holding its onset; the chord results are read there */
    ChainMelodyLine voiced;
    memset(&voiced, 0, sizeof(voiced));
    assert(chain_build_melody_line(NULL, lines->count, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), &voiced) ==
           CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_melody_line(voice_segments, 0, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), &voiced) ==
           CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_melody_line(voice_segments, lines->count + 1, lines->soprano, segments, count, results, 0,
                                   COMMON_PRACTICE_KEY_MAJOR, test_materials(), &voiced) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_melody_line(voice_segments, lines->count, lines->soprano, segments, 0, results, 0, COMMON_PRACTICE_KEY_MAJOR,
                                   test_materials(), &voiced) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_melody_line(voice_segments, lines->count, lines->soprano, segments, count, results, 0, COMMON_PRACTICE_KEY_MAJOR,
                                   test_materials(), &voiced) == CHAIN_INPUT_OK);
    size_t runs = 0;
    const char *previous = NULL;
    for (size_t v = 0; v < lines->count; v++) {
        const OuterVoiceEntry *entry = &lines->soprano[v];
        const char *id = entry->present ? entity_id_value(entry->note_id) : NULL;
        runs += id != NULL && (previous == NULL || strcmp(previous, id) != 0) ? 1 : 0;
        previous = id;
    }
    size_t notes_present = 0;
    for (size_t i = 0; i < voiced.line.count; i++) {
        notes_present += voiced.line.notes[i].present ? 1 : 0;
        assert(voiced.segment[i] < count && (i == 0 || voiced.segment[i] >= voiced.segment[i - 1]));
    }
    assert(notes_present == runs && runs > count);
    size_t next_note = 0; /* each note's chord segment is the last one starting at or before its first voice segment */
    previous = NULL;
    for (size_t v = 0; v < lines->count; v++) {
        const OuterVoiceEntry *entry = &lines->soprano[v];
        const char *id = entry->present ? entity_id_value(entry->note_id) : NULL;
        if (id != NULL && (previous == NULL || strcmp(previous, id) != 0)) {
            size_t chord = 0;
            while (chord + 1 < count && rational_cmp(segments->starts[chord + 1], voice_segments->starts[v]) <= 0) {
                chord++;
            }
            assert(voiced.line.notes[next_note].present && voiced.segment[next_note] == chord);
            next_note++;
        }
        previous = id;
    }
    assert(next_note == runs);
    assert(voiced.segment[0] == 0 && voiced.line.notes[0].degree == 3 && voiced.line.notes[0].has_strength &&
           voiced.line.notes[0].strength == BEAT_STRENGTH_STRONG);
    chain_melody_line_destroy(&voiced);

    /* the galant segments */
    ChainGalantSegments galant;
    memset(&galant, 0, sizeof(galant));
    assert(chain_build_galant_segments(segments, count, map, lines, 0, chords, NULL, &galant) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_galant_segments(segments, count, map, lines, 24, chords, romans, &galant) == CHAIN_INPUT_ERR_BAD_NOTES);
    assert(chain_build_galant_segments(segments, count, map, lines, 0, chords, romans, &galant) == CHAIN_INPUT_OK);
    assert(galant.list.count > 0 && galant.list.count <= count);
    assert(strcmp(galant.list.items[0].key_id, "key-0") == 0 && galant.list.items[0].key.degree == 1);
    assert(galant.list.items[0].has_bass && galant.list.items[0].bass.degree == 1);
    assert(galant.list.items[0].beat_numerator == 1 && galant.list.items[0].beat_denominator == 4);
    chain_galant_segments_destroy(&galant);
    chain_galant_segments_destroy(NULL);

    harmony_cadences_destroy(&cadences);
    harmony_analysis_destroy(prepared);
    chain_harmony_inputs_destroy(&harmony);
    chain_harmony_inputs_destroy(NULL);
    analysis_chain_destroy(chain);
}

static void test_segment_lines(TestPiece *piece) {
    /* the outer lines are read on the voice segments (one per change, not merged); the chord segments are merged: four, one per measure */
    ChainSegments chord_segments;
    ChainSegments voice_segments;
    memset(&chord_segments, 0, sizeof(chord_segments));
    memset(&voice_segments, 0, sizeof(voice_segments));
    AdaptiveSegmentationConfig beats = adaptive_segmentation_config_default();
    beats.merge_unchanged_harmony = false;
    assert(chain_build_segments(&piece->set, NULL, &chord_segments) == CHAIN_INPUT_OK);
    assert(chain_build_segments(&piece->set, &beats, &voice_segments) == CHAIN_INPUT_OK);
    assert(chord_segments.count == 4 && voice_segments.count == 14);
    ChainPitches pitches;
    memset(&pitches, 0, sizeof(pitches));
    assert(chain_build_pitches(&piece->set, &pitches) == CHAIN_INPUT_OK);
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&voice_segments.sonorities, pitches.events, pitches.values, pitches.count, piece->set.map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);

    OuterVoiceLines out = {0};
    assert(chain_build_segment_lines(NULL, &lines, &chord_segments, 4, &out) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_segment_lines(&voice_segments, NULL, &chord_segments, 4, &out) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_segment_lines(&voice_segments, &lines, NULL, 4, &out) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_segment_lines(&voice_segments, &lines, &chord_segments, 4, NULL) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(chain_build_segment_lines(&voice_segments, &lines, &chord_segments, 5, &out) == CHAIN_INPUT_ERR_NULL_ARGUMENT);
    assert(out.soprano == NULL && out.bass == NULL && out.count == 0);

    /* each chord segment starts where a voice segment starts: its entry is that voice segment's */
    assert(chain_build_segment_lines(&voice_segments, &lines, &chord_segments, 4, &out) == CHAIN_INPUT_OK);
    assert(out.count == 4);
    for (size_t i = 0; i < 4; i++) {
        size_t v = 0;
        while (v + 1 < voice_segments.count && rational_cmp(voice_segments.starts[v + 1], chord_segments.starts[i]) <= 0) {
            v++;
        }
        assert(out.soprano[i].segment_index == i && out.bass[i].segment_index == i);
        assert(out.soprano[i].present == lines.soprano[v].present && out.bass[i].present == lines.bass[v].present);
        assert(out.soprano[i].present && out.bass[i].present);
        assert(out.soprano[i].note_id != lines.soprano[v].note_id && out.soprano[i].pitch != lines.soprano[v].pitch); /* copies */
        assert(strcmp(entity_id_value(out.soprano[i].note_id), entity_id_value(lines.soprano[v].note_id)) == 0);
        assert(same_pitch_value(out.soprano[i].pitch, lines.soprano[v].pitch));
        assert(strcmp(entity_id_value(out.bass[i].note_id), entity_id_value(lines.bass[v].note_id)) == 0);
    }
    assert(pitch_numeric_value_position_numerator(out.soprano[0].pitch) == 76); /* E5 */
    outer_voice_lines_destroy(&out);

    /* chord segments that start inside a note: before the first voice segment the line is absent, and two segments over one held note share it */
    Rational starts[4] = {{-1, 2}, {1, 10}, {1, 5}, {1, 1}};
    ChainSegments shifted = chord_segments;
    shifted.starts = starts;
    assert(chain_build_segment_lines(&voice_segments, &lines, &shifted, 4, &out) == CHAIN_INPUT_OK);
    assert(!out.soprano[0].present && !out.bass[0].present && out.soprano[0].note_id == NULL);
    assert(out.soprano[1].present && out.soprano[2].present);
    assert(strcmp(entity_id_value(out.soprano[1].note_id), entity_id_value(out.soprano[2].note_id)) == 0);
    assert(strcmp(entity_id_value(out.soprano[1].note_id), entity_id_value(lines.soprano[0].note_id)) == 0);
    assert(strcmp(entity_id_value(out.bass[1].note_id), entity_id_value(out.bass[2].note_id)) == 0);
    outer_voice_lines_destroy(&out);

    outer_voice_lines_destroy(&lines);
    chain_pitches_destroy(&pitches);
    chain_segments_destroy(&voice_segments);
    chain_segments_destroy(&chord_segments);
}

int main(void) {
    TestPiece piece;
    test_piece_init(&piece);
    test_segments_and_members(&piece);
    test_attack_strength(&piece);
    test_keys(&piece);
    test_pitches(&piece);
    test_stage_inputs(&piece);
    test_segment_lines(&piece);
    test_piece_destroy(&piece);
    printf("test_chain_inputs: ok\n");
    return 0;
}
