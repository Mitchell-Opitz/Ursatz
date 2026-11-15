#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * Targeted chord tests on small constructed files (division 96, 4/4, C major
 * from a key signature event, so the key is not in question): the chord's
 * inversion, the runner-up present and absent, and four non-chord-tone types
 * the library names -- passing, neighbor, escape-tone and suspension -- plus
 * one tone it leaves unclassified.
 *
 * Notes are (start tick, length in ticks, MIDI note number); one beat is 96.
 * The expected figures follow Ursatz's weighted identification and
 * non-chord-tone rules (common-practice-minimal): a tone is read against its
 * neighbors in the adjacent segments.
 */

#define BEAT 96

typedef struct {
    int start;
    int length;
    int pitch;
} Note;

static void append_u32(unsigned char *buf, size_t *pos, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                           (unsigned char)v};
    memcpy(buf + *pos, b, 4);
    *pos += 4;
}

/* Variable-length quantity; the fixtures stay under 2^14 ticks. */
static size_t append_delta(unsigned char *track, size_t pos, int delta) {
    if (delta >= 128) {
        track[pos++] = (unsigned char)(0x80 | (delta >> 7));
    }
    track[pos++] = (unsigned char)(delta & 0x7F);
    return pos;
}

/* Writes an SMF type 0 file of the notes, with a C major key signature. */
static void write_fixture(const char *path, const Note *notes, size_t count) {
    typedef struct {
        int tick;
        int on;
        int pitch;
    } Event;
    Event events[64];
    size_t event_count = 0;
    for (size_t i = 0; i < count; i++) {
        events[event_count++] = (Event){notes[i].start, 1, notes[i].pitch};
        events[event_count++] = (Event){notes[i].start + notes[i].length, 0, notes[i].pitch};
    }
    for (size_t i = 1; i < event_count; i++) { /* by tick, offs before ons */
        Event key = events[i];
        size_t j = i;
        while (j > 0 && (events[j - 1].tick > key.tick || (events[j - 1].tick == key.tick && events[j - 1].on > key.on))) {
            events[j] = events[j - 1];
            j--;
        }
        events[j] = key;
    }

    unsigned char track[1024];
    size_t pos = 0;
    const unsigned char key_signature[] = {0x00, 0xFF, 0x59, 0x02, 0x00, 0x00};
    memcpy(track, key_signature, sizeof(key_signature));
    pos = sizeof(key_signature);
    int last = 0;
    for (size_t i = 0; i < event_count; i++) {
        pos = append_delta(track, pos, events[i].tick - last);
        last = events[i].tick;
        track[pos++] = events[i].on ? 0x90 : 0x80;
        track[pos++] = (unsigned char)events[i].pitch;
        track[pos++] = events[i].on ? 0x64 : 0x40;
    }
    const unsigned char end[] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(track + pos, end, sizeof(end));
    pos += sizeof(end);

    unsigned char file[1100];
    size_t size = 0;
    memcpy(file + size, "MThd", 4);
    size += 4;
    append_u32(file, &size, 6);
    file[size++] = 0;
    file[size++] = 0;
    file[size++] = 0;
    file[size++] = 1;
    file[size++] = 0;
    file[size++] = BEAT;
    memcpy(file + size, "MTrk", 4);
    size += 4;
    append_u32(file, &size, (uint32_t)pos);
    memcpy(file + size, track, pos);
    size += pos;

    FILE *out = fopen(path, "wb");
    assert(out != NULL);
    assert(fwrite(file, 1, size, out) == size);
    fclose(out);
}

typedef struct {
    cJSON *document;
    char *markdown;
} Analyzed;

static Analyzed analyze(const char *name, const Note *notes, size_t count) {
    char path[512];
    char file_name[128];
    snprintf(file_name, sizeof(file_name), "chord_details_%s.mid", name);
    test_temp_path(path, sizeof(path), file_name);
    write_fixture(path, notes, count);

    ImportedNotes imported;
    assert(midi_pipeline_import_file(path, &imported) == MIDI_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&imported, &report));
    AnalysisDocument *built = analysis_document_build(path, &imported, &report);
    assert(built != NULL);
    char *json = analysis_document_to_json(built);
    assert(json != NULL);
    Analyzed analyzed = {cJSON_Parse(json), markdown_report_render(built)};
    assert(analyzed.document != NULL && analyzed.markdown != NULL);
    free(json);
    analysis_document_destroy(built);
    analysis_report_destroy(&report);
    imported_notes_destroy(&imported);
    remove(path);
    return analyzed;
}

static void analyzed_destroy(Analyzed *analyzed) {
    cJSON_Delete(analyzed->document);
    free(analyzed->markdown);
}

static const char *string(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static int number(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsNumber(item));
    return (int)item->valuedouble;
}

static const cJSON *chord_at(const Analyzed *analyzed, int index) {
    const cJSON *chords = test_json_require(analyzed->document, "chords");
    assert(index < cJSON_GetArraySize(chords));
    return cJSON_GetArrayItem(chords, index);
}

/* ---- Inversion ---- */

static void test_inversion(void) {
    /* C-E-G with the E lowest, held for four beats: one C major chord in first inversion. */
    const Note first[] = {{0, 4 * BEAT, 52}, {0, 4 * BEAT, 55}, {0, 4 * BEAT, 60}};
    Analyzed analyzed = analyze("first_inversion", first, 3);
    assert(cJSON_GetArraySize(test_json_require(analyzed.document, "chords")) == 1);
    const cJSON *chord = chord_at(&analyzed, 0);
    assert(strcmp(string(chord, "label"), "I") == 0);
    assert(strcmp(string(chord, "root"), "C") == 0);
    assert(strcmp(string(chord, "quality"), "major") == 0);
    assert(strcmp(string(chord, "bass"), "E") == 0);
    assert(number(chord, "inversion") == 1);
    assert(number(test_json_require(test_json_require(chord, "duration"), "beats"), "num") == 4);
    assert(strstr(analyzed.markdown, "| I | I6 (in C major) | tonic | - | C | major | E | first | 4 |") != NULL);
    analyzed_destroy(&analyzed);

    /* The same chord with the G lowest is in second inversion; with the C lowest, root position. */
    const Note second[] = {{0, 4 * BEAT, 43}, {0, 4 * BEAT, 52}, {0, 4 * BEAT, 60}};
    analyzed = analyze("second_inversion", second, 3);
    assert(number(chord_at(&analyzed, 0), "inversion") == 2);
    assert(strcmp(string(chord_at(&analyzed, 0), "bass"), "G") == 0);
    analyzed_destroy(&analyzed);

    const Note root[] = {{0, 4 * BEAT, 48}, {0, 4 * BEAT, 52}, {0, 4 * BEAT, 55}};
    analyzed = analyze("root_position", root, 3);
    assert(number(chord_at(&analyzed, 0), "inversion") == 0);
    assert(strstr(analyzed.markdown, "root position") != NULL);
    analyzed_destroy(&analyzed);
}

/* ---- Runner-up ---- */

static void test_runner_up(void) {
    /* A lone C: no other root sounds, so the library has no runner-up. */
    const Note lone[] = {{0, 4 * BEAT, 48}};
    Analyzed analyzed = analyze("lone_note", lone, 1);
    const cJSON *chord = chord_at(&analyzed, 0);
    assert(strcmp(string(chord, "root"), "C") == 0);
    assert(cJSON_IsNull(test_json_require(chord, "runner_up")));
    analyzed_destroy(&analyzed);

    /* C-E-G-A is a C major sixth and an A minor seventh: two roots, so the loser is the runner-up. */
    const Note ambiguous[] = {{0, 4 * BEAT, 48}, {0, 4 * BEAT, 52}, {0, 4 * BEAT, 55}, {0, 4 * BEAT, 57}};
    analyzed = analyze("ambiguous", ambiguous, 4);
    chord = chord_at(&analyzed, 0);
    const cJSON *runner_up = test_json_require(chord, "runner_up");
    assert(cJSON_IsObject(runner_up));
    assert(strcmp(string(runner_up, "root"), string(chord, "root")) != 0);
    assert(cJSON_IsNumber(test_json_require(runner_up, "score")));
    assert(test_json_require(runner_up, "score")->valuedouble <= test_json_require(chord, "score")->valuedouble + 1e-9);
    /* The report shows the runner-up only when its score is within 0.10 of the best chord's. */
    double margin = test_json_require(chord, "score")->valuedouble - test_json_require(runner_up, "score")->valuedouble;
    assert((strstr(analyzed.markdown, ", score ") != NULL) == (margin < 0.10));
    analyzed_destroy(&analyzed);
}

/* ---- Non-chord tones ---- */

/* A C and a G held for four beats under a melody note on each beat. */
static size_t held_c_major(Note *notes) {
    notes[0] = (Note){0, 4 * BEAT, 36};
    notes[1] = (Note){0, 4 * BEAT, 55};
    return 2;
}

/* The one non-chord tone of the fixture, checked against the library's name for it. */
static void check_single_tone(const Analyzed *analyzed, const char *type, const char *chord_label) {
    const cJSON *tones = test_json_require(analyzed->document, "non_chord_tones");
    assert(cJSON_GetArraySize(tones) == 1);
    const cJSON *tone = cJSON_GetArrayItem(tones, 0);
    assert(strcmp(string(tone, "type"), type) == 0);
    assert(strcmp(string(tone, "label"), type) == 0);
    assert(cJSON_IsTrue(test_json_require(tone, "classified")));
    assert(strcmp(string(tone, "chord_label"), chord_label) == 0);
    assert(cJSON_GetArraySize(test_json_require(tone, "note_ids")) == 1);
    const cJSON *chord = chord_at(analyzed, number(tone, "chord_index"));
    assert(strcmp(string(chord, "label"), chord_label) == 0);
    assert(number(test_json_require(test_json_require(tone, "location"), "start"), "beat") == number(test_json_require(test_json_require(chord, "location"), "start"), "beat"));
    char row[64];
    snprintf(row, sizeof(row), " | %s | %s |\n", type, chord_label);
    assert(strstr(analyzed->markdown, row) != NULL);
}

static void test_non_chord_tone_types(void) {
    Note notes[16];

    /* Passing: E5 D5 C5 -- a D stepped into and out of in one direction. */
    size_t count = held_c_major(notes);
    notes[count++] = (Note){0, BEAT, 76};
    notes[count++] = (Note){BEAT, BEAT, 74};
    notes[count++] = (Note){2 * BEAT, 2 * BEAT, 72};
    Analyzed analyzed = analyze("passing", notes, count);
    check_single_tone(&analyzed, "passing", "I");
    analyzed_destroy(&analyzed);

    /* Neighbor: E5 F5 E5 -- an F stepped away from and back to the same note. */
    count = held_c_major(notes);
    notes[count++] = (Note){0, BEAT, 76};
    notes[count++] = (Note){BEAT, BEAT, 77};
    notes[count++] = (Note){2 * BEAT, 2 * BEAT, 76};
    analyzed = analyze("neighbor", notes, count);
    check_single_tone(&analyzed, "neighbor", "I");
    analyzed_destroy(&analyzed);

    /* Escape tone: C5 D5 G4 -- a D stepped into and left by a leap the other way. */
    count = held_c_major(notes);
    notes[count++] = (Note){0, BEAT, 72};
    notes[count++] = (Note){BEAT, BEAT, 74};
    notes[count++] = (Note){2 * BEAT, 2 * BEAT, 67};
    analyzed = analyze("escape_tone", notes, count);
    check_single_tone(&analyzed, "escape-tone", "I");
    analyzed_destroy(&analyzed);

    /* Suspension: a C5 held from the C chord into the G chord, then stepped down to B4. The change of chord is on a
       barline: this tests the suspension, not harmonic rhythm (a half-bar change inside one bar reads one chord). */
    const Note suspension[] = {{0, 4 * BEAT, 36},          {0, 4 * BEAT, 55},          {4 * BEAT, 4 * BEAT, 43},
                               {4 * BEAT, 4 * BEAT, 50},   {4 * BEAT, 4 * BEAT, 59},   {0, 5 * BEAT, 72},
                               {5 * BEAT, BEAT, 71}};
    analyzed = analyze("suspension", suspension, 7);
    check_single_tone(&analyzed, "suspension", "V");
    analyzed_destroy(&analyzed);
}

static void test_unclassified_tone_is_listed(void) {
    /* An E5 held over the bar into a G chord, with a D5 that comes in early: the D matches no rule. */
    const Note notes[] = {{0, 2 * BEAT, 36},        {0, 2 * BEAT, 55},        {0, BEAT + BEAT / 2, 76},
                          {2 * BEAT, 2 * BEAT, 43}, {2 * BEAT, 2 * BEAT, 50}, {2 * BEAT, 2 * BEAT, 59},
                          {BEAT + BEAT / 2, BEAT / 2, 74}, {2 * BEAT, 2 * BEAT, 74}};
    Analyzed analyzed = analyze("unclassified", notes, 8);
    const cJSON *tones = test_json_require(analyzed.document, "non_chord_tones");
    assert(cJSON_GetArraySize(tones) == 1);
    const cJSON *tone = cJSON_GetArrayItem(tones, 0);
    assert(cJSON_IsNull(test_json_require(tone, "type")));
    assert(cJSON_IsFalse(test_json_require(tone, "classified")));
    assert(strcmp(string(tone, "label"), "unclassified") == 0);
    assert(strstr(analyzed.markdown, " | unclassified | I |\n") != NULL);
    assert(strstr(analyzed.markdown, "1 the library left unclassified") != NULL);
    analyzed_destroy(&analyzed);
}

int main(void) {
    test_inversion();
    test_runner_up();
    test_non_chord_tone_types();
    test_unclassified_tone_is_listed();
    printf("test_chord_details: ok\n");
    return 0;
}
