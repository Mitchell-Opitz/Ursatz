#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

/*
 * Meter and measure/beat location test. The five reference pieces
 * (Ursatz framework-v1 corpus) and two constructed files are imported,
 * and the meter section and the location of the chords covering chosen
 * positions (a chord segment may start before the position) are compared
 * with expectations worked out by hand from the files' MIDI ticks:
 * barlines from the declared time signatures (a change that arrives
 * mid-measure starts a new grid there), beats from the signature (a
 * quarter in 4/4, 3/4, 2/4 and 1/4; a dotted quarter in 12/8).
 *
 * The expectations describe what each FILE declares. Bach 114.mid is a
 * minuet but declares 4/4, so the measure numbers below follow the
 * declared 4/4, not the printed 3/4 score.
 *
 * Constructed files (division 96):
 *   pickup: 1/4 at tick 0, 4/4 at tick 96; a G pickup, then a C-E-G
 *           chord, D and C on beats 1-3 of measure 1.
 *   no_meter: no time signature event; five quarter notes.
 */

typedef struct {
    int measure; /* position some chord segment must cover */
    int beat;
    long sub_num;
    long sub_den;
} ChordExpectation;

typedef struct {
    int measure;
    int numerator;
    int denominator;
} ChangeExpectation;

typedef struct {
    const char *name;
    const char *source;
    int numerator;
    int denominator;
    long measure_count;
    long last_measure;
    int pickup_beats; /* -1: no pickup */
    size_t chord_count;
    const ChordExpectation *chords;
    size_t chord_expectation_count;
    const ChangeExpectation *changes;
    size_t change_count;
} Expectation;

static const ChordExpectation BACH_CHORDS[] = {
    {1, 1, 0, 1}, {6, 2, 3, 4}, {12, 1, 1, 4}, {18, 2, 1, 2}, {24, 4, 1, 2}};
static const ChordExpectation CLEMENTI_CHORDS[] = {
    {1, 1, 0, 1}, {10, 2, 1, 2}, {20, 2, 0, 1}, {30, 1, 0, 1}, {38, 3, 0, 1}};
/* Field 1 opens with three beats of rest: its first chord is on beat 4 of measure 1. */
static const ChordExpectation FIELD_1_CHORDS[] = {
    {1, 4, 0, 1}, {37, 2, 1, 3}, {52, 4, 1, 3}, {67, 3, 0, 1}};
static const ChordExpectation FIELD_5_CHORDS[] = {
    {1, 1, 0, 1}, {12, 1, 0, 1}, {23, 2, 0, 1}, {33, 1, 2, 3}, {43, 3, 5, 32}};
static const ChordExpectation MOZART_CHORDS[] = {
    {1, 1, 0, 1},     {67, 4, 1, 4},  {134, 3, 1, 4}, {149, 1, 0, 1},
    {217, 2, 1, 4}, {256, 1, 0, 1}, {257, 1, 0, 1}, {338, 1, 0, 1}};
static const ChangeExpectation MOZART_CHANGES[] = {{149, 3, 4}, {256, 1, 4}, {257, 2, 4}};
static const ChordExpectation PICKUP_CHORDS[] = {{0, 4, 0, 1}, {1, 1, 0, 1}, {1, 2, 0, 1}, {1, 3, 0, 1}};
static const ChordExpectation NO_METER_CHORDS[] = {
    {1, 1, 0, 1}, {1, 2, 0, 1}, {1, 4, 0, 1}, {2, 1, 0, 1}};

static const Expectation PIECES[] = {
    {"Bach 114.mid", "file", 4, 4, 24, 24, -1, 155, BACH_CHORDS, 5, NULL, 0},
    {"Clementi 36.mid", "file", 4, 4, 38, 38, -1, 205, CLEMENTI_CHORDS, 5, NULL, 0},
    {"Field 1.mid", "file", 12, 8, 67, 67, -1, 268, FIELD_1_CHORDS, 4, NULL, 0},
    {"Field 5.mid", "file", 12, 8, 43, 43, -1, 170, FIELD_5_CHORDS, 5, NULL, 0},
    {"Mozart 545.mid", "file", 4, 4, 338, 338, -1, 1095, MOZART_CHORDS, 8, MOZART_CHANGES, 3},
};

static const Expectation PICKUP = {"pickup", "file", 4, 4, 2, 1, 1, 4, PICKUP_CHORDS, 4, NULL, 0};
static const Expectation NO_METER = {"no_meter", "inferred", 4, 4, 2, 2, -1, 5,
                                     NO_METER_CHORDS, 4, NULL, 0};

/* ---- Constructed files ---- */

static void append_bytes(unsigned char *buf, size_t *pos, const void *bytes, size_t len) {
    memcpy(buf + *pos, bytes, len);
    *pos += len;
}

static void append_u32(unsigned char *buf, size_t *pos, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                           (unsigned char)v};
    append_bytes(buf, pos, b, 4);
}

static void append_u16(unsigned char *buf, size_t *pos, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v >> 8), (unsigned char)v};
    append_bytes(buf, pos, b, 2);
}

static size_t build_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body,
                               size_t track_body_size) {
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, division);
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)track_body_size);
    append_bytes(buf, &pos, track_body, track_body_size);
    return pos;
}

static void write_fixture(const char *path, const unsigned char *track, size_t track_size) {
    unsigned char buf[512];
    size_t size = build_smf_type0(buf, 96, track, track_size);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(buf, 1, size, file) == size);
    fclose(file);
}

static void write_pickup_fixture(const char *path) {
    unsigned char track[] = {
        0x00, 0xFF, 0x58, 0x04, 0x01, 0x02, 0x18, 0x08, /* tick 0    1/4 */
        0x00, 0x90, 0x43, 0x64,                         /*           note-on  G4 (the pickup) */
        0x60, 0x80, 0x43, 0x40,                         /* tick 96   note-off G4 */
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, /*           4/4 */
        0x00, 0x90, 0x3C, 0x64, 0x00, 0x90, 0x40, 0x64, 0x00, 0x90, 0x43, 0x64, /* C-E-G */
        0x60, 0x80, 0x3C, 0x40, 0x00, 0x80, 0x40, 0x40, 0x00, 0x80, 0x43, 0x40, /* tick 192 off */
        0x00, 0x90, 0x3E, 0x64,                         /*           D4 */
        0x60, 0x80, 0x3E, 0x40,                         /* tick 288 off */
        0x00, 0x90, 0x3C, 0x64,                         /*           C4 */
        0x60, 0x80, 0x3C, 0x40,                         /* tick 384 off: the end of measure 1 */
        0x00, 0xFF, 0x2F, 0x00};
    write_fixture(path, track, sizeof(track));
}

static void write_no_meter_fixture(const char *path) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, 0x60, 0x80, 0x3C, 0x40, /* tick 0   C4 */
        0x00, 0x90, 0x3E, 0x64, 0x60, 0x80, 0x3E, 0x40, /* tick 96  D4 */
        0x00, 0x90, 0x40, 0x64, 0x60, 0x80, 0x40, 0x40, /* tick 192 E4 */
        0x00, 0x90, 0x41, 0x64, 0x60, 0x80, 0x41, 0x40, /* tick 288 F4 */
        0x00, 0x90, 0x43, 0x64, 0x60, 0x80, 0x43, 0x40, /* tick 384 G4: measure 2 */
        0x00, 0xFF, 0x2F, 0x00};
    write_fixture(path, track, sizeof(track));
}

/* ---- Checks ---- */

static const cJSON *member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    assert(item != NULL);
    return item;
}

static long number(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    assert(cJSON_IsNumber(item));
    return (long)item->valuedouble;
}

/* Orders positions: measure first, then beat plus the fraction of it. */
static double position_key(int measure, int beat, long sub_num, long sub_den) {
    return (double)measure * 1000.0 + (double)beat + (double)sub_num / (double)sub_den;
}

static double position_key_of(const cJSON *position) {
    assert(cJSON_IsObject(position));
    return position_key((int)number(position, "measure"), (int)number(position, "beat"),
                        number(member(position, "subdivision"), "num"),
                        number(member(position, "subdivision"), "den"));
}

static void check_piece(const char *path, const char *display_name, const Expectation *expected) {
    ImportedNotes notes;
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    AnalysisDocument *built = analysis_document_build(display_name, &notes, &report);
    assert(built != NULL);
    char *json = analysis_document_to_json(built);
    assert(json != NULL);
    char *markdown = markdown_report_render(built);
    assert(markdown != NULL);
    analysis_document_destroy(built);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);

    cJSON *document = cJSON_Parse(json);
    assert(document != NULL);

    const cJSON *meter = member(document, "meter");
    assert(strcmp(member(meter, "source")->valuestring, expected->source) == 0);
    assert(number(member(meter, "time_signature"), "numerator") == expected->numerator);
    assert(number(member(meter, "time_signature"), "denominator") == expected->denominator);
    assert(number(meter, "measure_count") == expected->measure_count);
    assert(number(meter, "last_measure") == expected->last_measure);

    const cJSON *pickup = member(meter, "pickup");
    if (expected->pickup_beats < 0) {
        assert(cJSON_IsNull(pickup));
    } else {
        assert(cJSON_IsObject(pickup));
        assert(number(member(pickup, "beats"), "num") == expected->pickup_beats);
        assert(number(member(pickup, "beats"), "den") == 1);
        assert(number(member(pickup, "length"), "num") == 1);
        assert(number(member(pickup, "length"), "den") == 4);
        assert(strstr(markdown, "- **Pickup:** 1 beat(s), numbered measure 0.\n") != NULL);
    }

    const cJSON *changes = member(meter, "changes");
    assert((size_t)cJSON_GetArraySize(changes) == expected->change_count);
    for (size_t i = 0; i < expected->change_count; i++) {
        const cJSON *change = cJSON_GetArrayItem(changes, (int)i);
        assert(number(change, "measure") == expected->changes[i].measure);
        assert(number(member(change, "time_signature"), "numerator") == expected->changes[i].numerator);
        assert(number(member(change, "time_signature"), "denominator") == expected->changes[i].denominator);
    }

    if (strcmp(expected->source, "inferred") == 0) {
        assert(strstr(markdown, "(inferred: the file has no time signature, so 4/4 is assumed)") != NULL);
    } else {
        assert(strstr(markdown, "(inferred") == NULL);
    }

    const cJSON *chords = member(document, "chords");
    assert((size_t)cJSON_GetArraySize(chords) == expected->chord_count);
    for (size_t i = 0; i < expected->chord_expectation_count; i++) {
        const ChordExpectation *chord = &expected->chords[i];
        double at = position_key(chord->measure, chord->beat, chord->sub_num, chord->sub_den);
        bool covered = false;
        const cJSON *claim;
        cJSON_ArrayForEach(claim, chords) {
            const cJSON *location = member(claim, "location");
            assert(cJSON_IsObject(member(location, "end")));
            if (position_key_of(member(location, "start")) <= at + 1e-9 &&
                at < position_key_of(member(location, "end")) - 1e-9) {
                covered = true;
                break;
            }
        }
        assert(covered);
    }

    /* Every claim, not only the chosen chords, carries a location. */
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        assert(cJSON_IsObject(member(member(chord, "location"), "start")));
        assert(cJSON_IsObject(member(member(chord, "location"), "end")));
    }
    assert(cJSON_IsObject(member(member(meter, "location"), "start")));
    const cJSON *key = cJSON_GetArrayItem(member(document, "keys"), 0);
    if (key) {
        assert(cJSON_IsObject(member(member(key, "location"), "start")));
    }

    /* The report shows measure and beat, not raw beats from the start of the file. */
    assert(strstr(markdown, "## Meter\n") != NULL);
    assert(strstr(markdown, "- Beat ") == NULL);
    assert(strstr(markdown, "| m. ") != NULL);

    cJSON_Delete(document);
    free(markdown);
    free(json);
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, PIECES[i].name);
        check_piece(path, PIECES[i].name, &PIECES[i]);
    }

    const char *pickup_path = "/tmp/ursatz_analyzer_pickup_fixture.mid";
    write_pickup_fixture(pickup_path);
    check_piece(pickup_path, "pickup.mid", &PICKUP);

    const char *no_meter_path = "/tmp/ursatz_analyzer_no_meter_fixture.mid";
    write_no_meter_fixture(no_meter_path);
    check_piece(no_meter_path, "no_meter.mid", &NO_METER);

    puts("test_measure_locations: ok");
    return 0;
}
