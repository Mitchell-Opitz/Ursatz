#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reading/reading.h"
#include "ursatz_test_knowledge.h"

/*
 * The reading's own checks (R-629): a claim is kept with copies of what it is given, ids are unique among all the kinds of claim, a
 * span does not end before it starts, evidence ids are realization ids, a group's parent is an earlier group, a confidence is from 0 to
 * 1; a person's reading holds no evidence and no producer key; the accessors give claims in the order they were added.
 */

static Rational at(long long n, long long d) {
    Rational value;
    assert(rational_make(n, d, &value) == RATIONAL_OK);
    return value;
}

static Reading *new_reading(ReadingSourceKind kind, const char *producer_key) {
    ReadingSource source;
    memset(&source, 0, sizeof(source));
    source.kind = kind;
    snprintf(source.name, sizeof(source.name), "%s", kind == READING_SOURCE_PERSON ? "michelle" : "ursatz-analyzer");
    snprintf(source.producer_key, sizeof(source.producer_key), "%s", producer_key);
    ReadingTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.work, sizeof(target.work), "%s", "w1");
    snprintf(target.movement, sizeof(target.movement), "%s", "m1");
    Reading *reading = NULL;
    assert(reading_create(&source, &target, &reading) == REALIZATION_OK);
    return reading;
}

static ReadingMeter meter(const char *id, long long start, long long end) {
    ReadingMeter claim;
    memset(&claim, 0, sizeof(claim));
    snprintf(claim.id, sizeof(claim.id), "%s", id);
    claim.start = at(start, 1);
    claim.end = at(end, 1);
    claim.detail = READING_NO_DETAIL;
    snprintf(claim.time_signature, sizeof(claim.time_signature), "%s", "4/4");
    return claim;
}

static void test_create(void) {
    ReadingSource source;
    memset(&source, 0, sizeof(source));
    snprintf(source.name, sizeof(source.name), "%s", "ursatz-analyzer");
    ReadingTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.work, sizeof(target.work), "%s", "w1");
    snprintf(target.movement, sizeof(target.movement), "%s", "m1");
    Reading *reading = NULL;
    assert(reading_create(&source, &target, &reading) == REALIZATION_OK && reading != NULL);
    assert(strcmp(reading_source(reading)->name, "ursatz-analyzer") == 0 && strcmp(reading_target(reading)->work, "w1") == 0);
    assert(reading_target(reading)->digest[0] == '\0' && reading_detail(reading) == NULL);
    assert(reading_set_target_digest(reading, "0123456789abcdef") == REALIZATION_OK && strcmp(reading_target(reading)->digest, "0123456789abcdef") == 0);
    assert(reading_set_target_digest(reading, "0123") == REALIZATION_ERR_BAD_VALUE && reading_set_target_digest(reading, NULL) == REALIZATION_ERR_NULL);
    reading_destroy(reading);

    Reading *refused = (Reading *)1;
    ReadingTarget bad_target = target;
    snprintf(bad_target.work, sizeof(bad_target.work), "%s", "1w");
    assert(reading_create(&source, &bad_target, &refused) == REALIZATION_ERR_BAD_ID);
    ReadingSource bad_source = source;
    snprintf(bad_source.producer_key, sizeof(bad_source.producer_key), "%s", "NOTHEX");
    assert(reading_create(&bad_source, &target, &refused) == REALIZATION_ERR_BAD_VALUE);
    bad_source = source;
    bad_source.kind = (ReadingSourceKind)7;
    assert(reading_create(&bad_source, &target, &refused) == REALIZATION_ERR_BAD_VALUE);
    ReadingTarget bad_digest = target;
    snprintf(bad_digest.digest, sizeof(bad_digest.digest), "%s", "xyz");
    assert(reading_create(&source, &bad_digest, &refused) == REALIZATION_ERR_BAD_VALUE);
    assert(reading_create(NULL, &target, &refused) == REALIZATION_ERR_NULL && reading_create(&source, &target, NULL) == REALIZATION_ERR_NULL);
    reading_destroy(NULL);
}

static void test_claims_are_copied(void) {
    Reading *reading = new_reading(READING_SOURCE_ANALYZER, "0123456789abcdef");
    char notes[2][REALIZATION_ID_MAX] = {"n1", "n2"};
    ReadingMeter claim = meter("meter-1", 0, 8);
    claim.evidence.ids = notes;
    claim.evidence.count = 2;
    claim.confidence = (ReadingConfidence){true, at(1, 2)};
    claim.detail = 3;
    assert(reading_add_meter(reading, &claim) == REALIZATION_OK);
    /* What was given can change: the reading holds its own copy. */
    snprintf(notes[0], sizeof(notes[0]), "%s", "changed");
    snprintf(claim.time_signature, sizeof(claim.time_signature), "%s", "6/8");
    const ReadingMeter *kept = reading_meter_at(reading, 0);
    assert(kept != NULL && reading_meter_count(reading) == 1 && strcmp(kept->time_signature, "4/4") == 0);
    assert(kept->evidence.count == 2 && strcmp(kept->evidence.ids[0], "n1") == 0 && strcmp(kept->evidence.ids[1], "n2") == 0);
    assert(kept->evidence.ids != notes && kept->detail == 3 && kept->confidence.present && rational_cmp(kept->confidence.value, at(1, 2)) == 0);
    assert(reading_meter_at(reading, 1) == NULL && reading_line_at(reading, 0) == NULL && reading_region_count(reading) == 0);
    /* Claims are kept in order, many of them (the id table grows). */
    for (int i = 2; i <= 200; i++) {
        char id[REALIZATION_ID_MAX];
        snprintf(id, sizeof(id), "meter-%d", i);
        ReadingMeter more = meter(id, i, i + 1);
        assert(reading_add_meter(reading, &more) == REALIZATION_OK);
    }
    assert(reading_meter_count(reading) == 200 && strcmp(reading_meter_at(reading, 199)->id, "meter-200") == 0);
    assert(strcmp(reading_meter_at(reading, 0)->id, "meter-1") == 0 && reading_meter_at(reading, 0)->evidence.count == 2);
    reading_destroy(reading);
}

static void test_add_checks(void) {
    Reading *reading = new_reading(READING_SOURCE_ANALYZER, "0123456789abcdef");
    ReadingMeter claim = meter("meter-1", 0, 8);
    assert(reading_add_meter(reading, &claim) == REALIZATION_OK);
    assert(reading_add_meter(reading, &claim) == REALIZATION_ERR_DUPLICATE_ID);
    /* The id is unique among every kind of claim. */
    ReadingLine line;
    memset(&line, 0, sizeof(line));
    snprintf(line.id, sizeof(line.id), "%s", "meter-1");
    line.start = at(0, 1);
    line.end = at(1, 1);
    line.role = LINE_ROLE_MELODY;
    assert(reading_add_line(reading, &line) == REALIZATION_ERR_DUPLICATE_ID);
    snprintf(line.id, sizeof(line.id), "%s", "line-1");
    assert(reading_add_line(reading, &line) == REALIZATION_OK);
    snprintf(line.id, sizeof(line.id), "%s", "2line");
    assert(reading_add_line(reading, &line) == REALIZATION_ERR_BAD_ID);
    snprintf(line.id, sizeof(line.id), "%s", "line-2");
    line.role = (LineRole)99;
    assert(reading_add_line(reading, &line) == REALIZATION_ERR_BAD_VALUE);
    /* A span that ends before it starts; a time with no denominator. */
    ReadingMeter backwards = meter("meter-2", 8, 0);
    assert(reading_add_meter(reading, &backwards) == REALIZATION_ERR_BAD_VALUE);
    ReadingMeter no_time = meter("meter-3", 0, 1);
    no_time.end.den = 0;
    assert(reading_add_meter(reading, &no_time) == REALIZATION_ERR_BAD_VALUE);
    /* Evidence has to name notes by realization ids. */
    char bad_notes[1][REALIZATION_ID_MAX] = {"1note"};
    ReadingMeter with_bad = meter("meter-4", 0, 1);
    with_bad.evidence.ids = bad_notes;
    with_bad.evidence.count = 1;
    assert(reading_add_meter(reading, &with_bad) == REALIZATION_ERR_BAD_ID);
    /* A confidence is from 0 to 1. */
    ReadingMeter sure = meter("meter-5", 0, 1);
    sure.confidence = (ReadingConfidence){true, at(3, 2)};
    assert(reading_add_meter(reading, &sure) == REALIZATION_ERR_BAD_VALUE);
    sure.confidence.value = at(-1, 2);
    assert(reading_add_meter(reading, &sure) == REALIZATION_ERR_BAD_VALUE);
    sure.confidence.value = at(1, 1);
    assert(reading_add_meter(reading, &sure) == REALIZATION_OK);
    /* Nothing was kept of the refused ones. */
    assert(reading_meter_count(reading) == 2 && reading_line_count(reading) == 1);
    assert(reading_add_meter(NULL, &claim) == REALIZATION_ERR_NULL && reading_add_meter(reading, NULL) == REALIZATION_ERR_NULL);
    reading_destroy(reading);
}

static void test_group_parents_and_kinds(void) {
    Reading *reading = new_reading(READING_SOURCE_ANALYZER, "0123456789abcdef");
    ReadingGroup group;
    memset(&group, 0, sizeof(group));
    snprintf(group.id, sizeof(group.id), "%s", "group-1");
    group.start = at(0, 1);
    group.end = at(8, 1);
    group.kind = READING_GROUP_SECTION;
    group.parent = 0; /* no group before this one */
    assert(reading_add_group(reading, &group) == REALIZATION_ERR_BAD_VALUE);
    group.parent = READING_NO_PARENT;
    assert(reading_add_group(reading, &group) == REALIZATION_OK);
    snprintf(group.id, sizeof(group.id), "%s", "group-2");
    group.kind = READING_GROUP_THEME;
    group.parent = 0;
    assert(reading_add_group(reading, &group) == REALIZATION_OK);
    snprintf(group.id, sizeof(group.id), "%s", "group-3");
    group.parent = 2; /* itself, not earlier */
    assert(reading_add_group(reading, &group) == REALIZATION_ERR_BAD_VALUE);
    group.parent = 1;
    group.kind = (ReadingGroupKind)9;
    assert(reading_add_group(reading, &group) == REALIZATION_ERR_BAD_VALUE);
    group.kind = READING_GROUP_PHRASE;
    group.part_cadence_count = 3;
    assert(reading_add_group(reading, &group) == REALIZATION_ERR_BAD_VALUE);
    group.part_cadence_count = 1;
    group.part_cadences[0] = (CadenceType)40;
    assert(reading_add_group(reading, &group) == REALIZATION_ERR_BAD_VALUE);
    group.part_cadences[0] = CADENCE_TYPE_HALF;
    assert(reading_add_group(reading, &group) == REALIZATION_OK);
    assert(reading_group_count(reading) == 3 && reading_group_at(reading, 2)->parent == 1 && reading_group_at(reading, 0)->parent == READING_NO_PARENT);
    reading_destroy(reading);
}

static void test_other_kinds(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    Reading *reading = new_reading(READING_SOURCE_ANALYZER, "0123456789abcdef");
    ReadingRegion region;
    memset(&region, 0, sizeof(region));
    snprintf(region.id, sizeof(region.id), "%s", "region-1");
    region.start = at(0, 1);
    region.end = at(4, 1);
    region.center = 12;
    region.material = knowledge_snapshot_material(knowledge, "omt.material.major");
    assert(reading_add_region(reading, &region) == REALIZATION_ERR_BAD_VALUE);
    region.center = 0;
    region.material = NULL;
    assert(reading_add_region(reading, &region) == REALIZATION_ERR_BAD_VALUE);
    region.material = knowledge_snapshot_material(knowledge, "omt.material.major");
    assert(reading_add_region(reading, &region) == REALIZATION_OK);
    ReadingCadence cadence;
    memset(&cadence, 0, sizeof(cadence));
    snprintf(cadence.id, sizeof(cadence.id), "%s", "cadence-1");
    cadence.start = at(3, 1);
    cadence.end = at(4, 1);
    cadence.type = CADENCE_TYPE_COUNT;
    cadence.arrival = at(3, 1);
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_ERR_BAD_VALUE);
    cadence.type = CADENCE_TYPE_PLAGAL;
    cadence.alternative_count = 5;
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_ERR_BAD_VALUE);
    cadence.alternative_count = 1;
    cadence.alternatives[0] = (CadenceType)77;
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_ERR_BAD_VALUE);
    cadence.alternatives[0] = CADENCE_TYPE_HALF;
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_OK);
    ReadingChord chord;
    memset(&chord, 0, sizeof(chord));
    snprintf(chord.id, sizeof(chord.id), "%s", "chord-1");
    chord.start = at(0, 1);
    chord.end = at(1, 1);
    chord.has_reading = true;
    chord.reading.root_pitch_class = 12;
    assert(reading_add_chord(reading, &chord) == REALIZATION_ERR_BAD_VALUE);
    chord.reading.root_pitch_class = 0;
    chord.reading.bass_pitch_class = 0;
    assert(reading_add_chord(reading, &chord) == REALIZATION_OK);
    ReadingMotiveOccurrence occurrence;
    memset(&occurrence, 0, sizeof(occurrence));
    snprintf(occurrence.id, sizeof(occurrence.id), "%s", "motive-1");
    occurrence.start = at(0, 1);
    occurrence.end = at(2, 1);
    assert(reading_add_motive_occurrence(reading, &occurrence) == REALIZATION_OK);
    ReadingVoiceFault fault;
    memset(&fault, 0, sizeof(fault));
    snprintf(fault.id, sizeof(fault.id), "%s", "fault-1");
    fault.start = at(0, 1);
    fault.end = at(1, 1);
    assert(reading_add_voice_fault(reading, &fault) == REALIZATION_OK);
    assert(reading_region_count(reading) == 1 && reading_chord_count(reading) == 1 && reading_cadence_count(reading) == 1);
    assert(reading_motive_occurrence_count(reading) == 1 && reading_voice_fault_count(reading) == 1 && reading_group_count(reading) == 0);
    reading_destroy(reading);
}

/* A person's reading: no producer key, no stages, no evidence; it holds regions and cadences and takes none of the chain's detail. */
static void test_person_reading(void) {
    Reading *reading = new_reading(READING_SOURCE_PERSON, "");
    assert(reading_source(reading)->kind == READING_SOURCE_PERSON && reading_source(reading)->producer_key[0] == '\0' && reading_source(reading)->stages == 0);
    ReadingRegion region;
    memset(&region, 0, sizeof(region));
    snprintf(region.id, sizeof(region.id), "%s", "region-1");
    region.start = at(0, 1);
    region.end = at(8, 1);
    region.center = 9;
    region.material = knowledge_snapshot_material(test_knowledge(), "omt.material.natural-minor");
    region.detail = READING_NO_DETAIL;
    assert(reading_add_region(reading, &region) == REALIZATION_OK);
    const ReadingRegion *kept = reading_region_at(reading, 0);
    assert(kept->evidence.count == 0 && kept->evidence.ids == NULL && !kept->confidence.present && kept->detail == READING_NO_DETAIL);
    assert(reading_set_detail(reading, NULL) == REALIZATION_OK && reading_detail(reading) == NULL && reading_set_detail(NULL, NULL) == REALIZATION_ERR_NULL);
    reading_destroy(reading);
}

int main(void) {
    test_create();
    test_claims_are_copied();
    test_add_checks();
    test_group_parents_and_kinds();
    test_other_kinds();
    test_person_reading();
    printf("test_reading: ok\n");
    return 0;
}
