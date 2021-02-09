#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/report_writer.h>

/*
 * End-to-end test: runs the same real MIDI fixture used by
 * test_analysis_pipeline.c through the full pipeline, renders it to
 * Markdown, writes it to the derived output path, and checks the
 * written file's content for the expected chord/key labels (a content
 * check, not an exact byte-for-byte match against a fixed golden
 * file, so wording changes don't require updating this test).
 */

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

static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);

    char *buffer = malloc((size_t)size + 1);
    assert(buffer != NULL);
    assert(fread(buffer, 1, (size_t)size, file) == (size_t)size);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

int main(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
        0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
        0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
        0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
        0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
        0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
        0x00, 0x90, 0x3E, 0x64, /* delta0  note-on  D4 (62) */
        0x60, 0x80, 0x3E, 0x40, /* delta96 note-off D4 */
        0x00, 0x90, 0x41, 0x64, /* delta0  note-on  F4 (65) */
        0x60, 0x80, 0x41, 0x40, /* delta96 note-off F4 */
        0x00, 0x90, 0x45, 0x64, /* delta0  note-on  A4 (69) */
        0x60, 0x80, 0x45, 0x40, /* delta96 note-off A4 */
        0x00, 0x90, 0x47, 0x64, /* delta0  note-on  B4 (71) */
        0x60, 0x80, 0x47, 0x40, /* delta96 note-off B4 */
        0x00, 0xFF, 0x2F, 0x00  /* end of track */
    };

    unsigned char buf[256];
    size_t size = build_smf_type0(buf, 96, track, sizeof(track));

    const char *input_path = "/tmp/ursatz_analyzer_report_test_fixture.mid";
    FILE *file = fopen(input_path, "wb");
    assert(file != NULL);
    assert(fwrite(buf, 1, size, file) == size);
    fclose(file);

    ImportedNotes notes;
    assert(midi_pipeline_import_file(input_path, &notes) == MIDI_PIPELINE_OK);

    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));

    AnalysisDocument *document = analysis_document_build(input_path, &notes, &report);
    assert(document != NULL);
    char *markdown = markdown_report_render(document);
    assert(markdown != NULL);
    analysis_document_destroy(document);

    char *output_path = derive_report_path(input_path);
    assert(output_path != NULL);
    assert(strcmp(output_path, "/tmp/ursatz_analyzer_report_test_fixture.report.md") == 0);

    assert(write_report_file(output_path, markdown));

    char *written = read_whole_file(output_path);
    assert(strstr(written, "C major") != NULL);
    assert(strstr(written, "I") != NULL);
    assert(strstr(written, "vii") != NULL);

    free(written);
    free(output_path);
    free(markdown);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    remove(input_path);
    remove("/tmp/ursatz_analyzer_report_test_fixture.report.md");

    printf("test_report_output: ok\n");
    return 0;
}
