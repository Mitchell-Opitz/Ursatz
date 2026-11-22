#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reading/reading.h"
#include "reading/reading_json.h"
#include "ursatz_test_knowledge.h"

/*
 * The reading codec (R-629): a reading with a claim of each kind (a chord reading of a C major triad from the snapshot among them)
 * round-trips to equal text; a person's reading with no evidence and no producer key too; the files it writes are the schema's; the
 * refusals name their paths; a reading read from a file has no detail.
 */

static Rational at(long long n, long long d) {
    Rational value;
    assert(rational_make(n, d, &value) == RATIONAL_OK);
    return value;
}

static ReadingSource analyzer_source(void) {
    ReadingSource source;
    memset(&source, 0, sizeof(source));
    source.kind = READING_SOURCE_ANALYZER;
    snprintf(source.name, sizeof(source.name), "%s", "ursatz-analyzer");
    snprintf(source.producer_key, sizeof(source.producer_key), "%s", "0123456789abcdef");
    source.stages = 255;
    snprintf(source.knowledge_hash, sizeof(source.knowledge_hash), "%s", "fedcba9876543210");
    return source;
}

static ReadingTarget target_of(const char *digest) {
    ReadingTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.work, sizeof(target.work), "%s", "w1");
    snprintf(target.movement, sizeof(target.movement), "%s", "m1");
    snprintf(target.digest, sizeof(target.digest), "%s", digest);
    return target;
}

static void head(ReadingClaim *claim, const char *id, long long start, long long end) {
    snprintf(claim->id, sizeof(claim->id), "%s", id);
    claim->start = at(start, 1);
    claim->end = at(end, 1);
    claim->detail = READING_NO_DETAIL;
}

/* A reading with one claim of each kind and a second group under the first. */
static Reading *build_reading(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    ReadingSource source = analyzer_source();
    ReadingTarget target = target_of("a1b2c3d4e5f60789");
    Reading *reading = NULL;
    assert(reading_create(&source, &target, &reading) == REALIZATION_OK);
    char notes[3][REALIZATION_ID_MAX] = {"piano-n1", "piano-n2", "piano-n3"};

    ReadingRegion region;
    memset(&region, 0, sizeof(region));
    head((ReadingClaim *)&region, "region-1", 0, 4);
    region.detail = 0;
    region.confidence = (ReadingConfidence){true, at(3, 4)};
    region.evidence.ids = notes;
    region.evidence.count = 3;
    region.center = 0;
    region.material = knowledge_snapshot_material(knowledge, "omt.material.major");
    region.strength = REGION_STRENGTH_ESTABLISHED;
    snprintf(region.label, sizeof(region.label), "%s", "C major");
    assert(reading_add_region(reading, &region) == REALIZATION_OK);

    /* A C major triad over bars 0 to 1, read as I with the snapshot's chord type, bass C; the second chord reads as no chord. */
    ReadingChord chord;
    memset(&chord, 0, sizeof(chord));
    head((ReadingClaim *)&chord, "chord-1", 0, 1);
    chord.detail = 0;
    chord.confidence = (ReadingConfidence){true, at(1, 1)};
    chord.evidence.ids = notes;
    chord.evidence.count = 2;
    SonorityPitches sonority = {{{at(0, 1)}, {at(1, 1)}}, (1u << 0) | (1u << 4) | (1u << 7), 0};
    assert(chord_reading_of_sonority(knowledge, sonority, 0, &chord.reading) && chord.reading.type != NULL);
    chord.reading.borrowed_from = knowledge_snapshot_material(knowledge, "omt.material.natural-minor");
    chord.reading.role = CHORD_ROLE_EMBELLISHING;
    chord.reading.has_expected = true;
    chord.reading.expected_root_pitch_class = 7;
    chord.reading.expected_type = chord.reading.type;
    chord.has_reading = true;
    snprintf(chord.numeral, sizeof(chord.numeral), "%s", "I");
    snprintf(chord.function, sizeof(chord.function), "%s", "tonic");
    snprintf(chord.label, sizeof(chord.label), "%s", "C major");
    assert(reading_add_chord(reading, &chord) == REALIZATION_OK);
    ReadingChord none;
    memset(&none, 0, sizeof(none));
    head((ReadingClaim *)&none, "chord-2", 1, 2);
    none.detail = 1;
    snprintf(none.label, sizeof(none.label), "%s", "no match");
    assert(reading_add_chord(reading, &none) == REALIZATION_OK);

    ReadingCadence cadence;
    memset(&cadence, 0, sizeof(cadence));
    head((ReadingClaim *)&cadence, "cadence-1", 3, 4);
    cadence.detail = 0;
    cadence.evidence.ids = notes;
    cadence.evidence.count = 1;
    cadence.type = CADENCE_TYPE_PERFECT_AUTHENTIC;
    cadence.arrival = at(7, 2);
    cadence.alternatives[0] = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    cadence.alternatives[1] = CADENCE_TYPE_HALF;
    cadence.alternative_count = 2;
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_OK);

    ReadingGroup group;
    memset(&group, 0, sizeof(group));
    head((ReadingClaim *)&group, "group-1", 0, 8);
    group.detail = 0;
    group.kind = READING_GROUP_THEME;
    group.parent = READING_NO_PARENT;
    snprintf(group.label, sizeof(group.label), "%s", "A");
    snprintf(group.form, sizeof(group.form), "%s", "sentence");
    snprintf(group.beginning, sizeof(group.beginning), "%s", "presentation");
    snprintf(group.ending, sizeof(group.ending), "%s", "continuation");
    snprintf(group.commonality, sizeof(group.commonality), "%s", "rhythm");
    group.first_measure = 1;
    group.last_measure = 8;
    group.phrase_count = 1;
    group.part_cadences[0] = CADENCE_TYPE_HALF;
    group.part_cadence_count = 1;
    group.archetypal = true;
    assert(reading_add_group(reading, &group) == REALIZATION_OK);
    head((ReadingClaim *)&group, "group-2", 0, 4);
    group.kind = READING_GROUP_PHRASE;
    group.parent = 0;
    group.fragmentation = true;
    group.part_cadence_count = 0;
    assert(reading_add_group(reading, &group) == REALIZATION_OK);

    ReadingMotiveOccurrence occurrence;
    memset(&occurrence, 0, sizeof(occurrence));
    head((ReadingClaim *)&occurrence, "motive-1", 0, 2);
    occurrence.detail = 0;
    occurrence.evidence.ids = notes;
    occurrence.evidence.count = 3;
    occurrence.motive = 2;
    snprintf(occurrence.kind, sizeof(occurrence.kind), "%s", "transposition");
    assert(reading_add_motive_occurrence(reading, &occurrence) == REALIZATION_OK);

    ReadingMeter meter;
    memset(&meter, 0, sizeof(meter));
    head((ReadingClaim *)&meter, "meter-1", 0, 8);
    snprintf(meter.time_signature, sizeof(meter.time_signature), "%s", "3/4");
    assert(reading_add_meter(reading, &meter) == REALIZATION_OK);

    ReadingLine line;
    memset(&line, 0, sizeof(line));
    head((ReadingClaim *)&line, "line-1", 0, 8);
    line.detail = 0;
    line.evidence.ids = notes;
    line.evidence.count = 3;
    line.role = LINE_ROLE_BASS;
    assert(reading_add_line(reading, &line) == REALIZATION_OK);

    ReadingVoiceFault fault;
    memset(&fault, 0, sizeof(fault));
    head((ReadingClaim *)&fault, "fault-1", 2, 3);
    fault.detail = 0;
    fault.evidence.ids = notes;
    fault.evidence.count = 2;
    snprintf(fault.rule, sizeof(fault.rule), "%s", "parallel-fifths");
    fault.hard = true;
    fault.measure = 3;
    assert(reading_add_voice_fault(reading, &fault) == REALIZATION_OK);
    return reading;
}

/* The person's reading: no producer key, no stages, no evidence, no confidence on a region and a cadence. */
static Reading *build_person_reading(void) {
    ReadingSource source;
    memset(&source, 0, sizeof(source));
    source.kind = READING_SOURCE_PERSON;
    snprintf(source.name, sizeof(source.name), "%s", "michelle");
    ReadingTarget target = target_of("a1b2c3d4e5f60789");
    Reading *reading = NULL;
    assert(reading_create(&source, &target, &reading) == REALIZATION_OK);
    ReadingRegion region;
    memset(&region, 0, sizeof(region));
    head((ReadingClaim *)&region, "region-1", 0, 8);
    region.center = 9;
    region.material = knowledge_snapshot_material(test_knowledge(), "omt.material.natural-minor");
    region.strength = REGION_STRENGTH_ESTABLISHED;
    assert(reading_add_region(reading, &region) == REALIZATION_OK);
    ReadingCadence cadence;
    memset(&cadence, 0, sizeof(cadence));
    head((ReadingClaim *)&cadence, "cadence-1", 7, 8);
    cadence.confidence = (ReadingConfidence){true, at(1, 2)};
    cadence.type = CADENCE_TYPE_HALF;
    cadence.arrival = at(7, 1);
    assert(reading_add_cadence(reading, &cadence) == REALIZATION_OK);
    return reading;
}

/* The schema test validates what the codec wrote: the documents go to files in the working directory. */
static void write_document(const char *name, const char *text) {
    char path[128];
    snprintf(path, sizeof(path), "reading_written_%s.json", name);
    FILE *file = fopen(path, "wb");
    assert(file != NULL && fwrite(text, 1, strlen(text), file) == strlen(text));
    fclose(file);
}

static void check_round_trip(const Reading *reading, const char *name) {
    ReadingJsonError error;
    char *text = reading_to_json(reading, &error);
    assert(text != NULL && text[strlen(text) - 1] == '\n');
    write_document(name, text);
    Reading *read = NULL;
    assert(reading_from_json(test_knowledge(), text, &read, &error) == REALIZATION_OK && read != NULL);
    char *again = reading_to_json(read, &error);
    assert(again != NULL && strcmp(text, again) == 0);
    /* The read copy keeps no detail, and the claims are where they were. */
    assert(reading_detail(read) == NULL);
    assert(reading_region_count(read) == reading_region_count(reading) && reading_chord_count(read) == reading_chord_count(reading));
    assert(reading_cadence_count(read) == reading_cadence_count(reading) && reading_group_count(read) == reading_group_count(reading));
    assert(reading_source(read)->kind == reading_source(reading)->kind && strcmp(reading_target(read)->digest, reading_target(reading)->digest) == 0);
    free(again);
    reading_destroy(read);
    free(text);
}

static void test_round_trip(void) {
    Reading *reading = build_reading();
    ReadingJsonError error;
    char *text = reading_to_json(reading, &error);
    assert(text != NULL);
    assert(strncmp(text, "{\n  \"format\": \"ursatz-reading\",\n  \"format_version\": 1,\n  \"source\": {", 66) == 0);
    assert(strstr(text, "\"kind\": \"analyzer\"") != NULL && strstr(text, "\"digest\": \"a1b2c3d4e5f60789\"") != NULL);
    assert(strstr(text, "\"material\": \"omt.material.major\"") != NULL && strstr(text, "\"strength\": \"established\"") != NULL);
    assert(strstr(text, "\"type\": \"perfect-authentic\"") != NULL && strstr(text, "\"imperfect-authentic\"") != NULL);
    assert(strstr(text, "\"role\": \"embellishing\"") != NULL && strstr(text, "\"borrowed_from\": \"omt.material.natural-minor\"") != NULL);
    assert(strstr(text, "\"parent\": null") != NULL && strstr(text, "\"parent\": 0") != NULL && strstr(text, "\"reading\": null") != NULL);
    assert(strstr(text, "\"confidence\": null") != NULL && strstr(text, "\"role\": \"bass\"") != NULL);
    free(text);
    check_round_trip(reading, "analyzer");
    reading_destroy(reading);

    /* A person's reading has no evidence and no producer key. */
    Reading *person = build_person_reading();
    text = reading_to_json(person, &error);
    assert(text != NULL && strstr(text, "\"kind\": \"person\"") != NULL && strstr(text, "\"producer_key\": \"\"") != NULL);
    assert(strstr(text, "\"evidence\": []") != NULL && strstr(text, "\"stages\": 0") != NULL);
    free(text);
    check_round_trip(person, "person");
    reading_destroy(person);

    /* An empty reading too. */
    ReadingSource source = analyzer_source();
    ReadingTarget target = target_of("0000000000000001");
    Reading *empty = NULL;
    assert(reading_create(&source, &target, &empty) == REALIZATION_OK);
    check_round_trip(empty, "empty");
    reading_destroy(empty);
}

static char *replace_once(const char *text, const char *from, const char *to) {
    const char *found = strstr(text, from);
    assert(found != NULL);
    size_t head_size = (size_t)(found - text);
    char *out = malloc(strlen(text) + strlen(to) + 1);
    assert(out != NULL);
    memcpy(out, text, head_size);
    strcpy(out + head_size, to);
    strcat(out, found + strlen(from));
    return out;
}

static void expect_refused(const char *text, RealizationResult result, const char *path, const char *message_part) {
    Reading *read = (Reading *)1;
    ReadingJsonError error;
    memset(&error, 0, sizeof(error));
    RealizationResult got = reading_from_json(test_knowledge(), text, &read, &error);
    if (got != result || strcmp(error.path, path) != 0 || (message_part && strstr(error.message, message_part) == NULL)) {
        fprintf(stderr, "expected %d at '%s' (%s), got %d at '%s': %s\n", (int)result, path, message_part ? message_part : "", (int)got, error.path, error.message);
    }
    assert(got == result && read == NULL);
    assert(strcmp(error.path, path) == 0);
    assert(message_part == NULL || strstr(error.message, message_part) != NULL);
}

static void test_refusals(void) {
    Reading *reading = build_reading();
    char *text = reading_to_json(reading, NULL);
    assert(text != NULL);
    char *edited = replace_once(text, "\"ursatz-reading\"", "\"ursatz-plan\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "format", "expected");
    free(edited);
    edited = replace_once(text, "\"format_version\": 1", "\"format_version\": 2");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "format_version", "version 1");
    free(edited);
    edited = replace_once(text, "\"material\": \"omt.material.major\"", "\"material\": \"omt.material.nope\"");
    expect_refused(edited, REALIZATION_ERR_UNKNOWN_REFERENCE, "regions[0].material", "no material");
    free(edited);
    const KnowledgeSnapshot *knowledge = test_knowledge();
    ReadingChord chord;
    memset(&chord, 0, sizeof(chord));
    SonorityPitches sonority = {{{at(0, 1)}, {at(1, 1)}}, (1u << 0) | (1u << 4) | (1u << 7), 0};
    assert(chord_reading_of_sonority(knowledge, sonority, 0, &chord.reading) && chord.reading.type != NULL);
    char wanted[160];
    snprintf(wanted, sizeof(wanted), "\"type\": \"%s\"", chord.reading.type->id);
    edited = replace_once(text, wanted, "\"type\": \"omt.chord.nope\"");
    expect_refused(edited, REALIZATION_ERR_UNKNOWN_REFERENCE, "chords[0].reading.type", "no chord type");
    free(edited);
    edited = replace_once(text, "\"type\": \"perfect-authentic\"", "\"type\": \"PAC\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "cadences[0].type", "unknown cadence type");
    free(edited);
    edited = replace_once(text, "\"parent\": 0", "\"parent\": 1");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "groups[1].start", "refused");
    free(edited);
    edited = replace_once(text, "\"id\": \"chord-2\"", "\"id\": \"chord-1\"");
    expect_refused(edited, REALIZATION_ERR_DUPLICATE_ID, "chords[1].id", "twice");
    free(edited);
    edited = replace_once(text, "\"id\": \"region-1\"", "\"id\": \"1region\"");
    expect_refused(edited, REALIZATION_ERR_BAD_ID, "regions[0].id", "valid id");
    free(edited);
    edited = replace_once(text, "\"piano-n1\"", "\"1piano\"");
    expect_refused(edited, REALIZATION_ERR_BAD_ID, "regions[0].evidence", "valid id");
    free(edited);
    expect_refused("{\"format\": \"ursatz-reading\", \"format_version\": 1}", REALIZATION_ERR_BAD_VALUE, "source", "missing");
    expect_refused("[1]", REALIZATION_ERR_BAD_VALUE, "", "object");
    expect_refused("not json", REALIZATION_ERR_BAD_VALUE, "", "JSON");
    free(text);

    /* A reading that names no realization version is not written. */
    ReadingSource source = analyzer_source();
    ReadingTarget target = target_of("");
    Reading *unknown = NULL;
    ReadingJsonError error;
    assert(reading_create(&source, &target, &unknown) == REALIZATION_OK);
    assert(reading_to_json(unknown, &error) == NULL && strstr(error.message, "names no realization version") != NULL);
    assert(reading_set_target_digest(unknown, "00112233aabbccdd") == REALIZATION_OK);
    char *written = reading_to_json(unknown, &error);
    assert(written != NULL);
    free(written);
    assert(reading_set_target_digest(unknown, "TOO SHORT") == REALIZATION_ERR_BAD_VALUE);
    reading_destroy(unknown);
    Reading *none = (Reading *)1;
    assert(reading_from_json(NULL, "{}", &none, NULL) == REALIZATION_ERR_NULL && none == NULL);
    reading_destroy(reading);
}

int main(void) {
    test_round_trip();
    test_refusals();
    printf("test_reading_json: ok\n");
    return 0;
}
