#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chain/analysis_chain.h"
#include "io/imported_realization.h"
#include "reading/reading_build.h"
#include "reading/reading_json.h"
#include "realization/realization_json.h"
#include "realization/work.h"
#include "ursatz_test_knowledge.h"

/*
 * The reading builder (R-629): Bach 114 and Mozart 545 imported, realized (R-628) and run through the chain with every stage: the claim
 * counts are the result's areas, every evidence id is a performer note of the realization, a cadence's arrival is its entry's time, the
 * reading round-trips through the codec to equal text and the read copy has no detail. `bach` as the argument runs Bach 114 only.
 */

#define BACH URSATZ_FIXTURES_DIR "/framework-v1-corpus/Bach 114.mid"
#define MOZART URSATZ_FIXTURES_DIR "/framework-v1-corpus/Mozart 545.mid"

static bool is_performer_note(const Movement *movement, const char *id) {
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        for (size_t n = 0; n < performer_part_note_count(part); n++) {
            if (strcmp(performer_part_note_at(part, n)->id, id) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* Every evidence id of the claim is a performer note of the movement. */
static void check_evidence(const Movement *movement, const ReadingClaim *claim) {
    for (size_t i = 0; i < claim->evidence.count; i++) {
        assert(is_performer_note(movement, claim->evidence.ids[i]));
        for (size_t j = i + 1; j < claim->evidence.count; j++) {
            assert(strcmp(claim->evidence.ids[i], claim->evidence.ids[j]) != 0);
        }
    }
}

static size_t chords_of(const ChainResult *result) {
    size_t count = 0;
    for (size_t e = 0; e < result->entry_count; e++) {
        count += result->entries[e].chord_label && strcmp(result->entries[e].chord_label, "no match") != 0;
    }
    return count;
}

static void check_piece(const char *path, const char *name, bool expect_cadences) {
    ImportedScore score;
    ImportedScoreError import_error = {{0}};
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &score, &import_error) == IMPORTED_SCORE_OK);
    Work *work = NULL;
    char(*note_ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &note_ids) == IMPORTED_SCORE_OK);
    const Movement *movement = work_movement_at(work, 0);
    char digest[17];
    assert(realization_digest(work, digest));

    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_ALL;
    config.refine_keys = true;
    config.knowledge = test_knowledge();
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run_score(&score, &config, &chain) == ANALYSIS_CHAIN_OK);
    const ChainResult *result = analysis_chain_result(chain);

    ReadingSource source;
    memset(&source, 0, sizeof(source));
    source.kind = READING_SOURCE_ANALYZER;
    snprintf(source.name, sizeof(source.name), "%s", "ursatz-analyzer");
    snprintf(source.producer_key, sizeof(source.producer_key), "%s", analysis_chain_producer_key());
    source.stages = ANALYSIS_CHAIN_STAGE_ALL;
    snprintf(source.knowledge_hash, sizeof(source.knowledge_hash), "%s", knowledge_snapshot_hash(test_knowledge()));
    ReadingTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.work, sizeof(target.work), "%s", work_id(work));
    snprintf(target.movement, sizeof(target.movement), "%s", movement_id(movement));
    snprintf(target.digest, sizeof(target.digest), "%s", digest);
    Reading *reading = NULL;
    assert(reading_build(chain, &source, &target, (const char(*)[REALIZATION_ID_MAX])note_ids, score.count, &reading) == REALIZATION_OK);
    assert(reading != NULL && reading_detail(reading) == chain);

    /* The claim counts are the result's areas. */
    assert(result->key_region_count > 0 && result->entry_count > 0 && (!expect_cadences || result->cadence_count > 0));
    assert(reading_region_count(reading) == result->key_region_count);
    assert(reading_chord_count(reading) == chords_of(result));
    assert(reading_cadence_count(reading) == result->cadence_count);
    assert(reading_group_count(reading) == result->section_count + result->theme_count + result->phrase_count + result->subphrase_count + result->idea_count);
    size_t occurrences = 0;
    for (size_t m = 0; m < result->motive_count; m++) {
        occurrences += result->motives[m].occurrence_count;
    }
    assert(reading_motive_occurrence_count(reading) == occurrences);
    assert(reading_meter_count(reading) == meter_map_region_count(score.meter.meter_map));
    assert(reading_line_count(reading) == (result->outer_line_count > 0 ? 2 : 0));
    ChainVoiceFault *faults = NULL;
    size_t fault_count = 0;
    chain_result_voice_faults(chain, &faults, &fault_count);
    assert(reading_voice_fault_count(reading) == fault_count);
    chain_voice_faults_free(faults, fault_count);

    /* Every claim rests on performer notes of the realization. */
    for (size_t i = 0; i < reading_region_count(reading); i++) {
        const ReadingRegion *region = reading_region_at(reading, i);
        check_evidence(movement, (const ReadingClaim *)region);
        assert(region->detail == i && region->material == result->key_regions[i].material && region->center == result->key_regions[i].tonic_pitch_class);
        assert(rational_cmp(region->start, result->key_regions[i].start) == 0 && rational_cmp(region->end, result->key_regions[i].end) == 0);
        assert(region->evidence.count > 0);
    }
    size_t chords_with_reading = 0;
    for (size_t i = 0; i < reading_chord_count(reading); i++) {
        const ReadingChord *chord = reading_chord_at(reading, i);
        check_evidence(movement, (const ReadingClaim *)chord);
        assert(chord->detail < result->entry_count && rational_cmp(chord->start, result->entries[chord->detail].time) == 0);
        assert(chord->evidence.count == result->entries[chord->detail].note_id_count && strcmp(chord->label, result->entries[chord->detail].chord_label) == 0);
        chords_with_reading += chord->has_reading;
    }
    assert(chords_with_reading > 0);
    /* A cadence arrives at its entry's time, and its type is the chain's. */
    for (size_t i = 0; i < reading_cadence_count(reading); i++) {
        const ReadingCadence *cadence = reading_cadence_at(reading, i);
        check_evidence(movement, (const ReadingClaim *)cadence);
        const AnalysisCadence *found = &result->cadences[i];
        assert(cadence->detail == i && rational_cmp(cadence->arrival, result->entries[found->arrival_first].time) == 0);
        assert(rational_cmp(cadence->start, cadence->arrival) == 0 && rational_cmp(cadence->end, cadence->start) >= 0);
        assert(strcmp(cadence_type_name(cadence->type), found->type) == 0 && cadence->evidence.count > 0);
    }
    for (size_t i = 0; i < reading_group_count(reading); i++) {
        const ReadingGroup *group = reading_group_at(reading, i);
        check_evidence(movement, (const ReadingClaim *)group);
        assert(group->parent == READING_NO_PARENT || group->parent < i);
        assert((group->kind == READING_GROUP_SECTION) == (group->parent == READING_NO_PARENT));
    }
    for (size_t i = 0; i < reading_motive_occurrence_count(reading); i++) {
        check_evidence(movement, (const ReadingClaim *)reading_motive_occurrence_at(reading, i));
    }
    for (size_t i = 0; i < reading_line_count(reading); i++) {
        const ReadingLine *line = reading_line_at(reading, i);
        check_evidence(movement, (const ReadingClaim *)line);
        assert(line->role == (i == 0 ? LINE_ROLE_MELODY : LINE_ROLE_BASS) && line->evidence.count > 0);
    }
    for (size_t i = 0; i < reading_voice_fault_count(reading); i++) {
        check_evidence(movement, (const ReadingClaim *)reading_voice_fault_at(reading, i));
    }
    assert(strcmp(reading_region_at(reading, 0)->id, "region-1") == 0);
    assert(reading_cadence_count(reading) == 0 || strcmp(reading_cadence_at(reading, 0)->id, "cadence-1") == 0);

    /* The reading round-trips through the codec; the read copy has no detail. */
    ReadingJsonError error;
    char *text = reading_to_json(reading, &error);
    assert(text != NULL);
    Reading *read = NULL;
    assert(reading_from_json(test_knowledge(), text, &read, &error) == REALIZATION_OK && read != NULL);
    char *again = reading_to_json(read, &error);
    assert(again != NULL && strcmp(text, again) == 0 && reading_detail(read) == NULL);
    assert(reading_region_count(read) == reading_region_count(reading) && reading_group_count(read) == reading_group_count(reading));
    printf("%s: %zu regions, %zu chords, %zu cadences, %zu groups, %zu occurrences, %zu faults\n", name, reading_region_count(reading),
           reading_chord_count(reading), reading_cadence_count(reading), reading_group_count(reading), reading_motive_occurrence_count(reading), fault_count);
    free(again);
    free(text);
    reading_destroy(read);
    reading_destroy(reading);
    analysis_chain_destroy(chain);
    work_destroy(work);
    free(note_ids);
    imported_score_destroy(&score);
}

/* A chain that ran without the perception stages gives a reading of its meters only. */
static void check_empty_result(void) {
    ImportedScore score;
    assert(imported_score_import_file(BACH, IMPORTED_FORMAT_AUTO, &score, NULL) == IMPORTED_SCORE_OK);
    Work *work = NULL;
    char(*note_ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &note_ids) == IMPORTED_SCORE_OK);
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_SEGMENTS;
    config.knowledge = test_knowledge();
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run_score(&score, &config, &chain) == ANALYSIS_CHAIN_OK);
    ReadingSource source;
    memset(&source, 0, sizeof(source));
    snprintf(source.name, sizeof(source.name), "%s", "ursatz-analyzer");
    ReadingTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.work, sizeof(target.work), "%s", "w1");
    snprintf(target.movement, sizeof(target.movement), "%s", "m1");
    Reading *reading = NULL;
    assert(reading_build(chain, &source, &target, (const char(*)[REALIZATION_ID_MAX])note_ids, score.count, &reading) == REALIZATION_OK);
    assert(reading_region_count(reading) == 0 && reading_chord_count(reading) == 0 && reading_cadence_count(reading) == 0 && reading_group_count(reading) == 0);
    assert(reading_meter_count(reading) == meter_map_region_count(score.meter.meter_map));
    /* A note index beyond the ids given is refused. */
    Reading *refused = (Reading *)1;
    config.stages = ANALYSIS_CHAIN_STAGE_ALL;
    config.refine_keys = true;
    AnalysisChain *full = NULL;
    assert(analysis_chain_run_score(&score, &config, &full) == ANALYSIS_CHAIN_OK);
    assert(reading_build(full, &source, &target, (const char(*)[REALIZATION_ID_MAX])note_ids, 3, &refused) == REALIZATION_ERR_BAD_VALUE && refused == NULL);
    assert(reading_build(NULL, &source, &target, (const char(*)[REALIZATION_ID_MAX])note_ids, score.count, &refused) == REALIZATION_ERR_NULL);
    analysis_chain_destroy(full);
    reading_destroy(reading);
    analysis_chain_destroy(chain);
    work_destroy(work);
    free(note_ids);
    imported_score_destroy(&score);
}

int main(int argc, char **argv) {
    check_piece(BACH, "Bach 114", false);
    if (argc < 2 || strcmp(argv[1], "bach") != 0) {
        check_piece(MOZART, "Mozart 545", true);
        check_empty_result();
    }
    printf("test_reading_build: ok\n");
    return 0;
}
