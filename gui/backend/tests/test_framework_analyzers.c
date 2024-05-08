/* Confirms ursatz_gui_analysis_import_and_save invokes and persists
 * every Framework-stage analyzer (Cadence Classification, Phrase
 * Boundary Detection, Motif Detection, Period Detection, Sentence
 * Detection, Thematic Return Detection) against a real MIDI fixture,
 * not just Key Estimation/Chord Identification, and that
 * ursatz_gui_analysis_get_piece can read every type back.
 *
 * The fixture is a hand-built SMF type 0 file (same construction as
 * ursatz's own tests/io/test_midi_importer.c), deliberately shaped so
 * every analyzer this branch wires gets real, non-empty output:
 *
 *   - 10 onset groups in C major: 7 diatonic root-position triads
 *     (alternating I/V, root note always first so it becomes both the
 *     group's harmonic root AND the phrase/motif window's representative
 *     event) plus 3 single non-diatonic "bridge" notes (C#) used only to
 *     suppress a forced phrase boundary at that point (Phrase Boundary
 *     Detection's cadence signal only fires when BOTH the previous and
 *     current group's root are diatonic).
 *   - This produces exactly 3 real cadences (closing groups 3, 6, 9),
 *     which is exactly 3 PhraseOccurrences: A = groups[0..3] (closes on
 *     a half cadence), B = groups[4..6], C = groups[7..9] (both close on
 *     an authentic-unclassified cadence -- this branch never supplies
 *     soprano-resolution data, so a plain PAC/IAC label, and therefore a
 *     structural Form Structure Detection boundary, is never reached;
 *     see run_form_and_thematic_return_detection's own comment).
 *   - Every note has the same duration, so RhythmicPattern equality
 *     always holds; B and C share the same representative-pitch
 *     Contour ([up, up]), so they match each other under
 *     MotifWindowMatching/ThematicReturnDetection. C's Contour is
 *     exactly A's first two directions ([up, up] of A's [up, up, up]),
 *     with fewer events than A, so C qualifies as A's Sentence
 *     continuation.
 *   - Period Detection: (A, B) is a valid pair (half cadence is
 *     strictly weaker than authentic-unclassified); (B, C) is not
 *     (equal strength).
 *
 * All of this rides on Key Estimation picking C major -- the first
 * tonic (0) it tries, over data that is overwhelmingly diatonic to C
 * major, so this is expected to be stable, not flaky.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_service.h"

static int failures = 0;

static void check(bool cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    } else {
        printf("PASS: %s\n", what);
    }
}

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

#define TICK_LEN 48u

/* Appends one onset group (note_count simultaneous notes, root first)
 * starting TICK_LEN ticks after the previous group ended (or at tick 0
 * for the very first group), held for TICK_LEN ticks, all notes ending
 * together. */
static void append_group(unsigned char *track, size_t *pos, const unsigned char *notes, size_t note_count,
                          bool first_group) {
    for (size_t i = 0; i < note_count; i++) {
        unsigned char delta = (i == 0) ? (first_group ? 0 : TICK_LEN) : 0;
        unsigned char event[4] = {delta, 0x90, notes[i], 0x64};
        append_bytes(track, pos, event, 4);
    }
    for (size_t i = 0; i < note_count; i++) {
        unsigned char delta = (i == 0) ? TICK_LEN : 0;
        unsigned char event[4] = {delta, 0x80, notes[i], 0x40};
        append_bytes(track, pos, event, 4);
    }
}

static unsigned char *build_fixture(size_t *out_size) {
    /* group index: 0 1  2 3  4 5  6  7 8  9 */
    unsigned char c4_triad[] = {60, 64, 67}; /* C major, root C4 */
    unsigned char c5_triad[] = {72, 76, 79}; /* C major, root C5 */
    unsigned char g4_triad[] = {67, 71, 74}; /* G major, root G4 */
    unsigned char g5_triad[] = {79, 83, 86}; /* G major, root G5 */
    unsigned char bridge[] = {61};           /* C#, non-diatonic in C major */

    unsigned char track[512];
    size_t pos = 0;

    append_group(track, &pos, c4_triad, 3, true);  /* group 0: C  (deg 1) */
    append_group(track, &pos, bridge, 1, false);   /* group 1: bridge     */
    append_group(track, &pos, c5_triad, 3, false); /* group 2: C  (deg 1) */
    append_group(track, &pos, g5_triad, 3, false); /* group 3: G  (deg 5) -- closes phrase A: half cadence */
    append_group(track, &pos, bridge, 1, false);   /* group 4: bridge     */
    append_group(track, &pos, g4_triad, 3, false); /* group 5: G  (deg 5) */
    append_group(track, &pos, c5_triad, 3, false); /* group 6: C  (deg 1) -- closes phrase B: authentic (unclassified) */
    append_group(track, &pos, bridge, 1, false);   /* group 7: bridge     */
    append_group(track, &pos, g4_triad, 3, false); /* group 8: G  (deg 5) */
    append_group(track, &pos, c5_triad, 3, false); /* group 9: C  (deg 1) -- closes phrase C: authentic (unclassified) */

    unsigned char end_of_track[3] = {0xFF, 0x2F, 0x00};
    unsigned char delta0 = 0x00;
    append_bytes(track, &pos, &delta0, 1);
    append_bytes(track, &pos, end_of_track, 3);

    unsigned char *buf = malloc(pos + 64);
    size_t buf_pos = 0;
    append_bytes(buf, &buf_pos, "MThd", 4);
    append_u32(buf, &buf_pos, 6);
    append_u16(buf, &buf_pos, 0);
    append_u16(buf, &buf_pos, 1);
    append_u16(buf, &buf_pos, 96);
    append_bytes(buf, &buf_pos, "MTrk", 4);
    append_u32(buf, &buf_pos, (uint32_t)pos);
    append_bytes(buf, &buf_pos, track, pos);

    *out_size = buf_pos;
    return buf;
}

static size_t count_type(const UrsatzGuiPieceDetail *detail, const char *type) {
    size_t count = 0;
    for (size_t i = 0; i < detail->interpretation_count; i++) {
        if (strcmp(detail->interpretations[i].type, type) == 0) {
            count++;
        }
    }
    return count;
}

static size_t count_predicate(const UrsatzGuiPieceDetail *detail, const char *predicate) {
    size_t count = 0;
    for (size_t i = 0; i < detail->relationship_count; i++) {
        if (strcmp(detail->relationships[i].predicate_name, predicate) == 0) {
            count++;
        }
    }
    return count;
}

int main(void) {
    const char *pieces_dir = "test_framework_analyzers_pieces";

    size_t midi_size = 0;
    unsigned char *midi_data = build_fixture(&midi_size);

    char *id = NULL;
    UrsatzGuiAnalysisResult import_result =
        ursatz_gui_analysis_import_and_save(pieces_dir, midi_data, midi_size, "framework-fixture.mid", &id);
    free(midi_data);

    check(import_result == URSATZ_GUI_ANALYSIS_OK, "import_and_save succeeds on the fixture");
    if (import_result != URSATZ_GUI_ANALYSIS_OK) {
        return failures > 0 ? 1 : 0;
    }

    UrsatzGuiPieceDetail *detail = NULL;
    UrsatzGuiAnalysisResult get_result = ursatz_gui_analysis_get_piece(pieces_dir, id, &detail);
    check(get_result == URSATZ_GUI_ANALYSIS_OK, "get_piece succeeds for the saved piece");
    free(id);
    if (get_result != URSATZ_GUI_ANALYSIS_OK) {
        return failures > 0 ? 1 : 0;
    }

    check(count_type(detail, "key") >= 1, "a key interpretation was persisted");
    check(count_type(detail, "chord") >= 1, "at least one chord interpretation was persisted");
    check(count_type(detail, "cadence") >= 3, "all 3 real cadence classifications were persisted");
    check(count_type(detail, "phrase") >= 1, "phrase boundary/signal interpretations were persisted");
    check(count_type(detail, "motif") >= 2, "both matching motif windows (phrases B and C) were persisted");
    check(count_type(detail, "period") >= 1, "the (A, B) period pair was persisted");
    check(count_type(detail, "sentence") >= 2, "the A/B/C sentence (statement/repetition/continuation) was persisted");
    check(count_type(detail, "thematic_return") >= 1, "phrase C's thematic return to phrase B was persisted");

    /* Form Structure Detection never finds a structural cadence in this
     * branch's wiring (it requires a plain "authentic (PAC)"/"authentic
     * (IAC)" label, which needs soprano-resolution data this branch
     * never supplies to Cadence Classification -- see
     * run_form_and_thematic_return_detection's own comment). Asserting
     * this explicitly, rather than silently allowing it either way,
     * keeps that limitation visible instead of masking it as "did not
     * get around to testing it". */
    check(count_type(detail, "form") == 0,
          "form structure interpretations are legitimately absent (no structural cadence reachable yet)");

    check(count_predicate(detail, "precedes") >= 1, "a period Relationship (precedes) was persisted");
    check(count_predicate(detail, "transformed_from") >= 2, "sentence Relationships (transformed_from) were persisted");
    check(count_predicate(detail, "resembles") >= 1, "a thematic return Relationship (resembles) was persisted");

    ursatz_gui_piece_detail_destroy(detail);

    return failures > 0 ? 1 : 0;
}
