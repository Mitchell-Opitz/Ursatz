#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/notation_reading.h>
#include <ursatz_analyzer/playback.h>

#include "test_support.h"

/*
 * The playback timing of an import (playback.h): the tempo regions and notes the GUI asked the library for.
 *   - the five reference pieces (MIDI) and tests/musicxml/pickup.musicxml, field-5.musicxml (score tempo) give the
 *     JSON of tests/snapshots/gui/<stem>.playback.json byte for byte; with UPDATE_SNAPSHOTS=1 (or
 *     URSATZ_UPDATE_SNAPSHOTS=1) the files are rewritten,
 *   - the numbers behind the JSON hold: tempo from 0 in time order, MIDI numbers 0 to 127, the end the latest note end,
 *   - a value scale of 2 multiplies the tempo regions by 2 (and a MIDI file's region positions), as the GUI did,
 *   - the argument errors.
 *
 *   test_playback          the six files and the cases
 *   test_playback smoke    Bach 114, pickup and the cases (the valgrind run)
 */

static const struct {
    const char *dir; /* "midi" or "musicxml" */
    const char *file;
    const char *stem;
    bool smoke;
} FILES[] = {
    {"midi", "Bach 114.mid", "bach-114", true},          {"midi", "Mozart 545.mid", "mozart-545", false},
    {"midi", "Clementi 36.mid", "clementi-36", false},   {"midi", "Field 1.mid", "field-1", false},
    {"midi", "Field 5.mid", "field-5", false},           {"musicxml", "pickup.musicxml", "pickup", true},
    {"musicxml", "field-5.musicxml", "field-5-musicxml", false},
};

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

static void file_path(size_t i, char *path, size_t size) {
    if (strcmp(FILES[i].dir, "midi") == 0) {
        snprintf(path, size, "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, FILES[i].file);
    } else {
        snprintf(path, size, "%s/%s", URSATZ_MUSICXML_DIR, FILES[i].file);
    }
}

static bool import_file(size_t i, ImportedNotes *notes) {
    char path[512];
    file_path(i, path, sizeof(path));
    return input_pipeline_import_file(path, INPUT_FORMAT_AUTO, notes) == INPUT_PIPELINE_OK;
}

static void check_snapshot(size_t i, const char *json, bool update) {
    char path[512];
    snprintf(path, sizeof(path), "%s/gui/%s.playback.json", URSATZ_SNAPSHOT_DIR, FILES[i].stem);
    if (update) {
        CHECK(test_write_file(path, json), "%s: cannot write %s", FILES[i].stem, path);
        return;
    }
    char *expected = test_read_file(path);
    CHECK(expected != NULL, "%s: missing %s (run with UPDATE_SNAPSHOTS=1 to create it)", FILES[i].stem, path);
    if (expected) {
        CHECK(strcmp(expected, json) == 0, "%s: playback JSON differs from %s", FILES[i].stem, path);
    }
    free(expected);
}

static void check_numbers(const char *stem, const ImportedNotes *notes, const AnalyzerPlayback *playback) {
    CHECK(playback->tempo_source != NULL, "%s: no tempo source", stem);
    CHECK(playback->tempo_count >= 1, "%s: no tempo region", stem);
    CHECK(playback->note_count == notes->count, "%s: %zu playback notes for %zu imported", stem, playback->note_count,
          notes->count);
    if (playback->tempo_count > 0) {
        CHECK(rational_is_zero(playback->tempo[0].at), "%s: the first tempo region is not at 0", stem);
    }
    for (size_t i = 1; i < playback->tempo_count; i++) {
        CHECK(rational_cmp(playback->tempo[i - 1].at, playback->tempo[i].at) < 0, "%s: tempo region %zu out of order",
              stem, i);
    }
    for (size_t i = 0; i < playback->tempo_count; i++) {
        CHECK(playback->tempo[i].quarters_per_minute > 0.0, "%s: tempo region %zu is not positive", stem, i);
    }
    Rational latest = rational_ratio(0, 1);
    for (size_t i = 0; i < playback->note_count; i++) {
        const AnalyzerPlaybackNote *note = &playback->notes[i];
        Rational note_end;
        CHECK(note->index == i, "%s: note %zu has index %zu", stem, i, note->index);
        CHECK(note->midi >= 0 && note->midi <= 127, "%s: note %zu has MIDI number %d", stem, i, note->midi);
        CHECK(note->part == notes->streams[i], "%s: note %zu has part %zu", stem, i, note->part);
        if (rational_add(note->onset, note->length, &note_end) == RATIONAL_OK && rational_cmp(note_end, latest) > 0) {
            latest = note_end;
        }
    }
    CHECK(rational_equals(playback->end, latest), "%s: end is %lld/%lld, latest note end %lld/%lld", stem,
          playback->end.num, playback->end.den, latest.num, latest.den);
}

static double round_to_milli(double value) {
    return floor(value * 1000.0 + 0.5) / 1000.0;
}

/* A value scale of 2 multiplies the tempo of every region by 2; a MIDI file's region positions too. */
static void check_scale(void) {
    ImportedNotes notes;
    if (!import_file(0, &notes)) {
        CHECK(false, "scale: cannot import %s", FILES[0].file);
        return;
    }
    AnalyzerPlayback before, after;
    CHECK(analyzer_playback_build(&notes, &before) == ANALYZER_OK, "scale: build before");
    NotationReading reading = notation_reading_none();
    reading.scale_numerator = 2;
    reading.scale_denominator = 1;
    CHECK(notation_reading_apply(&notes, &reading) == NOTATION_READING_OK, "scale: apply");
    CHECK(analyzer_playback_build(&notes, &after) == ANALYZER_OK, "scale: build after");
    CHECK(after.tempo_count == before.tempo_count, "scale: %zu regions after, %zu before", after.tempo_count,
          before.tempo_count);
    for (size_t i = 0; i < before.tempo_count && i < after.tempo_count; i++) {
        Rational doubled;
        CHECK(after.tempo[i].quarters_per_minute == round_to_milli(before.tempo[i].quarters_per_minute * 2.0),
              "scale: region %zu tempo %f after, %f before", i, after.tempo[i].quarters_per_minute,
              before.tempo[i].quarters_per_minute);
        CHECK(rational_mul_int(before.tempo[i].at, 2, &doubled) == RATIONAL_OK &&
                  rational_equals(after.tempo[i].at, doubled),
              "scale: region %zu position not doubled", i);
    }
    CHECK(rational_cmp(after.end, before.end) > 0, "scale: the end did not move");
    analyzer_playback_destroy(&before);
    analyzer_playback_destroy(&after);
    imported_notes_destroy(&notes);
}

static void check_arguments(void) {
    ImportedNotes notes;
    AnalyzerPlayback playback;
    char *json = (char *)1;
    CHECK(analyzer_playback_build(NULL, &playback) == ANALYZER_ERR_ARGUMENT, "NULL notes");
    CHECK(analyzer_playback_build(&notes, NULL) == ANALYZER_ERR_ARGUMENT, "NULL out");
    CHECK(analyzer_playback_to_json(NULL, &notes, &json) == ANALYZER_ERR_ARGUMENT && json == NULL, "NULL playback");
    if (!import_file(0, &notes)) {
        CHECK(false, "arguments: cannot import %s", FILES[0].file);
        return;
    }
    CHECK(analyzer_playback_build(&notes, &playback) == ANALYZER_OK, "build");
    CHECK(analyzer_playback_to_json(&playback, NULL, &json) == ANALYZER_ERR_ARGUMENT && json == NULL, "NULL notes (json)");
    CHECK(analyzer_playback_to_json(&playback, &notes, NULL) == ANALYZER_ERR_ARGUMENT, "NULL out (json)");
    analyzer_playback_destroy(&playback);
    TempoMap *map = notes.tempo_map;
    notes.tempo_map = NULL;
    CHECK(analyzer_playback_build(&notes, &playback) == ANALYZER_ERR_ARGUMENT, "MIDI import with no tempo map");
    notes.tempo_map = map;
    analyzer_playback_destroy(&playback);
    analyzer_playback_destroy(NULL);
    imported_notes_destroy(&notes);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    const char *update_env = getenv("UPDATE_SNAPSHOTS");
    if (!update_env) {
        update_env = getenv("URSATZ_UPDATE_SNAPSHOTS");
    }
    bool update = update_env && strcmp(update_env, "1") == 0;

    for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
        if (smoke && !FILES[i].smoke) {
            continue;
        }
        ImportedNotes notes;
        if (!import_file(i, &notes)) {
            CHECK(false, "%s: cannot import", FILES[i].file);
            continue;
        }
        AnalyzerPlayback playback;
        char *json = NULL;
        AnalyzerStatus status = analyzer_playback_build(&notes, &playback);
        CHECK(status == ANALYZER_OK, "%s: build status %d", FILES[i].stem, (int)status);
        if (status == ANALYZER_OK) {
            check_numbers(FILES[i].stem, &notes, &playback);
            status = analyzer_playback_to_json(&playback, &notes, &json);
            CHECK(status == ANALYZER_OK && json != NULL, "%s: json status %d", FILES[i].stem, (int)status);
            if (json) {
                check_snapshot(i, json, update);
                printf("%s: %zu tempo regions (%s), %zu notes\n", FILES[i].stem, playback.tempo_count,
                       playback.tempo_source, playback.note_count);
            }
        }
        free(json);
        analyzer_playback_destroy(&playback);
        imported_notes_destroy(&notes);
    }
    check_scale();
    check_arguments();
    if (failures > 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all playback checks passed\n");
    return 0;
}
