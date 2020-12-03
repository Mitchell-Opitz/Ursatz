#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * End-to-end test: a hand-constructed SMF type 0 fixture (same
 * construction style as Ursatz's own tests/io/test_midi_importer.c)
 * runs through the full pipeline -- import, segmentation, key
 * estimation, chord identification -- and the collected report is
 * checked against the expected chord/key labels.
 *
 * Fixture: a C-E-G chord (tick 0), then D, F, A, B played one at a
 * time (ticks 96, 192, 288, 384). The full note collection spans
 * exactly the seven C-major pitch classes, so the global key resolves
 * to "C major". Ursatz's adaptive segmentation cuts one segment per
 * beat, and weighted chord identification reads each as the chord built
 * on its sounding root: C-E-G is "I", the single melody notes D, F, A, B
 * are "ii", "IV", "vi" and "vii\xC2\xB0", and the closing C-E-G-D group
 * (tick 480) is "I" again, the extra D being a non-chord tone.
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
        0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
        0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
        0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
        0x00, 0x90, 0x4A, 0x64, /* delta0  note-on  D5 (74) */
        0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
        0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
        0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
        0x00, 0x80, 0x4A, 0x40, /* delta0  note-off D5 */
        0x00, 0xFF, 0x2F, 0x00  /* end of track */
    };

    unsigned char buf[256];
    size_t size = build_smf_type0(buf, 96, track, sizeof(track));

    const char *path = "/tmp/ursatz_analyzer_test_fixture.mid";
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(buf, 1, size, file) == size);
    fclose(file);

    ImportedNotes notes;
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    assert(notes.count == 11);

    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    assert(report.key_found);
    assert(report.entry_count == 6);

    assert(report.entries[0].time_numerator == 0);
    assert(strcmp(report.entries[0].chord_label, "I") == 0);
    assert(strcmp(report.entries[0].key_label, "C major") == 0);

    const char *const melody_labels[4] = {"ii", "IV", "vi", "vii\xC2\xB0"};
    for (size_t i = 1; i < 5; i++) {
        assert(strcmp(report.entries[i].chord_label, melody_labels[i - 1]) == 0);
        assert(strcmp(report.entries[i].key_label, "C major") == 0);
    }

    /* The extra diatonic D is weighed against the C-E-G, not matched whole. */
    assert(report.entries[5].time_numerator == 5);
    assert(report.entries[5].time_denominator == 4);
    assert(strcmp(report.entries[5].chord_label, "I") == 0);
    assert(strcmp(report.entries[5].key_label, "C major") == 0);

    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    remove(path);

    printf("test_analysis_pipeline: ok\n");
    return 0;
}
