#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * MusicXML input: format choice, the notated metadata, a pickup, multi-part
 * scores, notated pitch spelling, and every error case. The scores are the
 * small hand-written fixtures in tests/musicxml.
 */

static char fixture_path[512];

static const char *musicxml(const char *name) {
    snprintf(fixture_path, sizeof(fixture_path), "%s/%s", URSATZ_MUSICXML_DIR, name);
    return fixture_path;
}

static const char *string(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static long number(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsNumber(item));
    return (long)item->valuedouble;
}

/* Analyzes path and returns the parsed analysis document; *notes_out, if given, receives the imported notes. */
static cJSON *analyze(const char *path, ImportedNotes *notes_out) {
    ImportedNotes notes;
    assert(input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    char *json = analysis_document_render(path, &notes, &report);
    assert(json != NULL);
    analysis_report_destroy(&report);
    if (notes_out) {
        *notes_out = notes;
    } else {
        imported_notes_destroy(&notes);
    }
    cJSON *document = cJSON_Parse(json);
    assert(document != NULL);
    free(json);
    return document;
}

static void write_bytes(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(bytes, 1, size, file) == size);
    fclose(file);
}

static void copy_file(const char *from, const char *to) {
    FILE *file = fopen(from, "rb");
    assert(file != NULL);
    char buffer[8192];
    size_t size = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);
    assert(size > 0 && size < sizeof(buffer));
    write_bytes(to, buffer, size);
}

static void test_format_names(void) {
    InputFormat format = INPUT_FORMAT_AUTO;
    assert(input_format_parse("midi", &format) && format == INPUT_FORMAT_MIDI);
    assert(input_format_parse("musicxml", &format) && format == INPUT_FORMAT_MUSICXML);
    assert(!input_format_parse("mxl", &format) && format == INPUT_FORMAT_MUSICXML);
    assert(!input_format_parse("", &format));
    assert(strcmp(input_format_name(INPUT_FORMAT_MIDI), "midi") == 0);
    assert(strcmp(input_format_name(INPUT_FORMAT_MUSICXML), "musicxml") == 0);
}

static void test_format_detection(void) {
    /* By extension, whatever the content, and whatever the case. */
    assert(input_format_detect("/nowhere/piece.mid") == INPUT_FORMAT_MIDI);
    assert(input_format_detect("/nowhere/piece.MIDI") == INPUT_FORMAT_MIDI);
    assert(input_format_detect("/nowhere/piece.musicxml") == INPUT_FORMAT_MUSICXML);
    assert(input_format_detect("/nowhere/piece.xml") == INPUT_FORMAT_MUSICXML);
    assert(input_format_detect("/nowhere/piece.mxl") == INPUT_FORMAT_MUSICXML);
    assert(input_format_detect("/nowhere/piece") == INPUT_FORMAT_AUTO);
    assert(input_format_detect("/nowhere/dir.mid/piece") == INPUT_FORMAT_AUTO);

    /* By content when the extension says nothing. */
    const char *path = "/tmp/ursatz_analyzer_sniff.dat";
    write_bytes(path, "MThd\0\0\0\6", 8);
    assert(input_format_detect(path) == INPUT_FORMAT_MIDI);
    write_bytes(path, "\xEF\xBB\xBF  \n<?xml version=\"1.0\"?>", 29);
    assert(input_format_detect(path) == INPUT_FORMAT_MUSICXML);
    write_bytes(path, "PK\x03\x04zip", 7);
    assert(input_format_detect(path) == INPUT_FORMAT_MUSICXML);
    write_bytes(path, "plain text", 10);
    assert(input_format_detect(path) == INPUT_FORMAT_AUTO);
    remove(path);
}

static void test_notated_metadata_and_pickup(void) {
    ImportedNotes notes;
    cJSON *document = analyze(musicxml("pickup.musicxml"), &notes);

    assert(notes.source.format == INPUT_FORMAT_MUSICXML);
    assert(notes.source.meter_notated && notes.source.key_notated && notes.source.spelling_notated);
    assert(notes.key_signature.found && notes.key_signature.sharps_flats == 0 && !notes.key_signature.is_minor);
    assert(notes.key_signature_events.count == 1);
    imported_notes_destroy(&notes);

    const cJSON *input = test_json_require(test_json_require(document, "metadata"), "input");
    assert(number(document, "schema_version") == 9);
    assert(strcmp(string(input, "format"), "musicxml") == 0);
    assert(strcmp(string(input, "source_type"), "score") == 0);
    assert(strcmp(string(input, "meter"), "notated") == 0);
    assert(strcmp(string(input, "key"), "notated") == 0);
    assert(strcmp(string(input, "spelling"), "notated") == 0);

    const cJSON *meter = test_json_require(document, "meter");
    assert(strcmp(string(meter, "source"), "notated") == 0);
    assert(number(test_json_require(meter, "time_signature"), "numerator") == 4);
    assert(number(test_json_require(meter, "time_signature"), "denominator") == 4);
    assert(number(meter, "measure_count") == 5); /* the pickup counts as measure 0 */
    assert(cJSON_IsObject(test_json_require(meter, "pickup")));
    assert(number(meter, "last_measure") == 4);

    const cJSON *regions = test_json_require(test_json_require(document, "keys"), "regions");
    assert(cJSON_GetArraySize(regions) >= 1);
    assert(strcmp(string(cJSON_GetArrayItem(regions, 0), "tonic"), "C") == 0);
    assert(strcmp(string(cJSON_GetArrayItem(regions, 0), "mode"), "major") == 0);
    cJSON_Delete(document);
}

static void test_multi_part_score(void) {
    ImportedNotes notes;
    cJSON *document = analyze(musicxml("two-parts.musicxml"), &notes);

    /* Every part is analyzed, merged into one texture, and its label stays on its notes. */
    assert(notes.count == 12);
    assert(notes.source.part_count == 3);
    size_t per_part[3] = {0, 0, 0};
    for (size_t i = 0; i < notes.count; i++) {
        assert(notes.source.part_index[i] < 3);
        per_part[notes.source.part_index[i]]++;
        assert(notes.source.staff[i] == 1);
    }
    assert(per_part[0] == 4 && per_part[1] == 4 && per_part[2] == 4);
    assert(strcmp(notes.source.parts[0].id, "P1") == 0 && strcmp(notes.source.parts[0].name, "Soprano") == 0);
    assert(strcmp(notes.source.parts[2].id, "P3") == 0 && strcmp(notes.source.parts[2].name, "Bass") == 0);
    imported_notes_destroy(&notes);

    const cJSON *metadata = test_json_require(document, "metadata");
    assert(number(metadata, "note_count") == 12);
    const cJSON *parts = test_json_require(test_json_require(metadata, "input"), "parts");
    assert(cJSON_GetArraySize(parts) == 3);
    long total = 0;
    for (int i = 0; i < 3; i++) {
        total += number(cJSON_GetArrayItem(parts, i), "note_count");
    }
    assert(total == 12);
    assert(strcmp(string(cJSON_GetArrayItem(parts, 1), "name"), "Alto") == 0);

    /* The merged texture has the outer voices of all the parts: the first part's C5 over the third part's C3. */
    const cJSON *first = cJSON_GetArrayItem(test_json_require(document, "chords"), 0);
    const cJSON *outer = test_json_require(first, "outer");
    assert(strcmp(string(test_json_require(outer, "soprano"), "pitch"), "C5") == 0);
    assert(strcmp(string(test_json_require(outer, "bass"), "pitch"), "C3") == 0);
    cJSON_Delete(document);
}

/* The same sounding pitches, written in D-flat major and in C-sharp major, keep the spelling they were written in. */
static void test_enharmonic_spelling(void) {
    cJSON *flat = analyze(musicxml("spelling-flat.musicxml"), NULL);
    cJSON *sharp = analyze(musicxml("spelling-sharp.musicxml"), NULL);

    const cJSON *flat_first = cJSON_GetArrayItem(test_json_require(flat, "chords"), 0);
    const cJSON *sharp_first = cJSON_GetArrayItem(test_json_require(sharp, "chords"), 0);
    assert(strcmp(string(flat_first, "root"), "Db") == 0);
    assert(strcmp(string(flat_first, "bass"), "Db") == 0);
    assert(strcmp(string(test_json_require(test_json_require(flat_first, "outer"), "bass"), "pitch"), "Db3") == 0);
    assert(strcmp(string(sharp_first, "root"), "C#") == 0);
    assert(strcmp(string(sharp_first, "bass"), "C#") == 0);
    assert(strcmp(string(test_json_require(test_json_require(sharp_first, "outer"), "bass"), "pitch"), "C#3") == 0);

    /* Chord quality, inversion and degrees do not depend on the spelling. */
    assert(strcmp(string(flat_first, "quality"), string(sharp_first, "quality")) == 0);
    assert(number(flat_first, "inversion") == number(sharp_first, "inversion"));

    /* The fifth chord's upper note, B#4 / C5, is spelled as written. */
    const cJSON *sharp_third = cJSON_GetArrayItem(test_json_require(sharp, "chords"), 2);
    assert(strcmp(string(sharp_third, "root"), "G#") == 0);
    cJSON_Delete(flat);
    cJSON_Delete(sharp);
}

static void test_midi_keeps_rule_spelling(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Bach 114.mid", URSATZ_FIXTURE_DIR);
    ImportedNotes notes;
    cJSON *document = analyze(path, &notes);
    assert(notes.source.format == INPUT_FORMAT_MIDI);
    assert(!notes.source.spelling_notated && !notes.source.meter_notated && !notes.source.key_notated);
    /* The file's one named track is one part on two staves, its notes all in it. */
    assert(notes.source.part_count == 1);
    assert(strcmp(notes.source.parts[0].name, "Grand Piano") == 0);
    assert(notes.source.parts[0].staff_count == 2);
    for (size_t i = 0; i < notes.count; i++) {
        assert(notes.streams[i] == 0 && notes.source.part_index[i] == 0);
    }
    imported_notes_destroy(&notes);
    const cJSON *input = test_json_require(test_json_require(document, "metadata"), "input");
    assert(strcmp(string(input, "format"), "midi") == 0);
    assert(strcmp(string(input, "source_type"), "performance") == 0);
    assert(strcmp(string(input, "spelling"), "rule") == 0);
    assert(cJSON_GetArraySize(test_json_require(input, "parts")) == 1);
    cJSON_Delete(document);
}

static void expect_failure(const char *path, InputFormat format, InputPipelineResult expected) {
    ImportedNotes notes;
    memset(&notes, 0xA5, sizeof(notes));
    assert(input_pipeline_import_file(path, format, &notes) == expected);
    /* An error leaves the output untouched: nothing to destroy. */
    unsigned char *bytes = (unsigned char *)&notes;
    for (size_t i = 0; i < sizeof(notes); i++) {
        assert(bytes[i] == 0xA5);
    }
}

static void test_errors(void) {
    expect_failure(musicxml("malformed.musicxml"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    expect_failure(musicxml("not-music.xml"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    expect_failure(musicxml("timewise.musicxml"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    expect_failure(musicxml("empty-score.musicxml"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    expect_failure(musicxml("pickup.mxl"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    expect_failure(musicxml("README.md"), INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_UNKNOWN_FORMAT);
    expect_failure("/nowhere/missing.musicxml", INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_CANNOT_READ_FILE);
    expect_failure("/nowhere/missing.mid", INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_CANNOT_READ_FILE);

    /* A format forced against the content fails cleanly. */
    expect_failure(musicxml("pickup.musicxml"), INPUT_FORMAT_MIDI, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    char midi[512];
    snprintf(midi, sizeof(midi), "%s/framework-v1-corpus/Bach 114.mid", URSATZ_FIXTURE_DIR);
    expect_failure(midi, INPUT_FORMAT_MUSICXML, INPUT_PIPELINE_ERR_IMPORT_FAILED);
}

static void test_override_and_sniff(void) {
    /* A score named .mid is read as MusicXML only when the format is forced. */
    const char *path = "/tmp/ursatz_analyzer_forced.mid";
    copy_file(musicxml("pickup.musicxml"), path);
    expect_failure(path, INPUT_FORMAT_AUTO, INPUT_PIPELINE_ERR_IMPORT_FAILED);
    ImportedNotes notes;
    assert(input_pipeline_import_file(path, INPUT_FORMAT_MUSICXML, &notes) == INPUT_PIPELINE_OK);
    assert(notes.source.format == INPUT_FORMAT_MUSICXML);
    imported_notes_destroy(&notes);
    remove(path);

    /* With no extension to go on, the content decides. */
    path = "/tmp/ursatz_analyzer_sniffed";
    copy_file(musicxml("pickup.musicxml"), path);
    assert(input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_OK);
    assert(notes.source.format == INPUT_FORMAT_MUSICXML);
    imported_notes_destroy(&notes);
    remove(path);
}

int main(void) {
    test_format_names();
    test_format_detection();
    test_notated_metadata_and_pickup();
    test_multi_part_score();
    test_enharmonic_spelling();
    test_midi_keeps_rule_spelling();
    test_errors();
    test_override_and_sniff();
    return 0;
}
