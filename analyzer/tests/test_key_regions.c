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
 * Key regions, tonicizations, modulations and pivot chords.
 *
 * Reference pieces (usage: test_key_regions clementi | mozart): Clementi Sonatina Op. 36 No. 1 and Mozart
 * K. 545, first movement, are checked against hand-read key regions. A region is asserted over its CORE: the
 * region with two measures trimmed from each edge, or its middle measure when that leaves nothing, because the
 * edges are judgement calls (a window-based detector places a boundary a measure or two from the phrase). A
 * modulation is asserted when a region in the target key starts within two measures of the expected measure and
 * the library classes the change into it as a modulation, not a tonicization. The expected regions are the
 * hand readings in Ursatz's tests/fixtures/annotations (clementi36_key_regions.txt, mozart545_key_regions.txt),
 * which were read from the MIDI notes before the detection was run; they are not checked against a printed
 * score. The test reports every miss and fails if there is one: a miss is a result about the library, not a
 * reason to loosen the expectation.
 *
 * Constructed pieces (usage: test_key_regions constructed): four-beat measures of I, IV, V, I triads, one
 * key per stretch, for a piece with no modulation, an excursion whose cadence makes it a weak modulation, a
 * modulation, and a notated key signature that the music does not fit.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* ---- Reference pieces ---- */

typedef struct {
    int first;
    int last;
    const char *label;
} ExpectedRegion;

typedef struct {
    int measure;
    const char *label; /* the key the change goes into */
} ExpectedModulation;

typedef struct {
    const char *name;
    const char *path;
    const char *home_label; /* NULL: not asserted (Mozart 545 holds three movements, so the file's home key is
                               not the first movement's) */
    const char *closing_label;
    const ExpectedRegion *regions;
    size_t region_count;
    const ExpectedModulation *modulations;
    size_t modulation_count;
} ReferencePiece;

static const ExpectedRegion CLEMENTI_REGIONS[] = {
    {1, 5, "C major"}, {6, 15, "G major"}, {16, 22, "C minor"}, {23, 38, "C major"}};
static const ExpectedModulation CLEMENTI_MODULATIONS[] = {{6, "G major"}, {16, "C minor"}, {23, "C major"}};

/* The development (57-69 and 102-114) is not scored: its keys are too uncertain to assert. */
static const ExpectedRegion MOZART_REGIONS[] = {{1, 12, "C major"},   {13, 28, "G major"},  {29, 40, "C major"},
                                                {41, 56, "G major"},  {70, 78, "F major"},  {79, 101, "C major"},
                                                {115, 123, "F major"}, {124, 146, "C major"}};
static const ExpectedModulation MOZART_MODULATIONS[] = {
    {13, "G major"}, {29, "C major"}, {41, "G major"}, {79, "C major"}, {124, "C major"}};

static const ReferencePiece PIECES[] = {
    {"clementi", URSATZ_ANALYZER_FIXTURE_DIR "/framework-v1-corpus/Clementi 36.mid", "C major", "C major", CLEMENTI_REGIONS,
     sizeof(CLEMENTI_REGIONS) / sizeof(CLEMENTI_REGIONS[0]), CLEMENTI_MODULATIONS,
     sizeof(CLEMENTI_MODULATIONS) / sizeof(CLEMENTI_MODULATIONS[0])},
    {"mozart", URSATZ_ANALYZER_FIXTURE_DIR "/framework-v1-corpus/Mozart 545.mid", NULL, "C major", MOZART_REGIONS,
     sizeof(MOZART_REGIONS) / sizeof(MOZART_REGIONS[0]), MOZART_MODULATIONS,
     sizeof(MOZART_MODULATIONS) / sizeof(MOZART_MODULATIONS[0])},
};

static const AnalysisKeyRegion *region_of_measure(const AnalysisReport *report, int measure, size_t *index) {
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (report->key_regions[i].first_measure <= measure && measure <= report->key_regions[i].last_measure) {
            *index = i;
            return &report->key_regions[i];
        }
    }
    return NULL;
}

static bool is_modulation_extent(AnalysisKeyChangeExtent extent) {
    return extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION || extent == ANALYSIS_KEY_CHANGE_MODULATION;
}

static void check_regions_are_gap_free(const AnalysisReport *report) {
    CHECK(report->key_region_count >= 1);
    CHECK(report->key_change_count + 1 == report->key_region_count);
    for (size_t i = 1; i < report->key_region_count; i++) {
        CHECK(report->key_regions[i].first_measure == report->key_regions[i - 1].last_measure + 1);
        CHECK(report->key_regions[i].start_numerator * (long)report->key_regions[i - 1].end_denominator ==
              report->key_regions[i - 1].end_numerator * (long)report->key_regions[i].start_denominator);
    }
    for (size_t i = 0; i < report->key_region_count; i++) {
        CHECK(report->key_regions[i].uncertainty >= 0.0 && report->key_regions[i].uncertainty <= 1.0);
    }
}

static int run_reference_piece(const ReferencePiece *piece) {
    ImportedNotes notes;
    if (midi_pipeline_import_file(piece->path, &notes) != MIDI_PIPELINE_OK) {
        fprintf(stderr, "%s: cannot import\n", piece->path);
        return 1;
    }
    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "%s: pipeline failed\n", piece->path);
        imported_notes_destroy(&notes);
        return 1;
    }
    check_regions_are_gap_free(&report);

    /* The home key and the closing key. */
    CHECK(report.key_region_count > 0);
    if (report.key_region_count > 0) {
        const char *home = report.key_regions[report.home_region].label;
        const char *closing = report.key_regions[report.key_region_count - 1].label;
        if (piece->home_label && strcmp(home, piece->home_label) != 0) {
            fprintf(stderr, "%s: home key '%s', expected '%s'\n", piece->name, home, piece->home_label);
            failures++;
        }
        if (strcmp(closing, piece->closing_label) != 0) {
            fprintf(stderr, "%s: closing key '%s', expected '%s'\n", piece->name, closing, piece->closing_label);
            failures++;
        }
        if (report.key_regions[0].first_measure != 1 ||
            strcmp(report.key_regions[0].label, piece->regions[0].label) != 0) {
            fprintf(stderr, "%s: opening region is '%s' from m. %d, expected '%s' from m. 1\n", piece->name,
                    report.key_regions[0].label, report.key_regions[0].first_measure, piece->regions[0].label);
            failures++;
        }
    }

    /* Each expected region, over its core. */
    for (size_t r = 0; r < piece->region_count; r++) {
        const ExpectedRegion *expected = &piece->regions[r];
        int core_first = expected->first + 2;
        int core_last = expected->last - 2;
        if (core_first > core_last) {
            core_first = core_last = (expected->first + expected->last) / 2;
        }
        for (int measure = core_first; measure <= core_last; measure++) {
            size_t index = 0;
            const AnalysisKeyRegion *found = region_of_measure(&report, measure, &index);
            if (!found || strcmp(found->label, expected->label) != 0) {
                fprintf(stderr, "%s: m. %d (core of m. %d-%d) is %s, expected %s\n", piece->name, measure,
                        expected->first, expected->last, found ? found->label : "no key", expected->label);
                failures++;
            }
        }
    }

    /* Each principal modulation. */
    for (size_t m = 0; m < piece->modulation_count; m++) {
        const ExpectedModulation *expected = &piece->modulations[m];
        bool found = false;
        for (size_t i = 1; i < report.key_region_count && !found; i++) {
            const AnalysisKeyRegion *region = &report.key_regions[i];
            found = region->first_measure >= expected->measure - 2 && region->first_measure <= expected->measure + 2 &&
                    strcmp(region->label, expected->label) == 0 &&
                    is_modulation_extent(report.key_changes[i - 1].extent);
        }
        if (!found) {
            fprintf(stderr, "%s: no modulation to %s within 2 measures of m. %d\n", piece->name, expected->label,
                    expected->measure);
            failures++;
        }
    }

    /* The document carries the regions and the modulations the report has. */
    char *json = analysis_document_render(piece->path, &notes, &report);
    CHECK(json != NULL);
    if (json) {
        cJSON *document = cJSON_Parse(json);
        CHECK(document != NULL);
        if (document) {
            const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
            const cJSON *regions = cJSON_GetObjectItemCaseSensitive(keys, "regions");
            CHECK((size_t)cJSON_GetArraySize(regions) == report.key_region_count);
            size_t modulations = 0;
            for (size_t i = 0; i < report.key_change_count; i++) {
                modulations += is_modulation_extent(report.key_changes[i].extent) ? 1 : 0;
            }
            CHECK((size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "modulations")) ==
                  modulations);
            cJSON_Delete(document);
        }
        free(json);
    }

    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return failures == 0 ? 0 : 1;
}

/* ---- Constructed pieces ---- */

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

/* One beat is 96 ticks. A chord is three notes sounding one beat. */
static void put_chord(Bytes *track, int first, int second, int third) {
    unsigned char on[] = {0x00, 0x90, (unsigned char)first, 0x64, 0x00, 0x90, (unsigned char)second, 0x64,
                          0x00, 0x90, (unsigned char)third, 0x64};
    unsigned char off[] = {0x60, 0x80, (unsigned char)first, 0x40, 0x00, 0x80, (unsigned char)second, 0x40,
                           0x00, 0x80, (unsigned char)third,  0x40};
    put(track, on, sizeof(on));
    put(track, off, sizeof(off));
}

/* A measure of I, IV, V, I in the major key on tonic (a pitch class). */
static void put_measure(Bytes *track, int tonic) {
    int base = 60 + tonic;
    put_chord(track, base, base + 4, base + 7);
    put_chord(track, base + 5, base + 9, base + 12);
    put_chord(track, base + 7, base + 11, base + 14);
    put_chord(track, base, base + 4, base + 7);
}

typedef struct {
    int tonic;
    int measures;
} Stretch;

/* Writes the piece to path as a type-0 file (division 96, 4/4 by default). A key signature event is written at
   tick 0 when has_signature. */
static void write_piece(const char *path, const Stretch *stretches, size_t stretch_count, bool has_signature,
                        int sharps_flats) {
    Bytes track = {NULL, 0, 0};
    if (has_signature) {
        unsigned char event[] = {0x00, 0xFF, 0x59, 0x02, (unsigned char)(signed char)sharps_flats, 0x00};
        put(&track, event, sizeof(event));
    }
    for (size_t i = 0; i < stretch_count; i++) {
        for (int m = 0; m < stretches[i].measures; m++) {
            put_measure(&track, stretches[i].tonic);
        }
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
    free(track.data);
    free(file.data);
}

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

static int list_size(const Analyzed *analyzed, const char *section) {
    return cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(analyzed->document, section));
}

/* No modulation: one key for the whole piece. */
static void test_no_modulation(const char *path) {
    Stretch stretches[] = {{0, 16}};
    write_piece(path, stretches, 1, false, 0);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->key_region_count == 1);
    CHECK(report->key_change_count == 0);
    if (report->key_region_count == 1) {
        CHECK(strcmp(report->key_regions[0].label, "C major") == 0);
        CHECK(report->key_regions[0].first_measure == 1 && report->key_regions[0].last_measure == 16);
        CHECK(report->home_region == 0);
    }
    CHECK(list_size(&analyzed, "tonicizations") == 0);
    CHECK(list_size(&analyzed, "modulations") == 0);
    CHECK(list_size(&analyzed, "pivot_chords") == 0);
    CHECK(list_size(&analyzed, "notes") == 0);
    release(&analyzed);
}

/* A brief excursion that returns: C major, two measures of G major (the library smooths them into a three-measure region), C major again. Every
   measure of the excursion ends on V to I in G, so a cadence arrives in the new key (R-044): the length rule's
   tonicization is confirmed a weak modulation, the return takes its extent, and the document lists two modulations
   and no tonicization. */
static void test_excursion_with_cadence(const char *path) {
    Stretch stretches[] = {{0, 8}, {7, 2}, {0, 8}};
    write_piece(path, stretches, 3, false, 0);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->key_region_count == 3);
    if (report->key_region_count == 3) {
        CHECK(strcmp(report->key_regions[0].label, "C major") == 0);
        CHECK(strcmp(report->key_regions[1].label, "G major") == 0);
        CHECK(strcmp(report->key_regions[2].label, "C major") == 0);
        CHECK(report->key_changes[0].extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION && !report->key_changes[0].is_return);
        CHECK(report->key_changes[1].extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION && report->key_changes[1].is_return);
        CHECK(report->key_changes[0].cadence_confirmed && report->key_changes[1].cadence_confirmed);
        CHECK(strcmp(report->key_regions[report->home_region].label, "C major") == 0);
    }
    CHECK(list_size(&analyzed, "tonicizations") == 0);
    CHECK(list_size(&analyzed, "modulations") == 2);
    release(&analyzed);
}

/* A lasting change of key: C major, then G major to the end. Whatever the library says about a pivot chord, the
   document's three lists agree with one another and with the report. */
static void test_modulation(const char *path) {
    Stretch stretches[] = {{0, 8}, {7, 8}};
    write_piece(path, stretches, 2, false, 0);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    CHECK(report->key_region_count == 2);
    if (report->key_region_count == 2) {
        CHECK(strcmp(report->key_regions[0].label, "C major") == 0);
        CHECK(strcmp(report->key_regions[1].label, "G major") == 0);
        CHECK(report->key_changes[0].extent == ANALYSIS_KEY_CHANGE_MODULATION);
        CHECK(!report->key_changes[0].is_return);
        CHECK(report->key_regions[1].first_measure >= 7 && report->key_regions[1].first_measure <= 11);
    }
    CHECK(list_size(&analyzed, "tonicizations") == 0);
    CHECK(list_size(&analyzed, "modulations") == 1);
    if (list_size(&analyzed, "modulations") == 1) {
        const cJSON *modulation =
            cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(analyzed.document, "modulations"), 0);
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(modulation, "type");
        const cJSON *pivot_index = cJSON_GetObjectItemCaseSensitive(modulation, "pivot_chord_index");
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(modulation, "extent")->valuestring, "modulation") == 0);
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(modulation, "label")->valuestring, "C major to G major") == 0);
        CHECK(strcmp(type->valuestring, "pivot") == 0 || strcmp(type->valuestring, "direct") == 0);
        CHECK(cJSON_IsNull(pivot_index) == (strcmp(type->valuestring, "direct") == 0));
        CHECK(list_size(&analyzed, "pivot_chords") == (cJSON_IsNull(pivot_index) ? 0 : 1));
        if (list_size(&analyzed, "pivot_chords") == 1) {
            const cJSON *pivot =
                cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(analyzed.document, "pivot_chords"), 0);
            CHECK(cJSON_GetObjectItemCaseSensitive(pivot, "modulation_index")->valuedouble == 0);
            CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(pivot, "from_key")->valuestring, "C major") == 0);
            CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(pivot, "to_key")->valuestring, "G major") == 0);
            int chord_index = (int)cJSON_GetObjectItemCaseSensitive(pivot, "chord_index")->valuedouble;
            CHECK(chord_index >= 0 && (size_t)chord_index < report->entry_count);
        }
    }
    release(&analyzed);
}

/* A file that notates F major (one flat) over music in C major: both are reported, and the disagreement is a
   note, not a silent choice. */
static void test_key_signature_disagreement(const char *path) {
    Stretch stretches[] = {{0, 16}};
    write_piece(path, stretches, 1, true, -1);
    Analyzed analyzed;
    analyze(path, &analyzed);
    const AnalysisReport *report = &analyzed.report;

    const cJSON *signatures = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(analyzed.document, "metadata"), "key_signatures");
    CHECK(cJSON_GetArraySize(signatures) == 1);
    if (cJSON_GetArraySize(signatures) == 1) {
        const cJSON *signature = cJSON_GetArrayItem(signatures, 0);
        CHECK(cJSON_GetObjectItemCaseSensitive(signature, "sharps_flats")->valuedouble == -1);
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(signature, "major_key")->valuestring, "F major") == 0);
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(signature, "minor_key")->valuestring, "D minor") == 0);
    }

    /* The home key is what the music says, not the signature. */
    CHECK(report->key_region_count >= 1);
    if (report->key_region_count >= 1) {
        CHECK(strcmp(report->key_regions[report->home_region].label, "C major") == 0);
    }
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(analyzed.document, "notes");
    bool home_note = false;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(note, "kind")->valuestring, "key_signature_disagreement") == 0);
        home_note = home_note || strcmp(cJSON_GetObjectItemCaseSensitive(note, "scope")->valuestring, "home_key") == 0;
    }
    CHECK(home_note);
    release(&analyzed);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_key_regions clementi|mozart|constructed\n");
        return 2;
    }
    if (strcmp(argv[1], "constructed") == 0) {
        test_no_modulation("/tmp/ursatz_analyzer_key_regions_none.mid");
        test_excursion_with_cadence("/tmp/ursatz_analyzer_key_regions_tonicization.mid");
        test_modulation("/tmp/ursatz_analyzer_key_regions_modulation.mid");
        test_key_signature_disagreement("/tmp/ursatz_analyzer_key_regions_signature.mid");
        remove("/tmp/ursatz_analyzer_key_regions_none.mid");
        remove("/tmp/ursatz_analyzer_key_regions_tonicization.mid");
        remove("/tmp/ursatz_analyzer_key_regions_modulation.mid");
        remove("/tmp/ursatz_analyzer_key_regions_signature.mid");
    } else {
        for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
            if (strcmp(argv[1], PIECES[i].name) == 0) {
                run_reference_piece(&PIECES[i]);
                goto done;
            }
        }
        fprintf(stderr, "unknown case '%s'\n", argv[1]);
        return 2;
    }
done:
    if (failures > 0) {
        fprintf(stderr, "test_key_regions %s: %d failure(s)\n", argv[1], failures);
        return 1;
    }
    printf("test_key_regions %s: ok\n", argv[1]);
    return 0;
}
