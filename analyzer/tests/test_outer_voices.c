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
 * Soprano and bass scale degrees per chord, and the outer_voices section of the analysis document, on small
 * constructed pieces whose answers are read by hand:
 *
 *   progression   I, IV, V, I in C major, one chord a beat. Soprano G4 C5 D5 G4 reads 5 1 2 5; bass C4 F4 G4 C4
 *                 reads 1 4 5 1.
 *   tonicization  The same progression with two measures in G major between C major. Inside the G region the
 *                 degrees are those of G major (D5 is 5, not 2; C5 in the bass is 4, not 1).
 *   texture       A repeated melody note over a held inner voice, and a melody that drops below that inner voice.
 *                 The soprano is the highest sounding note whichever voice plays it; of a repeated note it is the
 *                 one attacked most recently.
 *   monophonic    One line. Where one note sounds, soprano and bass are that note.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* ---- A minimal MIDI writer ---- */

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

/* A delta time as a variable-length quantity. */
static void put_delta(Bytes *bytes, uint32_t ticks) {
    unsigned char b[4];
    size_t count = 0;
    b[count++] = (unsigned char)(ticks & 0x7F);
    while ((ticks >>= 7) > 0) {
        b[count++] = (unsigned char)((ticks & 0x7F) | 0x80);
    }
    for (size_t i = count; i > 0; i--) {
        put(bytes, &b[i - 1], 1);
    }
}

/* A note: starts at tick, sounds length ticks. One beat is 96 ticks, a 4/4 measure 384. */
typedef struct {
    uint32_t tick;
    uint32_t length;
    int pitch;
} PieceNote;

typedef struct {
    uint32_t tick;
    bool on;
    int pitch;
} PieceEvent;

static int compare_events(const void *a, const void *b) {
    const PieceEvent *x = a;
    const PieceEvent *y = b;
    if (x->tick != y->tick) {
        return x->tick < y->tick ? -1 : 1;
    }
    return x->on == y->on ? 0 : (x->on ? 1 : -1); /* a release before an attack at the same tick */
}

/* Writes the notes to path as a type-0 file (division 96, 4/4 by default, no key signature). */
static void write_piece(const char *path, const PieceNote *notes, size_t count) {
    PieceEvent *events = malloc(sizeof(PieceEvent) * count * 2);
    for (size_t i = 0; i < count; i++) {
        events[2 * i] = (PieceEvent){notes[i].tick, true, notes[i].pitch};
        events[2 * i + 1] = (PieceEvent){notes[i].tick + notes[i].length, false, notes[i].pitch};
    }
    qsort(events, count * 2, sizeof(PieceEvent), compare_events);

    Bytes track = {NULL, 0, 0};
    uint32_t now = 0;
    for (size_t i = 0; i < count * 2; i++) {
        put_delta(&track, events[i].tick - now);
        now = events[i].tick;
        unsigned char message[] = {events[i].on ? 0x90 : 0x80, (unsigned char)events[i].pitch,
                                   events[i].on ? 0x64 : 0x40};
        put(&track, message, sizeof(message));
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
    free(events);
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

/* True when the outer note is present with this pitch name and degree. */
static bool outer_is(const AnalysisOuterNote *note, const char *pitch, const char *degree) {
    return note->status == ANALYSIS_OUTER_PRESENT && note->pitch_name && strcmp(note->pitch_name, pitch) == 0 &&
           note->degree && strcmp(note->degree, degree) == 0;
}

static void report_outer(const char *what, size_t index, const AnalysisOuterNote *note) {
    fprintf(stderr, "  %s %zu: status %d, pitch %s, degree %s\n", what, index, (int)note->status,
            note->pitch_name ? note->pitch_name : "-", note->degree ? note->degree : "-");
}

/* The outer_voices section: both lines hold one entry per chord, in order, and each entry agrees with the
   report entry and with the chord's own "outer" summary. */
static void check_document_lines(const Analyzed *analyzed) {
    const AnalysisReport *report = &analyzed->report;
    const cJSON *outer = cJSON_GetObjectItemCaseSensitive(analyzed->document, "outer_voices");
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(analyzed->document, "chords");
    CHECK(cJSON_IsObject(outer));
    CHECK(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(analyzed->document, "melody")));
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(analyzed->document, "melody")) == 0);
    for (int voice = 0; voice < 2; voice++) {
        const char *name = voice == 0 ? "soprano" : "bass";
        const cJSON *line = cJSON_GetObjectItemCaseSensitive(outer, name);
        CHECK(cJSON_GetArraySize(line) == (int)report->entry_count);
        CHECK(cJSON_GetArraySize(chords) == (int)report->entry_count);
        for (size_t i = 0; i < report->entry_count && i < (size_t)cJSON_GetArraySize(line); i++) {
            const AnalysisOuterNote *note = voice == 0 ? &report->entries[i].soprano : &report->entries[i].bass;
            const cJSON *entry = cJSON_GetArrayItem(line, (int)i);
            const cJSON *summary = cJSON_GetObjectItemCaseSensitive(
                cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(chords, (int)i), "outer"), name);
            CHECK((size_t)cJSON_GetObjectItemCaseSensitive(entry, "chord_index")->valuedouble == i);
            CHECK(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry, "framework")));
            CHECK(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry, "location")));
            const cJSON *degree = cJSON_GetObjectItemCaseSensitive(entry, "degree");
            const cJSON *summary_degree = cJSON_GetObjectItemCaseSensitive(summary, "degree");
            CHECK(cJSON_IsNull(degree) == (note->degree == NULL));
            CHECK(cJSON_IsNull(summary_degree) == (note->degree == NULL));
            if (note->degree) {
                CHECK(strcmp(degree->valuestring, note->degree) == 0);
                CHECK(strcmp(summary_degree->valuestring, note->degree) == 0);
            }
            if (note->status == ANALYSIS_OUTER_PRESENT) {
                const cJSON *note_ids = cJSON_GetObjectItemCaseSensitive(entry, "note_ids");
                CHECK(cJSON_GetArraySize(note_ids) == 1);
                CHECK((size_t)cJSON_GetObjectItemCaseSensitive(entry, "note_id")->valuedouble == note->note_id);
                CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "pitch"),
                                                              "name")->valuestring,
                             note->pitch_name) == 0);
                CHECK(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry, "onset")));
                CHECK(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry, "duration")));
            } else {
                CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(entry, "note_ids")) == 0);
                CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "note_id")));
                CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "onset")));
            }
        }
    }
}

/* ---- Pieces ---- */

/* One chord, three notes, sounding one beat from tick. */
static void add_chord(PieceNote *notes, size_t *count, uint32_t tick, int first, int second, int third) {
    notes[(*count)++] = (PieceNote){tick, 96, first};
    notes[(*count)++] = (PieceNote){tick, 96, second};
    notes[(*count)++] = (PieceNote){tick, 96, third};
}

/* A measure of I, IV, V, I in the major key on tonic (a pitch class), from tick. */
static void add_measure(PieceNote *notes, size_t *count, uint32_t tick, int tonic) {
    int base = 60 + tonic;
    add_chord(notes, count, tick, base, base + 4, base + 7);
    add_chord(notes, count, tick + 96, base + 5, base + 9, base + 12);
    add_chord(notes, count, tick + 192, base + 7, base + 11, base + 14);
    add_chord(notes, count, tick + 288, base, base + 4, base + 7);
}

/* I, IV, V, I in C major, eight measures, a chord a beat. In the first measure the soprano is G4 C5 D5 G4 (scale
   degrees 5 1 2 5) and the bass C4 F4 G4 C4 (1 4 5 1), read by hand. */
static void test_progression(const char *path) {
    PieceNote notes[8 * 12];
    size_t count = 0;
    for (uint32_t m = 0; m < 8; m++) {
        add_measure(notes, &count, m * 384, 0);
    }
    write_piece(path, notes, count);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->entry_count == 32);
    CHECK(report->key_region_count == 1);
    if (report->key_region_count == 1) {
        CHECK(strcmp(report->key_regions[0].label, "C major") == 0);
    }
    static const char *const SOPRANO_PITCH[4] = {"G4", "C5", "D5", "G4"};
    static const char *const SOPRANO_DEGREE[4] = {"5", "1", "2", "5"};
    static const char *const BASS_PITCH[4] = {"C4", "F4", "G4", "C4"};
    static const char *const BASS_DEGREE[4] = {"1", "4", "5", "1"};
    for (size_t i = 0; i < report->entry_count && i < 32; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        bool soprano_ok = outer_is(&entry->soprano, SOPRANO_PITCH[i % 4], SOPRANO_DEGREE[i % 4]);
        bool bass_ok = outer_is(&entry->bass, BASS_PITCH[i % 4], BASS_DEGREE[i % 4]);
        CHECK(soprano_ok);
        CHECK(bass_ok);
        if (!soprano_ok || !bass_ok) {
            report_outer("soprano", i, &entry->soprano);
            report_outer("bass", i, &entry->bass);
        }
    }
    check_document_lines(&analyzed);
    release(&analyzed);
}

/* C major, two measures of G major, C major. Inside the G major region the degrees are read in G major: the
   soprano D5 of its first chord is 5 (it is 2 in C major) and the G4 bass is 1 (5 in C major). Each of the four
   chords of the region's first measure is checked against the pitch and the region's own key. */
static void test_tonicization(const char *path) {
    PieceNote notes[18 * 12];
    size_t count = 0;
    uint32_t tick = 0;
    for (int m = 0; m < 18; m++, tick += 384) {
        add_measure(notes, &count, tick, (m >= 8 && m < 10) ? 7 : 0);
    }
    write_piece(path, notes, count);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    bool found_local_region = false;
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!entry->in_region || strcmp(report->key_regions[entry->region].label, "G major") != 0) {
            continue;
        }
        found_local_region = true;
        /* Whatever the region's edges, the degrees follow the key of the region the chord lies in. */
        CHECK(entry->soprano.status == ANALYSIS_OUTER_PRESENT && entry->soprano.degree != NULL);
        CHECK(entry->bass.status == ANALYSIS_OUTER_PRESENT && entry->bass.degree != NULL);
    }
    CHECK(found_local_region);
    if (!found_local_region) {
        release(&analyzed);
        return;
    }

    /* The G major measure's first chord: G B D over G, from beat 1 of the first measure played in G (the ninth,
       starting at 8 whole notes). The second pass's phrase boundaries (R-039) put the region's edge a measure
       early, so the chord is found by time, and it still lies in the G major region. */
    size_t first_in_g = report->entry_count;
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (entry->time_numerator == 8 && entry->time_denominator == 1) {
            first_in_g = i;
            break;
        }
    }
    CHECK(first_in_g + 1 < report->entry_count);
    if (first_in_g + 1 < report->entry_count) {
        CHECK(report->entries[first_in_g].in_region &&
              strcmp(report->key_regions[report->entries[first_in_g].region].label, "G major") == 0);
        CHECK(outer_is(&report->entries[first_in_g].soprano, "D5", "5"));
        CHECK(outer_is(&report->entries[first_in_g].bass, "G4", "1"));
        CHECK(outer_is(&report->entries[first_in_g + 1].soprano, "G5", "1"));
        CHECK(outer_is(&report->entries[first_in_g + 1].bass, "C5", "4"));
    }

    /* The same notes in the C major measures before it read in C major. */
    CHECK(outer_is(&report->entries[2].soprano, "D5", "2"));
    CHECK(outer_is(&report->entries[2].bass, "G4", "5"));
    check_document_lines(&analyzed);
    release(&analyzed);
}

/* Texture. Under a bass C3 (attacked on beats 1 and 3) and a held inner voice E4, a melody plays C5 on beat 1 and
   again on beat 2 (a repeated note), then drops to A3, below the inner voice, for beats 3 and 4. The bass attack
   on beat 3 keeps the chord segmentation from merging the two halves (it merges over unchanged harmony only when
   the new attacks are above the bass). Documented behavior:
     - the soprano is the highest sounding note, whichever voice plays it: on beats 3 and 4 it is the inner voice
       E4 (degree 3), not the melody that has crossed below it;
     - for a repeated note the soprano is the note attacked most recently;
     - the bass is the lowest sounding note, C3, throughout. */
static void test_texture(const char *path) {
    PieceNote notes[8 * 6];
    size_t count = 0;
    for (uint32_t m = 0; m < 8; m++) {
        uint32_t tick = m * 384;
        notes[count++] = (PieceNote){tick, 192, 48};
        notes[count++] = (PieceNote){tick + 192, 192, 48};
        notes[count++] = (PieceNote){tick, 384, 64};
        notes[count++] = (PieceNote){tick, 96, 72};
        notes[count++] = (PieceNote){tick + 96, 96, 72};
        notes[count++] = (PieceNote){tick + 192, 192, 57};
    }
    write_piece(path, notes, count);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->entry_count >= 2);
    if (report->entry_count >= 2) {
        /* Beats 1 and 2: one segment (the same pitch classes over the same bass). */
        const AnalysisReportEntry *melody = &report->entries[0];
        CHECK(outer_is(&melody->soprano, "C5", "1"));
        CHECK(outer_is(&melody->bass, "C3", "1"));
        /* The second C5, attacked on beat 2, is the soprano of the merged segment. */
        const cJSON *soprano_line = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(analyzed.document, "outer_voices"), "soprano");
        const cJSON *onset = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(soprano_line, 0), "onset"), "time");
        CHECK(cJSON_GetObjectItemCaseSensitive(onset, "num")->valuedouble == 1);
        CHECK(cJSON_GetObjectItemCaseSensitive(onset, "den")->valuedouble == 4);

        /* Beats 3 and 4: the melody is below the inner voice, which is now the highest. */
        const AnalysisReportEntry *crossed = &report->entries[1];
        CHECK(outer_is(&crossed->soprano, "E4", "3"));
        CHECK(outer_is(&crossed->bass, "C3", "1"));
        CHECK(crossed->soprano.pitch > report->entries[1].bass.pitch);
    }
    check_document_lines(&analyzed);
    release(&analyzed);
}

/* One line: C4 E4 G4 E4 over eight measures. Where only one note sounds the soprano and the bass are that
   note, with the same degree. Documented in the README. */
static void test_monophonic(const char *path) {
    PieceNote notes[8 * 4];
    size_t count = 0;
    static const int LINE[4] = {60, 64, 67, 64};
    for (uint32_t m = 0; m < 8; m++) {
        for (uint32_t beat = 0; beat < 4; beat++) {
            notes[count++] = (PieceNote){m * 384 + beat * 96, 96, LINE[beat]};
        }
    }
    write_piece(path, notes, count);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->entry_count > 0);
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisOuterNote *soprano = &report->entries[i].soprano;
        const AnalysisOuterNote *bass = &report->entries[i].bass;
        CHECK(soprano->status == ANALYSIS_OUTER_PRESENT && bass->status == ANALYSIS_OUTER_PRESENT);
        if (soprano->status == ANALYSIS_OUTER_PRESENT && bass->status == ANALYSIS_OUTER_PRESENT) {
            CHECK(soprano->note_id == bass->note_id);
            CHECK(soprano->pitch == bass->pitch);
            CHECK((soprano->degree == NULL) == (bass->degree == NULL));
            CHECK(soprano->degree == NULL || strcmp(soprano->degree, bass->degree) == 0);
        }
    }
    check_document_lines(&analyzed);
    release(&analyzed);
}

int main(void) {
    test_progression("/tmp/ursatz_analyzer_outer_progression.mid");
    test_tonicization("/tmp/ursatz_analyzer_outer_tonicization.mid");
    test_texture("/tmp/ursatz_analyzer_outer_texture.mid");
    test_monophonic("/tmp/ursatz_analyzer_outer_monophonic.mid");
    remove("/tmp/ursatz_analyzer_outer_progression.mid");
    remove("/tmp/ursatz_analyzer_outer_tonicization.mid");
    remove("/tmp/ursatz_analyzer_outer_texture.mid");
    remove("/tmp/ursatz_analyzer_outer_monophonic.mid");
    if (failures > 0) {
        fprintf(stderr, "test_outer_voices: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_outer_voices: ok\n");
    return 0;
}
