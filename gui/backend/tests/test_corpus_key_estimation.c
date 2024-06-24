/* Regression coverage over the real-world corpus fixtures that exposed
 * the original key-detection bug (see ursatz-analyzer's
 * tests/test_corpus_key_estimation.c for the twin of this test): a
 * fixed-order pitch-class scan with one-directional collection
 * containment let chromatic content in a piece spuriously match the
 * wrong (earlier-tried) tonic, and had no fallback for flat keys at
 * all. Each fixture below is checked against its actual key, exercising
 * both paths this fix adds to analysis_service.c: the MIDI key
 * signature meta-event (Bach Prelude, Mozart 545, Clementi 36, Field 5)
 * and the first/last-chord bass-note fallback for files that carry no
 * key signature (Bach 114, Field 1).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_service.h"

static int failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failures++;
    } else {
        printf("PASS: %s\n", what);
    }
}

typedef struct {
    const char *path;
    const char *expected_key_label;
} CorpusCase;

static const CorpusCase CASES[] = {
    {"../../external/ursatz/tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid", "C major"},
    {"../../external/ursatz/tests/fixtures/framework-v1-corpus/Bach 114.mid", "G major"},
    {"../../external/ursatz/tests/fixtures/framework-v1-corpus/Mozart 545.mid", "C major"},
    {"../../external/ursatz/tests/fixtures/framework-v1-corpus/Clementi 36.mid", "C major"},
    {"../../external/ursatz/tests/fixtures/framework-v1-corpus/Field 1.mid", "Eb major"},
    {"../../external/ursatz/tests/fixtures/framework-v1-corpus/Field 5.mid", "Bb major"},
};

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    unsigned char *buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, file) != (size_t)size) {
        free(buf);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *out_size = (size_t)size;
    return buf;
}

static const char *find_key_label(const UrsatzGuiPieceDetail *detail) {
    for (size_t i = 0; i < detail->interpretation_count; i++) {
        if (strcmp(detail->interpretations[i].type, "key") == 0) {
            return detail->interpretations[i].label;
        }
    }
    return NULL;
}

int main(void) {
    const char *pieces_dir = "test_corpus_key_estimation_pieces";

    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        size_t midi_size = 0;
        unsigned char *midi_data = read_file(CASES[i].path, &midi_size);
        if (!midi_data) {
            fprintf(stderr, "FAIL: could not read fixture %s\n", CASES[i].path);
            failures++;
            continue;
        }

        char *id = NULL;
        UrsatzGuiAnalysisResult import_result =
            ursatz_gui_analysis_import_and_save(pieces_dir, midi_data, midi_size, "corpus-fixture.mid", &id);
        free(midi_data);
        if (import_result != URSATZ_GUI_ANALYSIS_OK) {
            fprintf(stderr, "FAIL: %s: import_and_save failed\n", CASES[i].path);
            failures++;
            continue;
        }

        UrsatzGuiPieceDetail *detail = NULL;
        UrsatzGuiAnalysisResult get_result = ursatz_gui_analysis_get_piece(pieces_dir, id, &detail);
        free(id);
        if (get_result != URSATZ_GUI_ANALYSIS_OK) {
            fprintf(stderr, "FAIL: %s: get_piece failed\n", CASES[i].path);
            failures++;
            continue;
        }

        const char *label = find_key_label(detail);
        char what[256];
        snprintf(what, sizeof(what), "%s: expected key '%s', got '%s'", CASES[i].path, CASES[i].expected_key_label,
                 label ? label : "(no key persisted)");
        check(label != NULL && strcmp(label, CASES[i].expected_key_label) == 0, what);

        ursatz_gui_piece_detail_destroy(detail);
    }

    printf("test_corpus_key_estimation: %s\n", failures > 0 ? "FAILED" : "ok");
    return failures > 0 ? 1 : 0;
}
