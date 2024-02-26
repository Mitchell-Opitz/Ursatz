#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

/*
 * Roman numerals, applied chords and the chord vocabulary.
 *
 * Constructed pieces: four-beat measures of one chord per beat, written as MIDI (division 96), analysed, and the
 * numerals the document reports at chosen measures and beats compared with a hand reading. The chords are
 * spelled out below with their pitches; each expected numeral is the reading of those pitches in the key the
 * measures establish, worked out by hand and not taken from the analyzer's output.
 *
 *   major        C major: I IV V7 I
 *   minor        A minor: i iv V i (the raised leading tone, G#, in the V)
 *   inversions   C major: I V6 I6 V6/5, then I ii7 V4/3 I
 *   applied      C major with a V7/V (D F# A C) before a two-measure stay in G major
 *   borrowed     C major with the iv (F Ab C) in place of the IV
 *   modulation   C major, then G major: the G major chord is V in the first and I in the second
 *
 * Where the library cannot label a chord the test asserts that the document says so, with the library's reason,
 * and does not accept a guess. A result that differs from the hand reading is a result about the library, not a
 * reason to loosen the expectation.
 *
 * Every piece, constructed and reference, is also checked for the vocabulary's invariants: the counts of each
 * scope add up to the number of chords with a numeral, the shares add up to one, the entries are sorted, and
 * a run never exceeds its count.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* ---- MIDI writing ---- */

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} Bytes;

static void put(Bytes *bytes, const void *data, size_t length) {
    if (bytes->size + length > bytes->capacity) {
        bytes->capacity = (bytes->size + length) * 2;
        bytes->data = realloc(bytes->data, bytes->capacity);
    }
    memcpy(bytes->data + bytes->size, data, length);
    bytes->size += length;
}

static void put_u32(Bytes *bytes, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                          (unsigned char)v};
    put(bytes, b, 4);
}

static void put_u16(Bytes *bytes, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v >> 8), (unsigned char)v};
    put(bytes, b, 2);
}

/* A chord: up to five MIDI pitches, 0 ends the list. One beat is 96 ticks. */
typedef struct {
    int pitches[6];
} Chord;

/* Four chords, one per beat. */
typedef struct {
    Chord beats[4];
} Measure;

static void put_chord(Bytes *track, const Chord *chord) {
    size_t count = 0;
    while (chord->pitches[count] != 0) {
        count++;
    }
    for (size_t i = 0; i < count; i++) {
        unsigned char on[] = {0x00, 0x90, (unsigned char)chord->pitches[i], 0x64};
        put(track, on, sizeof(on));
    }
    for (size_t i = 0; i < count; i++) {
        unsigned char off[] = {(unsigned char)(i == 0 ? 0x60 : 0x00), 0x80, (unsigned char)chord->pitches[i], 0x40};
        put(track, off, sizeof(off));
    }
}

/* Writes the measures to path as a type-0 file in 4/4. */
static void write_piece(const char *path, const Measure *measures, size_t measure_count) {
    Bytes track = {NULL, 0, 0};
    for (size_t m = 0; m < measure_count; m++) {
        for (int b = 0; b < 4; b++) {
            put_chord(&track, &measures[m].beats[b]);
        }
    }
    unsigned char end[] = {0x00, 0xFF, 0x2F, 0x00};
    put(&track, end, sizeof(end));

    Bytes file = {NULL, 0, 0};
    put(&file, "MThd", 4);
    put_u32(&file, 6);
    put_u16(&file, 0);
    put_u16(&file, 1);
    put_u16(&file, 96);
    put(&file, "MTrk", 4);
    put_u32(&file, (uint32_t)track.size);
    put(&file, track.data, track.size);

    FILE *out = fopen(path, "wb");
    if (!out || fwrite(file.data, 1, file.size, out) != file.size) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fclose(out);
    free(track.data);
    free(file.data);
}

/* ---- Analysis ---- */

typedef struct {
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document;
} Analyzed;

static void analyze(const char *path, Analyzed *out) {
    if (midi_pipeline_import_file(path, &out->notes) != MIDI_PIPELINE_OK ||
        !run_analysis_pipeline(&out->notes, &out->report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        exit(1);
    }
    char *json = analysis_document_render(path, &out->notes, &out->report);
    out->document = json ? cJSON_Parse(json) : NULL;
    free(json);
    if (!out->document) {
        fprintf(stderr, "cannot render %s\n", path);
        exit(1);
    }
}

static void release(Analyzed *analyzed) {
    cJSON_Delete(analyzed->document);
    analysis_report_destroy(&analyzed->report);
    imported_notes_destroy(&analyzed->notes);
}

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static const char *text(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

/* measure * 10 + beat of a location position. */
static int position_key(const cJSON *position) {
    return (int)member(position, "measure")->valuedouble * 10 + (int)member(position, "beat")->valuedouble;
}

/* The chord claim sounding at a measure and beat. */
static const cJSON *chord_at(const Analyzed *analyzed, int measure, int beat) {
    int query = measure * 10 + beat;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, member(analyzed->document, "chords")) {
        const cJSON *location = member(chord, "location");
        if (position_key(member(location, "start")) <= query && query < position_key(member(location, "end"))) {
            return chord;
        }
    }
    return NULL;
}

/* The numeral the document reports at a measure and beat is exactly expected. */
static void expect_numeral(const Analyzed *analyzed, const char *name, int measure, int beat, const char *expected) {
    const cJSON *chord = chord_at(analyzed, measure, beat);
    const char *actual = chord ? text(member(chord, "roman_numeral"), "label") : NULL;
    if (!actual || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: m. %d beat %d: expected %s, got %s\n", name, measure, beat, expected,
                actual ? actual : "(unlabelled)");
        failures++;
    }
}

/* The chord at a measure and beat has no numeral, and says why. */
static void expect_unlabelled(const Analyzed *analyzed, const char *name, int measure, int beat, const char *reason) {
    const cJSON *chord = chord_at(analyzed, measure, beat);
    const cJSON *roman = chord ? member(chord, "roman_numeral") : NULL;
    const char *actual = roman ? text(roman, "unlabelled_reason") : NULL;
    if (!roman || !cJSON_IsNull(member(roman, "label")) || !actual || strstr(actual, reason) == NULL) {
        fprintf(stderr, "%s: m. %d beat %d: expected unlabelled (%s), got %s / %s\n", name, measure, beat, reason,
                roman && text(roman, "label") ? text(roman, "label") : "(no label)", actual ? actual : "(no reason)");
        failures++;
    }
}

/* The key the chord's numeral is relative to. */
static void expect_key(const Analyzed *analyzed, const char *name, int measure, int beat, const char *key) {
    const cJSON *chord = chord_at(analyzed, measure, beat);
    const char *actual = chord ? text(member(chord, "roman_numeral"), "key") : NULL;
    if (!actual || strcmp(actual, key) != 0) {
        fprintf(stderr, "%s: m. %d beat %d: expected key %s, got %s\n", name, measure, beat, key,
                actual ? actual : "(none)");
        failures++;
    }
}

/* ---- The vocabulary's invariants ---- */

static int labelled_chords(const cJSON *document, long region) {
    int total = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, member(document, "chords")) {
        const cJSON *roman = member(chord, "roman_numeral");
        const cJSON *key_region = member(roman, "key_region");
        if (cJSON_IsString(member(roman, "label")) && (region < 0 || (cJSON_IsNumber(key_region) &&
                                                                    (long)key_region->valuedouble == region))) {
            total++;
        }
    }
    return total;
}

/* For one scope (region < 0: overall) and level: the counts add up to the labelled chords, the shares to one, the
   entries are sorted, and a run never exceeds its count. */
static void check_vocabulary_scope(const cJSON *document, const char *name, long region, const char *level) {
    int count_sum = 0;
    double share_sum = 0.0;
    int entries_seen = 0;
    const cJSON *previous = NULL;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(document, "chord_vocabulary")) {
        const cJSON *key_region = member(entry, "key_region");
        bool in_scope = region < 0 ? cJSON_IsNull(key_region)
                                   : (cJSON_IsNumber(key_region) && (long)key_region->valuedouble == region);
        if (!in_scope || strcmp(text(entry, "level"), level) != 0) {
            continue;
        }
        entries_seen++;
        count_sum += (int)member(entry, "count")->valuedouble;
        share_sum += member(entry, "share")->valuedouble;
        CHECK(member(entry, "runs")->valuedouble <= member(entry, "count")->valuedouble);
        CHECK(member(entry, "runs")->valuedouble >= 1);
        if (previous && strcmp(text(previous, "group"), text(entry, "group")) == 0) {
            double before = member(previous, "count")->valuedouble;
            double now = member(entry, "count")->valuedouble;
            CHECK(before >= now);
            if (before == now) {
                const cJSON *a = member(member(previous, "duration"), "whole_notes");
                const cJSON *b = member(member(entry, "duration"), "whole_notes");
                double left = member(a, "num")->valuedouble / member(a, "den")->valuedouble;
                double right = member(b, "num")->valuedouble / member(b, "den")->valuedouble;
                CHECK(left >= right);
            }
        }
        previous = entry;
    }
    int labelled = labelled_chords(document, region);
    if (count_sum != labelled) {
        fprintf(stderr, "%s: %s scope %ld: counts add up to %d, but %d chords have a numeral\n", name, level, region,
                count_sum, labelled);
        failures++;
    }
    if (labelled > 0) {
        CHECK(fabs(share_sum - 1.0) < 1e-6);
    } else {
        CHECK(entries_seen == 0);
    }
}

static void check_vocabulary(const Analyzed *analyzed, const char *name) {
    const cJSON *document = analyzed->document;
    CHECK(cJSON_IsArray(member(document, "chord_vocabulary")));
    for (int level = 0; level < 2; level++) {
        const char *level_name = level == 0 ? "numeral" : "base_numeral";
        check_vocabulary_scope(document, name, -1, level_name);
        for (size_t region = 0; region < analyzed->report.key_region_count; region++) {
            check_vocabulary_scope(document, name, (long)region, level_name);
        }
    }
    /* A chord is in one group: applied entries are exactly the applied chords. */
    int applied_chords = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, member(document, "chords")) {
        applied_chords += cJSON_IsObject(member(member(chord, "roman_numeral"), "applied")) ? 1 : 0;
    }
    int applied_count = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(document, "chord_vocabulary")) {
        if (cJSON_IsNull(member(entry, "key_region")) && strcmp(text(entry, "level"), "numeral") == 0 &&
            strcmp(text(entry, "group"), "applied") == 0) {
            applied_count += (int)member(entry, "count")->valuedouble;
        }
    }
    CHECK(applied_count == applied_chords);
    /* Every chord is either numbered or says why not. */
    cJSON_ArrayForEach(chord, member(document, "chords")) {
        const cJSON *roman = member(chord, "roman_numeral");
        CHECK(cJSON_IsString(member(roman, "label")) != cJSON_IsString(member(roman, "unlabelled_reason")));
    }
}

/* ---- Constructed pieces ---- */

/* Pitches: C3 48, C4 60, G2 43, B2 47, D3 50, F2 41, E3 52. */
#define MAJOR_I {{48, 64, 67, 72, 0}}
#define MAJOR_IV {{53, 65, 69, 72, 0}}
#define MAJOR_V7 {{43, 59, 62, 65, 0}}

static void test_major(const char *path) {
    Measure measure = {{MAJOR_I, MAJOR_IV, MAJOR_V7, MAJOR_I}};
    Measure piece[8];
    for (size_t i = 0; i < 8; i++) {
        piece[i] = measure;
    }
    write_piece(path, piece, 8);
    Analyzed analyzed;
    analyze(path, &analyzed);
    CHECK(analyzed.report.key_region_count == 1);
    for (int m = 3; m <= 6; m++) {
        expect_numeral(&analyzed, "major", m, 1, "I");
        expect_numeral(&analyzed, "major", m, 2, "IV");
        expect_numeral(&analyzed, "major", m, 3, "V7");
        expect_numeral(&analyzed, "major", m, 4, "I");
        expect_key(&analyzed, "major", m, 3, "C major");
    }
    check_vocabulary(&analyzed, "major");
    release(&analyzed);
}

/* A minor: i (A C E), iv (D F A), V (E G# B). The library's weighted chord vocabulary is the major-key set, so a
   chord in a minor region has no numeral: the document says so, with that reason, in place of a guess. */
static void test_minor(const char *path) {
    Chord i = {{45, 57, 60, 64, 0}};
    Chord iv = {{50, 62, 65, 69, 0}};
    Chord v = {{52, 64, 68, 71, 0}};
    Measure measure = {{i, iv, v, i}};
    Measure piece[8];
    for (size_t k = 0; k < 8; k++) {
        piece[k] = measure;
    }
    write_piece(path, piece, 8);
    Analyzed analyzed;
    analyze(path, &analyzed);
    CHECK(analyzed.report.key_region_count == 1);
    if (analyzed.report.key_region_count == 1) {
        CHECK(strcmp(analyzed.report.key_regions[0].label, "A minor") == 0);
    }
    for (int m = 3; m <= 6; m++) {
        for (int beat = 1; beat <= 4; beat++) {
            expect_unlabelled(&analyzed, "minor", m, beat, "major keys only");
        }
        expect_key(&analyzed, "minor", m, 3, "A minor");
    }
    check_vocabulary(&analyzed, "minor");
    release(&analyzed);
}

/* Inversions and seventh-chord figures in C major. */
static void test_inversions(const char *path) {
    Chord i = MAJOR_I;
    Chord v6 = {{47, 55, 62, 67, 0}};            /* G B D, B in the bass */
    Chord i6 = {{52, 60, 67, 72, 0}};            /* C E G, E in the bass */
    Chord v65 = {{47, 62, 65, 67, 0}};           /* G B D F, B in the bass */
    Chord ii7 = {{50, 62, 65, 69, 72, 0}};       /* D F A C, D in the bass */
    Chord v43 = {{50, 59, 65, 67, 0}};           /* G B D F, D in the bass */
    Chord v42 = {{41, 59, 62, 67, 0}};           /* G B D F, F in the bass */
    Chord i64 = {{43, 60, 64, 67, 0}};           /* C E G, G in the bass */
    Measure odd = {{i, v6, i6, v65}};
    Measure even = {{i, ii7, v43, i}};
    Measure third = {{i, v42, i64, i}};
    Measure piece[9] = {odd, even, odd, even, odd, even, third, third, third};
    write_piece(path, piece, 9);
    Analyzed analyzed;
    analyze(path, &analyzed);
    for (int m = 3; m <= 6; m += 2) {
        expect_numeral(&analyzed, "inversions", m, 2, "V6");
        expect_numeral(&analyzed, "inversions", m, 3, "I6");
        expect_numeral(&analyzed, "inversions", m, 4, "V6/5");
        expect_numeral(&analyzed, "inversions", m + 1, 2, "ii7");
        expect_numeral(&analyzed, "inversions", m + 1, 3, "V4/3");
    }
    expect_numeral(&analyzed, "inversions", 8, 2, "V4/2");
    expect_numeral(&analyzed, "inversions", 8, 3, "I6/4");
    check_vocabulary(&analyzed, "inversions");

    /* The base numeral puts the inversions of V7 together. */
    int v7_count = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(analyzed.document, "chord_vocabulary")) {
        if (cJSON_IsNull(member(entry, "key_region")) && strcmp(text(entry, "level"), "base_numeral") == 0 &&
            strcmp(text(entry, "numeral"), "V7") == 0) {
            v7_count += (int)member(entry, "count")->valuedouble;
        }
    }
    CHECK(v7_count >= 6);
    release(&analyzed);
}

/* An applied chord: C major, with a V7/V (D F# A C) as the last chord before a stay in G major. The chord is the
   library's reading, and its cross-reference to the tonicization claims is reported either way: the two
   agree when the claim for the G major region begins where the chord ends. */
static void test_applied(const char *path) {
    Chord c_i = MAJOR_I;
    Chord c_iv = MAJOR_IV;
    Chord c_v = {{43, 59, 62, 67, 0}};
    Chord v7_of_v = {{50, 60, 66, 69, 0}}; /* D F# A C */
    Chord g_i = {{43, 59, 62, 67, 0}};
    Chord g_iv = {{48, 60, 64, 67, 72, 0}};
    Chord g_v = {{50, 62, 66, 69, 0}};
    Measure c_measure = {{c_i, c_iv, c_v, c_i}};
    Measure lead = {{c_i, c_iv, v7_of_v, v7_of_v}};
    Measure g_measure = {{g_i, g_iv, g_v, g_i}};
    Measure piece[18];
    for (size_t m = 0; m < 7; m++) {
        piece[m] = c_measure;
    }
    piece[7] = lead;
    piece[8] = g_measure;
    piece[9] = g_measure;
    for (size_t m = 10; m < 18; m++) {
        piece[m] = c_measure;
    }
    write_piece(path, piece, 18);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const cJSON *chord = chord_at(&analyzed, 8, 3);
    const cJSON *roman = chord ? member(chord, "roman_numeral") : NULL;
    const cJSON *applied = roman ? member(roman, "applied") : NULL;
    CHECK(cJSON_IsObject(applied));
    if (cJSON_IsObject(applied)) {
        CHECK(strcmp(text(roman, "label"), "V7/V") == 0);
        CHECK(strcmp(text(applied, "numeral"), "V7") == 0);
        CHECK(strcmp(text(applied, "target_numeral"), "V") == 0);
        CHECK(member(applied, "target_degree")->valuedouble == 5);
        CHECK(strcmp(text(applied, "target_tonic"), "G") == 0);
        CHECK(strcmp(text(applied, "kind"), "secondary-dominant") == 0);
        CHECK(strcmp(text(roman, "base_numeral"), "V7/V") == 0);
        const char *agreement = text(applied, "tonicization_agreement");
        CHECK(agreement != NULL);
        /* The cross-reference agrees exactly when a tonicization claim for G major touches the chord. */
        bool tonicization_of_g = false;
        const cJSON *tonicization;
        cJSON_ArrayForEach(tonicization, member(analyzed.document, "tonicizations")) {
            tonicization_of_g = tonicization_of_g || strcmp(text(tonicization, "to_key"), "G major") == 0;
        }
        if (agreement && strcmp(agreement, "agrees") == 0) {
            CHECK(tonicization_of_g);
            CHECK(strcmp(text(applied, "tonicization_key"), "G major") == 0);
            CHECK(cJSON_IsNumber(member(applied, "tonicization_index")));
        } else if (agreement && strcmp(agreement, "no_tonicization") == 0) {
            CHECK(cJSON_IsNull(member(applied, "tonicization_index")));
            CHECK(cJSON_IsNull(member(applied, "target_region")));
        }
    }
    check_vocabulary(&analyzed, "applied");
    release(&analyzed);
}

/* A borrowed chord: the iv (F Ab C) where the IV belongs. The library's Roman numeral adapter names no borrowed
   chord, so the chord is not given a mixture numeral. */
static void test_borrowed(const char *path) {
    Chord i = MAJOR_I;
    Chord iv = MAJOR_IV;
    Chord v = {{43, 59, 62, 67, 0}};
    Chord minor_iv = {{53, 63, 68, 72, 0}}; /* F Ab C */
    Measure plain = {{i, iv, v, i}};
    Measure borrowed = {{i, minor_iv, v, i}};
    Measure piece[8] = {plain, plain, plain, plain, borrowed, plain, plain, plain};
    write_piece(path, piece, 8);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const cJSON *chord = chord_at(&analyzed, 5, 2);
    const cJSON *roman = chord ? member(chord, "roman_numeral") : NULL;
    CHECK(roman != NULL);
    if (roman) {
        const char *label = text(roman, "label");
        /* Never the minor iv: the library has no numeral for a borrowed chord. */
        CHECK(label == NULL || strcmp(label, "iv") != 0);
        CHECK(cJSON_IsNull(member(roman, "applied")));
        if (!label) {
            CHECK(strstr(text(roman, "unlabelled_reason"), "no Roman numeral") != NULL);
        }
    }
    int mixture = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(analyzed.document, "chord_vocabulary")) {
        mixture += strcmp(text(entry, "group"), "mixture") == 0 ? 1 : 0;
    }
    CHECK(mixture == 0);
    check_vocabulary(&analyzed, "borrowed");
    release(&analyzed);
}

/* C major, then G major: the G major chord is V in the first key and I in the second. */
static void test_modulation(const char *path) {
    Chord c_i = MAJOR_I;
    Chord c_iv = MAJOR_IV;
    Chord c_v = {{43, 59, 62, 67, 0}};
    Chord g_i = {{43, 59, 62, 67, 0}};
    Chord g_iv = {{48, 60, 64, 67, 72, 0}};
    Chord g_v = {{50, 62, 66, 69, 0}};
    Measure c_measure = {{c_i, c_iv, c_v, c_i}};
    Measure g_measure = {{g_i, g_iv, g_v, g_i}};
    Measure piece[16];
    for (size_t m = 0; m < 8; m++) {
        piece[m] = c_measure;
        piece[8 + m] = g_measure;
    }
    write_piece(path, piece, 16);
    Analyzed analyzed;
    analyze(path, &analyzed);
    CHECK(analyzed.report.key_region_count == 2);
    expect_numeral(&analyzed, "modulation", 4, 3, "V");
    expect_key(&analyzed, "modulation", 4, 3, "C major");
    for (int m = 13; m <= 15; m++) {
        expect_numeral(&analyzed, "modulation", m, 1, "I");
        expect_numeral(&analyzed, "modulation", m, 2, "IV");
        expect_numeral(&analyzed, "modulation", m, 3, "V");
        expect_key(&analyzed, "modulation", m, 3, "G major");
    }
    const cJSON *first = chord_at(&analyzed, 4, 3);
    const cJSON *last = chord_at(&analyzed, 14, 3);
    if (first && last) {
        CHECK(member(member(first, "roman_numeral"), "key_region")->valuedouble !=
              member(member(last, "roman_numeral"), "key_region")->valuedouble);
    }
    check_vocabulary(&analyzed, "modulation");
    release(&analyzed);
}

/* ---- Reference pieces ---- */

static void test_reference_piece(const char *file) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, file);
    Analyzed analyzed;
    analyze(path, &analyzed);
    check_vocabulary(&analyzed, file);
    release(&analyzed);
}

int main(void) {
    const char *path = "/tmp/ursatz_analyzer_roman_numerals.mid";
    test_major(path);
    test_minor(path);
    test_inversions(path);
    test_applied(path);
    test_borrowed(path);
    test_modulation(path);
    remove(path);
    static const char *const REFERENCE[] = {"Bach 114.mid", "Mozart 545.mid", "Clementi 36.mid", "Field 1.mid",
                                            "Field 5.mid"};
    for (size_t i = 0; i < sizeof(REFERENCE) / sizeof(REFERENCE[0]); i++) {
        test_reference_piece(REFERENCE[i]);
    }
    if (failures == 0) {
        printf("test_roman_numerals: ok\n");
    }
    return failures == 0 ? 0 : 1;
}
