#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chain/analysis_chain.h"
#include "io/imported_realization.h"
#include "plan/plan_constraint.h"
#include "plan/plan_json.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "reading/reading_build.h"
#include "reading/reading_json.h"
#include "reading/reading_plan.h"
#include "realization/realization_json.h"
#include "realization/work.h"
#include "test_knowledge.h"
#include "ursatz_test.h"

/*
 * A reading made into a plan (R-635): the Analyzer's reading of Mozart 545 gives a plan whose sections, key spans and harmony spans follow
 * the claims (a section for each top-level section claim, every chord claim with a numeral a harmony span, the key spans covering the region
 * claims), that plan_to_json writes and plan_from_json reads back to equal text, with `derived_from` naming the reading and the realization;
 * Michelle's reading of the same movement gives one section and her 10 regions; a reading with no region is refused.
 */

#define MOZART URSATZ_FIXTURES_DIR "/framework-v1-corpus/Mozart 545.mid"

static PlanProducer producer(void) {
    PlanProducer stamp;
    memset(&stamp, 0, sizeof(stamp));
    snprintf(stamp.name, sizeof(stamp.name), "test");
    snprintf(stamp.version, sizeof(stamp.version), "1");
    return stamp;
}

static size_t spans_of(const Plan *plan, PlanLaneKind lane) {
    size_t count = 0;
    for (size_t id = 1; id <= plan_lane_span_count(plan); id++) {
        count += plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == lane;
    }
    return count;
}

/* Every span of the lane is Given, owned by a node that holds it, and the spans do not overlap. */
static void check_lane(const Plan *plan, PlanLaneKind lane) {
    Rational cursor = {0, 1};
    PlanSpan all = {{0, 1}, plan_node_span(plan, plan_root(plan)).length};
    LaneSpanId ids[2048];
    size_t found = plan_lane_find(plan, lane, 0, all, ids, 2048);
    assert(found == spans_of(plan, lane));
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN);
        assert(plan_span_contains(plan_node_span(plan, plan_lane_span_owner(plan, ids[i])), span));
        assert(rational_cmp(span.start, cursor) >= 0);
        assert(plan_span_end(span, &cursor) == PLAN_OK);
    }
}

static size_t section_claims(const Reading *reading) {
    size_t count = 0;
    for (size_t i = 0; i < reading_group_count(reading); i++) {
        count += reading_group_at(reading, i)->kind == READING_GROUP_SECTION && reading_group_at(reading, i)->parent == READING_NO_PARENT;
    }
    return count;
}

static size_t sections_of(const Plan *plan) {
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, plan_root(plan)); i++) {
        count += plan_node_kind(plan, plan_node_child_at(plan, plan_root(plan), i)) == PLAN_NODE_SECTION;
    }
    return count;
}

static void expect_round_trip(const Plan *plan, const Reading *reading) {
    PlanProducer stamp = producer();
    PlanJsonError error;
    char *text = plan_to_json(plan, &stamp, &error);
    assert(text);
    Plan *read = NULL;
    assert(plan_from_json(test_knowledge(), text, NULL, &read, &error) == PLAN_OK);
    char *again = plan_to_json(read, &stamp, &error);
    assert(again && strcmp(text, again) == 0);
    const PlanDerivation *derivation = plan_derived_from(read);
    assert(derivation && strcmp(derivation->reading, reading_source(reading)->name) == 0 &&
           strcmp(derivation->realization, reading_target(reading)->digest) == 0);
    free(text);
    free(again);
    plan_destroy(read);
}

static void test_analyzer_reading(void) {
    ImportedScore score;
    ImportedScoreError import_error = {{0}};
    assert(imported_score_import_file(MOZART, IMPORTED_FORMAT_AUTO, &score, &import_error) == IMPORTED_SCORE_OK);
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

    Plan *plan = NULL;
    ReadingPlanError error;
    assert(reading_to_plan(reading, movement, test_knowledge(), &plan, &error) == REALIZATION_OK && plan);
    assert(plan_node_kind(plan, plan_root(plan)) == PLAN_NODE_FORM && strcmp(plan_node_name(plan, plan_root(plan)), "form") == 0);

    /* One section for each section claim, one harmony span for each chord claim with a numeral (a chord a section boundary cuts is two). */
    assert(sections_of(plan) == section_claims(reading) && sections_of(plan) > 1);
    size_t numerals = 0;
    for (size_t i = 0; i < reading_chord_count(reading); i++) {
        numerals += reading_chord_at(reading, i)->numeral[0] != '\0';
    }
    assert(numerals > 0 && spans_of(plan, PLAN_LANE_HARMONY) >= numerals && spans_of(plan, PLAN_LANE_HARMONY) <= numerals + sections_of(plan));
    assert(spans_of(plan, PLAN_LANE_REGION) >= reading_region_count(reading));
    assert(spans_of(plan, PLAN_LANE_METER) == meter_map_region_count(movement_meter_map(movement)));
    assert(spans_of(plan, PLAN_LANE_MELODY) + spans_of(plan, PLAN_LANE_BASS) + spans_of(plan, PLAN_LANE_INNER_VOICE) == 0); /* no notes */
    check_lane(plan, PLAN_LANE_METER);
    check_lane(plan, PLAN_LANE_REGION);
    check_lane(plan, PLAN_LANE_HARMONY);

    /* The form ends on a barline at or after the last note. */
    Rational end = plan_node_span(plan, plan_root(plan)).length;
    int barline = 0;
    MusicalTime at = musical_time_of(end);
    assert(meter_map_measure_index_at(movement_meter_map(movement), &at, &barline) == METER_MAP_OK);
    TimeSpan measure;
    assert(meter_map_measure_span(movement_meter_map(movement), barline, &measure) == METER_MAP_OK && rational_cmp(measure.start.value, end) == 0);

    /* A section holds the cadence heard in its last bar. */
    size_t with_cadence = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, plan_root(plan)); i++) {
        NodeId node = plan_node_child_at(plan, plan_root(plan), i);
        with_cadence += plan_section_or_empty(plan, node)->has_cadence;
    }
    assert(with_cadence > 0);

    expect_round_trip(plan, reading);
    plan_destroy(plan);
    reading_destroy(reading);
    analysis_chain_destroy(chain);
    free(note_ids);
    work_destroy(work);
    imported_score_destroy(&score);
}

static void test_person_reading(void) {
    ImportedScore score;
    ImportedScoreError import_error = {{0}};
    assert(imported_score_import_file(MOZART, IMPORTED_FORMAT_AUTO, &score, &import_error) == IMPORTED_SCORE_OK);
    Work *work = NULL;
    char(*note_ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &note_ids) == IMPORTED_SCORE_OK);
    const Movement *movement = work_movement_at(work, 0);

    char *text = test_must_read_file(URSATZ_FIXTURES_DIR "/readings/mozart-545.michelle.json", NULL);
    Reading *reading = NULL;
    ReadingJsonError json_error;
    assert(reading_from_json(test_knowledge(), text, &reading, &json_error) == REALIZATION_OK);
    free(text);
    assert(reading_region_count(reading) == 10);

    Plan *plan = NULL;
    ReadingPlanError error;
    assert(reading_to_plan(reading, movement, test_knowledge(), &plan, &error) == REALIZATION_OK);
    assert(sections_of(plan) == 1 && spans_of(plan, PLAN_LANE_REGION) == 10 && spans_of(plan, PLAN_LANE_HARMONY) == 0);
    NodeId section = plan_node_child_at(plan, plan_root(plan), 0);
    assert(strcmp(plan_node_name(plan, section), "A") == 0 && strcmp(plan_section_or_empty(plan, section)->letter, "A") == 0);
    assert(rational_cmp(plan_node_span(plan, section).length, plan_node_span(plan, plan_root(plan)).length) == 0);
    check_lane(plan, PLAN_LANE_REGION);
    expect_round_trip(plan, reading);
    plan_destroy(plan);

    /* A reading with no region names no key: refused with the reason, and the plan is untouched. */
    Reading *empty = NULL;
    assert(reading_create(reading_source(reading), reading_target(reading), &empty) == REALIZATION_OK);
    Plan *none = NULL;
    memset(&error, 0, sizeof(error));
    assert(reading_to_plan(empty, movement, test_knowledge(), &none, &error) == REALIZATION_ERR_BAD_VALUE && none == NULL);
    assert(strcmp(error.message, "the reading names no region") == 0);
    reading_destroy(empty);

    /* A reading that does not name the version of its realization is refused too. */
    Reading *unnamed = NULL;
    ReadingTarget target = *reading_target(reading);
    target.digest[0] = '\0';
    assert(reading_create(reading_source(reading), &target, &unnamed) == REALIZATION_OK);
    assert(reading_add_region(unnamed, reading_region_at(reading, 0)) == REALIZATION_OK);
    assert(reading_to_plan(unnamed, movement, test_knowledge(), &none, &error) == REALIZATION_ERR_BAD_VALUE && none == NULL);
    reading_destroy(unnamed);

    reading_destroy(reading);
    free(note_ids);
    work_destroy(work);
    imported_score_destroy(&score);
}

int main(void) {
    test_analyzer_reading();
    test_person_reading();
    test_knowledge_release();
    puts("reading plan: ok");
    return 0;
}
