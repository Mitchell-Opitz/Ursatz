#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/text_export.h>
#include <ursatz_analyzer/version.h>

/*
 * The piece text export (text_export.h). Format rules are checked on three pieces:
 *   - pickup.musicxml: the header, the pickup numbered 0 with its note on beat 4, claims in the measure where they start,
 *     the cadence placed at its start and naming its arrival, and the measure range (a pickup is in a range that starts
 *     at 0 or 1),
 *   - triplet.musicxml: a 3:2 tuplet and beats that are not terminating decimals,
 *   - Bach 114 (MIDI): the key, tempo, modulation, cadence and motive lines and a tie.
 * and the refusals of analyzer_text_export_piece.
 */

static int failures = 0;

#define CHECK(condition, ...)                                    \
    do {                                                         \
        if (!(condition)) {                                      \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
            failures++;                                          \
        }                                                        \
    } while (0)

typedef struct {
    ImportedNotes notes;
    char *document;
    bool ok;
} Piece;

static bool piece_load(Piece *piece, const char *path, const char *name) {
    memset(piece, 0, sizeof(*piece));
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &piece->notes) != INPUT_PIPELINE_OK) {
        return false;
    }
    AnalyzerResult *result = NULL;
    if (analyzer_analyze(&piece->notes, NULL, &result) != ANALYZER_OK) {
        imported_notes_destroy(&piece->notes);
        return false;
    }
    piece->document = analysis_document_render_result(name, result);
    analyzer_result_destroy(result);
    piece->ok = piece->document != NULL;
    if (!piece->ok) {
        imported_notes_destroy(&piece->notes);
    }
    return piece->ok;
}

static void piece_release(Piece *piece) {
    if (piece->ok) {
        imported_notes_destroy(&piece->notes);
    }
    free(piece->document);
}

static char *export_range(const Piece *piece, const char *title, int first, int last) {
    AnalyzerTextExportOptions options = analyzer_text_export_options_default();
    options.title = title;
    options.first_measure = first;
    options.last_measure = last;
    char *text = NULL;
    AnalyzerStatus status = analyzer_text_export_piece(&piece->notes, piece->document, &options, &text);
    CHECK(status == ANALYZER_OK && text != NULL, "export %d-%d: status %d", first, last, (int)status);
    return text;
}

/* The lines of measure `number`: from its "m<number>" line up to the next measure line. A malloc'd copy, or NULL. */
static char *measure_block(const char *text, const char *number) {
    char marker[32];
    snprintf(marker, sizeof(marker), "\nm%s", number);
    size_t marker_length = strlen(marker);
    const char *at = text;
    while ((at = strstr(at, marker)) != NULL) {
        if (at[marker_length] == '\n' || at[marker_length] == ' ') {
            break;
        }
        at += marker_length;
    }
    if (!at) {
        return NULL;
    }
    at++;
    const char *end = at;
    while ((end = strstr(end, "\nm")) != NULL && !(end[2] >= '0' && end[2] <= '9')) {
        end += 2;
    }
    size_t length = end ? (size_t)(end - at) : strlen(at);
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, at, length);
        copy[length] = '\0';
    }
    return copy;
}

static bool has_measure(const char *text, const char *number) {
    char *block = measure_block(text, number);
    bool found = block != NULL;
    free(block);
    return found;
}

static bool block_has(const char *text, const char *number, const char *needle) {
    char *block = measure_block(text, number);
    bool found = block && strstr(block, needle) != NULL;
    free(block);
    return found;
}

static const char *line_n(const char *text, int n, size_t *length) {
    const char *line = text;
    for (int i = 1; i < n && line; i++) {
        line = strchr(line, '\n');
        line = line ? line + 1 : NULL;
    }
    if (!line) {
        *length = 0;
        return NULL;
    }
    *length = strcspn(line, "\n");
    return line;
}

static bool line_is(const char *text, int n, const char *expected) {
    size_t length = 0;
    const char *line = line_n(text, n, &length);
    return line && length == strlen(expected) && strncmp(line, expected, length) == 0;
}

static bool line_starts(const char *text, int n, const char *prefix) {
    size_t length = 0;
    const char *line = line_n(text, n, &length);
    return line && strncmp(line, prefix, strlen(prefix)) == 0;
}

static void check_pickup_piece(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/pickup.musicxml", URSATZ_MUSICXML_DIR);
    Piece piece;
    if (!piece_load(&piece, path, "pickup.musicxml")) {
        CHECK(false, "cannot analyze %s", path);
        return;
    }
    char *whole = export_range(&piece, NULL, 0, 0);
    if (!whole) {
        piece_release(&piece);
        return;
    }

    /* Header lines. */
    CHECK(line_is(whole, 1, "# Ursatz export v2: piece"), "line 1 is the title line");
    CHECK(URSATZ_ANALYZER_TEXT_EXPORT_VERSION == 2, "the format version constant is 2");
    CHECK(line_is(whole, 2, "title: pickup.musicxml"), "the title is the file name the document records");
    CHECK(line_is(whole, 3, "key: C major | meter: 4/4 | tempo: not declared | measures: 0-4"), "the key, meter, tempo, measures line");
    CHECK(line_is(whole, 4, "clefs: P1 S1 treble"), "the clefs line names each part's opening clefs");
    char commit[13];
    snprintf(commit, sizeof(commit), "%.12s", ursatz_analyzer_ursatz_commit());
    char analyzer_line[160];
    snprintf(analyzer_line, sizeof(analyzer_line), "analyzer: %s ursatz %s", ursatz_analyzer_version(), commit);
    CHECK(line_is(whole, 5, analyzer_line), "the analyzer line names the version and a 12 character commit");
    CHECK(line_is(whole, 6,
                  "notes: <pitch><octave> @<beat> <duration>; r = rest; ~ = tied to next; 3:2 = tuplet; c=<confidence>"),
          "the notes legend");
    CHECK(line_is(whole, 7, ""), "a blank line before the first measure");

    /* The pickup is measure 0, states the meter, and its note is on beat 4 of the bar. */
    CHECK(line_starts(whole, 8, "m0 4/4"), "the first measure line is 'm0 4/4'");
    CHECK(block_has(whole, "0", " P1 S1 V1: G4 @4 q\n"), "the pickup note is G4 on beat 4");
    CHECK(strstr(whole, "\nm1\n") != NULL && strstr(whole, "\nm1 4/4") == NULL, "an unchanged meter is not repeated");
    CHECK(block_has(whole, "1", " P1 S1 V2: C4+E4+G4 @1 w\n"), "a chord joins its pitches with +");

    /* Claims sit in the measure where they start. */
    CHECK(block_has(whole, "0", " key C major c="), "the key region starts in measure 0");
    CHECK(block_has(whole, "0", " (m0-m4)\n"), "and spans m0 to m4");
    CHECK(block_has(whole, "0", " chord G V @4 c="), "the pickup chord has a root, a numeral and beat 4");
    CHECK(block_has(whole, "1", " chord C I @1 c="), "a chord starts in measure 1");
    CHECK(block_has(whole, "1", " phrase 1 m1-m4 c="), "the phrase is numbered from 1 and gives its range");
    CHECK(block_has(whole, "1", " | form A\n"), "the phrase names its form section");
    CHECK(block_has(whole, "1", " form A m1-m4 c="), "the form section starts in measure 1 and spans the phrase");
    CHECK(block_has(whole, "3", " cadence IAC m4 @1 c="), "the cadence is placed where it starts and names where it arrives");
    CHECK(!block_has(whole, "4", " cadence"), "and is not repeated in its arrival measure");
    char *zero = measure_block(whole, "0");
    CHECK(zero && strstr(zero, " key ") < strstr(zero, " chord "), "within a measure the key line comes before the chord line");
    free(zero);

    /* Ranges. */
    char *middle = export_range(&piece, NULL, 2, 3);
    if (middle) {
        CHECK(line_is(middle, 3, "key: C major | meter: 4/4 | tempo: not declared | measures: 2-3 of 0-4"), "the header states the range");
        CHECK(has_measure(middle, "2") && has_measure(middle, "3"), "measures 2 and 3 are in");
        CHECK(!has_measure(middle, "0") && !has_measure(middle, "1") && !has_measure(middle, "4"), "the others are out");
        CHECK(strstr(middle, " key ") == NULL && strstr(middle, " phrase 1 ") == NULL,
              "a claim that starts before the range is not listed");
        CHECK(block_has(middle, "3", " cadence IAC m4 @1 c="), "a claim that starts in the range is listed, even if it arrives after it");
        CHECK(block_has(middle, "2", " chord F IV @1 c="), "the chord of measure 2");
        free(middle);
    }
    char *from_one = export_range(&piece, NULL, 1, 1);
    if (from_one) {
        CHECK(line_starts(from_one, 3, "key: C major | meter: 4/4 | tempo: not declared | measures: 0-1 of 0-4"),
              "a range that starts at 1 includes the pickup");
        CHECK(has_measure(from_one, "0") && has_measure(from_one, "1") && !has_measure(from_one, "2"), "measures 0 and 1");
        free(from_one);
    }
    char *from_zero = export_range(&piece, NULL, 0, 1);
    if (from_zero) {
        CHECK(has_measure(from_zero, "0") && has_measure(from_zero, "1") && !has_measure(from_zero, "2"), "0-1 holds the pickup and measure 1");
        free(from_zero);
    }
    char *no_pickup = export_range(&piece, NULL, 2, 4);
    if (no_pickup) {
        CHECK(!has_measure(no_pickup, "0") && has_measure(no_pickup, "4"), "a range that starts at 2 leaves the pickup out");
        CHECK(line_starts(no_pickup, 3, "key: C major | meter: 4/4 | tempo: not declared | measures: 2-4 of 0-4"), "its header");
        free(no_pickup);
    }
    char *past_end = export_range(&piece, NULL, 4, 99);
    if (past_end) {
        CHECK(has_measure(past_end, "4") && !has_measure(past_end, "3"), "a range past the last measure stops at the last");
        CHECK(line_starts(past_end, 3, "key: C major | meter: 4/4 | tempo: not declared | measures: 4-4 of 0-4"), "its header");
        free(past_end);
    }
    char *everything = export_range(&piece, NULL, 0, 4);
    CHECK(everything && strcmp(everything, whole) == 0, "0-4 is the whole piece");
    free(everything);

    /* A title option replaces the file name. */
    char *titled = export_range(&piece, "My piece", 0, 0);
    if (titled) {
        CHECK(line_is(titled, 2, "title: My piece"), "the title option is printed");
        free(titled);
    }
    char *empty_title = export_range(&piece, "", 0, 0);
    CHECK(empty_title && line_is(empty_title, 2, "title: pickup.musicxml"), "an empty title falls back to the file name");
    free(empty_title);

    free(whole);
    piece_release(&piece);
}

static void check_triplet_piece(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/triplet.musicxml", URSATZ_MUSICXML_DIR);
    Piece piece;
    if (!piece_load(&piece, path, "triplet.musicxml")) {
        CHECK(false, "cannot analyze %s", path);
        return;
    }
    char *text = export_range(&piece, NULL, 0, 0);
    if (text) {
        /* The score writer splits the second triplet note at the beat; the export shows it as written. */
        CHECK(block_has(text, "1", " P1 S1 V1: C5 @1 3:2q, D5 @1+2/3 3:2e~, D5 @2 3:2e, E5 @2+1/3 3:2q, F5 @3 h\n"),
              "a tuplet is written 3:2 before its value, with exact beats and a tie");
        free(text);
    }
    piece_release(&piece);
}

static void check_bach_piece(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Bach 114.mid", URSATZ_ANALYZER_FIXTURE_DIR);
    Piece piece;
    if (!piece_load(&piece, path, "Bach 114.mid")) {
        CHECK(false, "cannot analyze %s", path);
        return;
    }
    char *text = export_range(&piece, NULL, 0, 0);
    if (text) {
        CHECK(line_is(text, 2, "title: Bach 114.mid"), "the title");
        CHECK(line_is(text, 3, "key: G major | meter: 4/4 | tempo: q=70 | measures: 1-24"), "a declared tempo is q=N");
        CHECK(block_has(text, "1", " key G major c="), "the opening key region");
        CHECK(block_has(text, "1", " chord G I @1 c="), "the opening chord");
        CHECK(block_has(text, "1", " motive a m1 @1-@4\n"), "a motive occurrence gives its measure and its onset and end beats");
        CHECK(block_has(text, "1", " motive c m1 @3-m2@2\n"), "an occurrence that ends in a later measure names it");
        /* Bach 114 has no cadence, so keys placed on phrases give it no change of key (P42, L7.4): the opening key line is
           the only one. */
        size_t key_lines = 0;
        for (const char *at = strstr(text, "\n key "); at; at = strstr(at + 1, "\n key ")) {
            key_lines++;
        }
        CHECK(key_lines == 1, "no key line follows measure 1 (no change of key)");
        CHECK(strstr(text, " cadence ") == NULL, "no cadence line on Bach 114 (P41 reads the cadences)");
        CHECK(block_has(text, "2", "B3 @1.5 e~"), "a note tied to the next ends in ~");
        CHECK(block_has(text, "2", "r @2.5 e"), "a rest is r");
        free(text);
    }
    char *range = export_range(&piece, NULL, 5, 6);
    if (range) {
        CHECK(line_is(range, 3, "key: G major | meter: 4/4 | tempo: q=70 | measures: 5-6 of 1-24"), "the range header");
        CHECK(has_measure(range, "5") && has_measure(range, "6") && !has_measure(range, "4") && !has_measure(range, "7"),
              "only measures 5 and 6");
        free(range);
    }
    piece_release(&piece);
}

/* Repeats and returns of the library (P42, R-400): one line at the first measure of each later run, naming the earlier run
   and the later one, and for a return the transposition. */
static void check_repeat_pieces(void) {
    static const struct {
        const char *file;
        const char *name;
        struct {
            const char *measure;
            const char *line;
        } lines[3];
        size_t count;
    } PIECES[] = {
        {"Mozart 545.mid", "Mozart 545.mid",
         {{"29", " repeat of m1-m28 (m29-m56)\n"},
          {"70", " return of m1-m8 (m70-m77), 5 semitones\n"},
          {"102", " repeat of m57-m101 (m102-m146)\n"}},
         3},
        {"Clementi 36.mid", "Clementi 36.mid",
         {{"24", " return of m1-m4 (m24-m27), an octave lower\n"},
          {"31", " return of m8-m12 (m31-m35), -7 semitones\n"}},
         2},
    };
    for (size_t p = 0; p < sizeof(PIECES) / sizeof(PIECES[0]); p++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, PIECES[p].file);
        Piece piece;
        if (!piece_load(&piece, path, PIECES[p].name)) {
            CHECK(false, "cannot analyze %s", path);
            continue;
        }
        char *text = export_range(&piece, NULL, 0, 0);
        if (text) {
            size_t lines = 0;
            for (const char *at = strstr(text, "\n repeat of "); at; at = strstr(at + 1, "\n repeat of ")) {
                lines++;
            }
            for (const char *at = strstr(text, "\n return of "); at; at = strstr(at + 1, "\n return of ")) {
                lines++;
            }
            CHECK(lines == PIECES[p].count, "%s: %zu repeat or return lines, expected %zu", PIECES[p].name, lines,
                  PIECES[p].count);
            for (size_t i = 0; i < PIECES[p].count; i++) {
                CHECK(block_has(text, PIECES[p].lines[i].measure, PIECES[p].lines[i].line), "%s m%s: the line %s",
                      PIECES[p].name, PIECES[p].lines[i].measure, PIECES[p].lines[i].line);
            }
            free(text);
        }
        /* A range that opens after the later run's first measure leaves its line out. */
        char *range = export_range(&piece, NULL, 3, 4);
        if (range) {
            CHECK(strstr(range, " repeat of ") == NULL && strstr(range, " return of ") == NULL,
                  "%s: no repeat line in measures 3 and 4", PIECES[p].name);
            free(range);
        }
        piece_release(&piece);
    }
}

static void check_refusals(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/pickup.musicxml", URSATZ_MUSICXML_DIR);
    Piece piece;
    if (!piece_load(&piece, path, "pickup.musicxml")) {
        CHECK(false, "cannot analyze %s", path);
        return;
    }
    AnalyzerTextExportOptions options = analyzer_text_export_options_default();
    CHECK(options.title == NULL && options.first_measure == 0 && options.last_measure == 0, "the default is the whole piece");
    char *text = (char *)"unchanged";

    CHECK(analyzer_text_export_piece(NULL, piece.document, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "NULL notes");
    text = (char *)"unchanged";
    CHECK(analyzer_text_export_piece(&piece.notes, NULL, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "NULL document");
    text = (char *)"unchanged";
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, NULL, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "NULL options");
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, &options, NULL) == ANALYZER_ERR_ARGUMENT, "NULL text");

    text = NULL;
    CHECK(analyzer_text_export_piece(&piece.notes, "not json", &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "a document that is not JSON");
    CHECK(analyzer_text_export_piece(&piece.notes, "{}", &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "a document with no schema_version");

    char *older = malloc(strlen(piece.document) + 1);
    if (older) {
        strcpy(older, piece.document);
        char *version = strstr(older, "\"schema_version\": 11");
        CHECK(version != NULL, "the document states schema_version 11");
        if (version) {
            version[strlen("\"schema_version\": ")] = '8';
            CHECK(analyzer_text_export_piece(&piece.notes, older, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
                  "a document of another schema version");
        }
        free(older);
    }

    options.first_measure = 3;
    options.last_measure = 2;
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "first greater than last");
    options.first_measure = 5;
    options.last_measure = 0;
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "a first measure with no last measure");
    options.first_measure = 50;
    options.last_measure = 60;
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "no measure in range");
    options.first_measure = -1;
    options.last_measure = 2;
    CHECK(analyzer_text_export_piece(&piece.notes, piece.document, &options, &text) == ANALYZER_ERR_ARGUMENT && text == NULL,
          "a negative measure");
    piece_release(&piece);
}

int main(void) {
    check_pickup_piece();
    check_triplet_piece();
    check_bach_piece();
    check_repeat_pieces();
    check_refusals();
    return failures == 0 ? 0 : 1;
}
