#ifndef URSATZ_CHAIN_TESTS_CHAIN_TEST_PIECE_H
#define URSATZ_CHAIN_TESTS_CHAIN_TEST_PIECE_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/import_notes.h"
#include "io/musicxml_importer.h"

/*
 * A small constructed piece for the chain tests: four measures of 4/4 in C major, a chorale texture of three voices, I IV V I. The
 * soprano moves in quarter notes, the alto and the bass hold a whole note per measure. It is written as MusicXML and read by the
 * importer, so the notes are what an import gives (events, ids, spellings, voices).
 */

typedef struct {
    MusicXmlImport import;
    AnalysisNote *notes;
    size_t count;
    AnalysisNoteSet set;
} TestPiece;

static inline void test_piece_note(char *out, size_t size, const char *step, int octave, int duration, int voice) {
    snprintf(out, size, "<note><pitch><step>%c</step><octave>%d</octave></pitch><duration>%d</duration><voice>%d</voice></note>\n",
             step[0], octave, duration, voice);
}

static inline void test_piece_init(TestPiece *piece) {
    static const char *const SOPRANO[4][4] = {{"E5", "D5", "C5", "E5"}, {"C5", "A4", "F4", "A4"}, {"D5", "D5", "B4", "G4"},
                                              {"C5", "E5", "G4", "C5"}};
    static const char *const ALTO[4] = {"G4", "C4", "D4", "E4"};
    static const char *const BASS[4] = {"C3", "F3", "G3", "C3"};
    char xml[16384];
    size_t used = 0;
    used += (size_t)snprintf(xml + used, sizeof(xml) - used,
                             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<score-partwise version=\"3.1\">\n"
                             "<part-list><score-part id=\"P1\"><part-name>Chorale</part-name></score-part></part-list>\n"
                             "<part id=\"P1\">\n");
    for (int m = 0; m < 4; m++) {
        used += (size_t)snprintf(xml + used, sizeof(xml) - used, "<measure number=\"%d\">\n", m + 1);
        if (m == 0) {
            used += (size_t)snprintf(xml + used, sizeof(xml) - used,
                                     "<attributes><divisions>4</divisions><key><fifths>0</fifths><mode>major</mode></key>"
                                     "<time><beats>4</beats><beat-type>4</beat-type></time></attributes>\n");
        }
        char note[256];
        for (int b = 0; b < 4; b++) {
            test_piece_note(note, sizeof(note), SOPRANO[m][b], SOPRANO[m][b][1] - '0', 4, 1);
            used += (size_t)snprintf(xml + used, sizeof(xml) - used, "%s", note);
        }
        used += (size_t)snprintf(xml + used, sizeof(xml) - used, "<backup><duration>16</duration></backup>\n");
        test_piece_note(note, sizeof(note), ALTO[m], ALTO[m][1] - '0', 16, 2);
        used += (size_t)snprintf(xml + used, sizeof(xml) - used, "%s<backup><duration>16</duration></backup>\n", note);
        test_piece_note(note, sizeof(note), BASS[m], BASS[m][1] - '0', 16, 3);
        used += (size_t)snprintf(xml + used, sizeof(xml) - used, "%s</measure>\n", note);
    }
    snprintf(xml + used, sizeof(xml) - used, "</part>\n</score-partwise>\n");
    memset(piece, 0, sizeof(*piece));
    assert(musicxml_import_buffer(xml, strlen(xml), &piece->import) == MUSICXML_IMPORT_OK);
    assert(musicxml_import_analysis_notes(&piece->import, &piece->notes, &piece->count));
    piece->set.notes = piece->notes;
    piece->set.count = piece->count;
    piece->set.map = piece->import.meter_map;
    assert(analysis_note_set_valid(&piece->set));
}

static inline void test_piece_destroy(TestPiece *piece) {
    import_analysis_notes_destroy(piece->notes, piece->count);
    musicxml_import_destroy(&piece->import);
    memset(piece, 0, sizeof(*piece));
}

#endif
