#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The schemas and sequences of the report, on the hand-written galant fixture of Ursatz's tests/integration/
 * test_schema_sequence.c (tests/fixtures/galant/galant_fixture.txt: 24 measures in C major planted with a Romanesca, a
 * Prinner, a Do-Re-Mi, a Meyer, a Fonte, a Monte, a Romanesca in G, a cycle of descending fifths and an ascending 5-6,
 * each with its span in galant_annotation.txt). Ursatz scores the adapters on the fixture's own segments (every planted
 * item found) and, separately, through its whole chain on the fixture written as a MIDI file; the Analyzer has only the
 * second, so this test writes the same MIDI file and runs the Analyzer's whole chain on it:
 *
 *   A planted item is FOUND when the report has the same schema name (or sequence kind) with its start and its end each
 *   within one beat of the annotated ones. The matches that are none of the planted ones are reported, not asserted.
 *
 * Asserted: at least CHAIN_FLOOR_FOUND of the planted items found (Ursatz asserts a floor of 4 of 9 for its own chain,
 * which found 5; the Analyzer's whole chain finds 3 since P42), and the structure of every schema and sequence in the report: entry ranges in
 * order, stages inside them, degrees in range, confidence parts in [0, 1], the library's names.
 *
 * On the reference pieces (the Mozart 545 and Clementi 36 of Ursatz's own test, printed there and not asserted) the
 * matches are pinned to what the library gives, with the structure checked, and the Markdown names them. The library
 * finds no schema in the Bach 114, Clementi 36, Field 1 or Field 5 outer voices.
 */

/* 3 since Ursatz P42: keys placed on phrases give the galant fixture's romanesca at beats 52 to 56 another reading, so
   the chain finds 3 of the 9 planted items (Ursatz's own galant floor stays 4: its run stops before the form stage). */
#define CHAIN_FLOOR_FOUND 3

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* --- the fixture --- */

typedef struct {
    int measure;
    double beat;
    double length;
    int key_pc;
    int bass;
    int soprano;
    int root_pc;
    char quality[4];
} FixtureSegment;

typedef struct {
    FixtureSegment segments[256];
    size_t count;
} Fixture;

static bool load_fixture(const char *path, Fixture *fixture) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    memset(fixture, 0, sizeof(*fixture));
    char line[512];
    while (fgets(line, sizeof(line), file) && fixture->count < 256) {
        FixtureSegment *s = &fixture->segments[fixture->count];
        int inversion = 0;
        int applied = 0;
        if (line[0] != '#' && sscanf(line, "seg %d %lf %lf %d %d %d %d %3s %d %d", &s->measure, &s->beat, &s->length,
                                     &s->key_pc, &s->bass, &s->soprano, &s->root_pc, s->quality, &inversion,
                                     &applied) == 10) {
            fixture->count++;
        }
    }
    fclose(file);
    return fixture->count > 0;
}

typedef struct {
    unsigned long tick;
    int order; /* note-offs before note-ons at one tick */
    unsigned char status;
    unsigned char key;
} MidiEvent;

static int compare_events(const void *a, const void *b) {
    const MidiEvent *x = a;
    const MidiEvent *y = b;
    if (x->tick != y->tick) {
        return x->tick < y->tick ? -1 : 1;
    }
    return (x->order > y->order) - (x->order < y->order);
}

static size_t put_vlq(unsigned char *out, unsigned long value) {
    unsigned char bytes[5];
    size_t n = 0;
    bytes[n++] = (unsigned char)(value & 0x7f);
    while ((value >>= 7) != 0) {
        bytes[n++] = (unsigned char)((value & 0x7f) | 0x80);
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = bytes[n - 1 - i];
    }
    return n;
}

/* The chord tones (the root's quality) that are not the bass or the soprano, placed between them. */
static size_t inner_pitches(const FixtureSegment *f, int *out) {
    static const int TRIAD[4][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 3, 6, -1}, {0, 4, 7, 10}};
    int kind = f->quality[0] == 'M' ? 0 : (f->quality[0] == 'm' ? 1 : (f->quality[0] == 'd' ? 2 : 3));
    size_t n = 0;
    for (int t = 0; t < 4; t++) {
        if (TRIAD[kind][t] < 0) {
            continue;
        }
        int pc = (f->root_pc + TRIAD[kind][t]) % 12;
        if (pc == f->bass % 12 || pc == f->soprano % 12) {
            continue;
        }
        int middle = (f->bass + f->soprano) / 2;
        int best = -1;
        for (int pitch = f->bass + 1; pitch < f->soprano; pitch++) {
            if (pitch % 12 == pc && (best < 0 || abs(pitch - middle) < abs(best - middle))) {
                best = pitch;
            }
        }
        if (best >= 0) {
            out[n++] = best;
        }
    }
    return n;
}

/* Writes a format 0 standard MIDI file of the fixture, 480 ticks to the quarter, 4/4. */
static bool write_midi(const Fixture *fixture, const char *path) {
    const unsigned long division = 480;
    MidiEvent *events = calloc(fixture->count * 14, sizeof(MidiEvent));
    unsigned char *track = events ? calloc(fixture->count * 14 * 8 + 64, 1) : NULL;
    if (!track) {
        free(events);
        return false;
    }
    size_t n = 0;
    for (size_t i = 0; i < fixture->count; i++) {
        const FixtureSegment *f = &fixture->segments[i];
        unsigned long start = (unsigned long)((((f->measure - 1) * 4.0) + (f->beat - 1.0)) * (double)division + 0.5);
        unsigned long end = start + (unsigned long)(f->length * (double)division + 0.5);
        int pitches[8];
        size_t count = 0;
        pitches[count++] = f->bass;
        pitches[count++] = f->soprano;
        count += inner_pitches(f, pitches + count);
        for (size_t k = 0; k < count; k++) {
            events[n++] = (MidiEvent){start, 1, 0x90, (unsigned char)pitches[k]};
            events[n++] = (MidiEvent){end, 0, 0x80, (unsigned char)pitches[k]};
        }
    }
    qsort(events, n, sizeof(MidiEvent), compare_events);
    static const unsigned char header[] = {0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
                                           0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20};
    memcpy(track, header, sizeof(header));
    size_t t = sizeof(header);
    unsigned long previous = 0;
    for (size_t i = 0; i < n; i++) {
        t += put_vlq(track + t, events[i].tick - previous);
        previous = events[i].tick;
        track[t++] = events[i].status;
        track[t++] = events[i].key;
        track[t++] = events[i].status == 0x90 ? 80 : 0;
    }
    static const unsigned char end_of_track[] = {0x00, 0xff, 0x2f, 0x00};
    memcpy(track + t, end_of_track, sizeof(end_of_track));
    t += sizeof(end_of_track);
    unsigned char head[22] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (unsigned char)(division >> 8),
                              (unsigned char)(division & 0xff), 'M', 'T', 'r', 'k', (unsigned char)(t >> 24),
                              (unsigned char)(t >> 16), (unsigned char)(t >> 8), (unsigned char)t};
    FILE *file = fopen(path, "wb");
    bool ok = file && fwrite(head, 1, sizeof(head), file) == sizeof(head) && fwrite(track, 1, t, file) == t;
    if (file) {
        fclose(file);
    }
    free(track);
    free(events);
    return ok;
}

/* --- the annotation --- */

typedef struct {
    bool schema;
    char name[40];
    double start; /* in beats from the start, quarter notes */
    double end;
} Planted;

typedef struct {
    Planted items[32];
    size_t count;
} PlantedList;

static bool load_planted(const char *path, PlantedList *list) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    memset(list, 0, sizeof(*list));
    char line[512];
    while (fgets(line, sizeof(line), file) && list->count < 32) {
        char type[16];
        char name[40];
        int m1 = 0;
        int m2 = 0;
        double b1 = 0.0;
        double b2 = 0.0;
        if (line[0] != '#' && sscanf(line, "%15s %39s %d %lf %d %lf", type, name, &m1, &b1, &m2, &b2) == 6) {
            Planted *p = &list->items[list->count++];
            p->schema = strcmp(type, "schema") == 0;
            snprintf(p->name, sizeof(p->name), "%s", name);
            p->start = (m1 - 1) * 4.0 + (b1 - 1.0);
            p->end = (m2 - 1) * 4.0 + (b2 - 1.0);
        }
    }
    fclose(file);
    return list->count > 0;
}

/* --- the report --- */

typedef struct {
    ImportedNotes notes;
    AnalysisReport report;
} Analysis;

static bool analyze(const char *path, Analysis *analysis) {
    memset(analysis, 0, sizeof(*analysis));
    if (midi_pipeline_import_file(path, &analysis->notes) != MIDI_PIPELINE_OK ||
        !run_analysis_pipeline(&analysis->notes, &analysis->report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        failures++;
        return false;
    }
    return true;
}

static void analysis_destroy(Analysis *analysis) {
    analysis_report_destroy(&analysis->report);
    imported_notes_destroy(&analysis->notes);
}

/* The start of an entry's chord and the end of another's, in beats (quarter notes) from the start of the piece. */
static double beats_start(const AnalysisReport *report, size_t entry) {
    const AnalysisReportEntry *e = &report->entries[entry];
    return 4.0 * (double)e->time_numerator / (double)e->time_denominator;
}

static double beats_end(const AnalysisReport *report, size_t entry) {
    const AnalysisReportEntry *e = &report->entries[entry];
    return 4.0 * (double)e->end_numerator / (double)e->end_denominator;
}

static bool near_planted(const Planted *p, double start, double end) {
    return fabs(start - p->start) <= 1.0 + 1e-9 && fabs(end - p->end) <= 1.0 + 1e-9;
}

static bool planted_found(const AnalysisReport *report, const Planted *p) {
    if (p->schema) {
        for (size_t k = 0; k < report->schema_count; k++) {
            const AnalysisSchema *s = &report->schemas[k];
            if (strcmp(s->name, p->name) == 0 &&
                near_planted(p, beats_start(report, s->first_entry), beats_end(report, s->last_entry))) {
                return true;
            }
        }
    } else {
        for (size_t k = 0; k < report->sequence_count; k++) {
            const AnalysisSequence *s = &report->sequences[k];
            if (strcmp(s->kind, p->name) == 0 &&
                near_planted(p, beats_start(report, s->first_entry), beats_end(report, s->last_entry))) {
                return true;
            }
        }
    }
    return false;
}

/* The structure of a degree: a letter distance 1 to 7 and an alteration of at most two semitones. */
static bool degree_ok(int degree, int alteration) {
    return degree >= 1 && degree <= 7 && alteration >= -2 && alteration <= 2;
}

static bool unit_ok(double x) {
    return x >= 0.0 && x <= 1.0;
}

static void check_structure(const AnalysisReport *report) {
    size_t previous_first = 0;
    for (size_t i = 0; i < report->schema_count; i++) {
        const AnalysisSchema *s = &report->schemas[i];
        CHECK(s->name && s->name[0] && s->function && s->framework);
        CHECK(s->first_entry <= s->last_entry && s->last_entry < report->entry_count);
        CHECK(i == 0 || s->first_entry >= previous_first);
        previous_first = s->first_entry;
        CHECK(s->stage_count >= 2 && s->stage_count <= 8);
        CHECK(degree_ok(s->frame_degree, s->frame_alteration));
        CHECK(unit_ok(s->confidence) && unit_ok(s->pair_share) && unit_ok(s->metric_fit) &&
              unit_ok(s->mean_uncertainty));
        CHECK(fabs(s->uncertainty - (1.0 - s->confidence)) < 1e-9);
        size_t bass_only = 0;
        for (size_t k = 0; k < s->stage_count; k++) {
            const AnalysisSchemaStage *stage = &s->stages[k];
            CHECK(stage->first_entry <= stage->last_entry && stage->first_entry >= s->first_entry &&
                  stage->last_entry <= s->last_entry);
            CHECK(k == 0 || stage->first_entry >= s->stages[k - 1].first_entry);
            CHECK(degree_ok(stage->bass_degree, stage->bass_alteration));
            CHECK(stage->has_melody == !stage->bass_only);
            CHECK(!stage->has_melody || degree_ok(stage->melody_degree, stage->melody_alteration));
            CHECK(!stage->has_second || (stage->second_last_entry <= s->last_entry &&
                                         degree_ok(stage->second_degree, stage->second_alteration)));
            CHECK(unit_ok(stage->credit));
            bass_only += stage->bass_only ? 1 : 0;
        }
        CHECK(bass_only == s->bass_only_stages);
    }
    previous_first = 0;
    for (size_t i = 0; i < report->sequence_count; i++) {
        const AnalysisSequence *s = &report->sequences[i];
        CHECK(s->kind && s->kind[0] && s->framework);
        CHECK(s->first_entry <= s->last_entry && s->last_entry < report->entry_count);
        CHECK(i == 0 || s->first_entry >= previous_first);
        previous_first = s->first_entry;
        CHECK(s->units >= 3 && s->unit_length >= 1);
        CHECK(s->stage_count == (size_t)(s->units * s->unit_length) || s->stage_count > 0);
        CHECK(s->rule_backed == (strcmp(s->kind, "cycle-of-fifths") != 0));
        CHECK(s->rule_backed == (s->pattern != NULL) && s->rule_backed == (s->direction != NULL));
        CHECK(unit_ok(s->confidence) && unit_ok(s->unit_share) && unit_ok(s->mean_uncertainty));
        CHECK(fabs(s->uncertainty - (1.0 - s->confidence)) < 1e-9);
        for (size_t k = 0; k < s->stage_count; k++) {
            const AnalysisSequenceStage *stage = &s->stages[k];
            CHECK(stage->first_entry <= stage->last_entry && stage->first_entry >= s->first_entry &&
                  stage->last_entry <= s->last_entry);
            CHECK(degree_ok(stage->bass_degree, stage->bass_alteration));
            CHECK(!stage->has_soprano || degree_ok(stage->soprano_degree, stage->soprano_alteration));
            CHECK(!stage->has_root || degree_ok(stage->root_degree, stage->root_alteration));
        }
    }
}

/* --- the runs --- */

static void check_fixture(const char *dir) {
    char fixture_path[1024];
    char annotation_path[1024];
    char midi_path[1024];
    snprintf(fixture_path, sizeof(fixture_path), "%s/galant/galant_fixture.txt", dir);
    snprintf(annotation_path, sizeof(annotation_path), "%s/galant/galant_annotation.txt", dir);
    snprintf(midi_path, sizeof(midi_path), "%s/galant_fixture.mid", URSATZ_TEST_OUTPUT_DIR);
    Fixture *fixture = malloc(sizeof(Fixture));
    PlantedList planted;
    if (!fixture || !load_fixture(fixture_path, fixture) || !load_planted(annotation_path, &planted) ||
        !write_midi(fixture, midi_path)) {
        fprintf(stderr, "cannot build the galant fixture from %s\n", fixture_path);
        failures++;
        free(fixture);
        return;
    }
    free(fixture);
    Analysis analysis;
    if (!analyze(midi_path, &analysis)) {
        return;
    }
    const AnalysisReport *report = &analysis.report;
    size_t found = 0;
    for (size_t i = 0; i < planted.count; i++) {
        bool hit = planted_found(report, &planted.items[i]);
        found += hit ? 1 : 0;
        if (!hit) {
            printf("  missed %s %s at beat %.1f to %.1f\n", planted.items[i].schema ? "schema" : "sequence",
                   planted.items[i].name, planted.items[i].start, planted.items[i].end);
        }
    }
    printf("the galant fixture: planted items found %zu of %zu; %zu schema(s) and %zu sequence(s) reported\n", found,
           planted.count, report->schema_count, report->sequence_count);
    CHECK(planted.count == 9);
    CHECK(found >= CHAIN_FLOOR_FOUND);
    check_structure(report);
    analysis_destroy(&analysis);
}

typedef struct {
    const char *file;
    size_t schemas;
    size_t sequences;
    size_t cycles_of_fifths;
    size_t parallel_chords;
} Pinned;

static size_t count_kind(const AnalysisReport *report, const char *kind) {
    size_t n = 0;
    for (size_t i = 0; i < report->sequence_count; i++) {
        n += strcmp(report->sequences[i].kind, kind) == 0 ? 1 : 0;
    }
    return n;
}

static void check_piece(const char *dir, const Pinned *pinned) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", dir, pinned->file);
    Analysis analysis;
    if (!analyze(path, &analysis)) {
        return;
    }
    const AnalysisReport *report = &analysis.report;
    printf("%s: %zu schema(s), %zu sequence(s)\n", pinned->file, report->schema_count, report->sequence_count);
    CHECK(report->schema_count == pinned->schemas);
    CHECK(report->sequence_count == pinned->sequences);
    CHECK(count_kind(report, "cycle-of-fifths") == pinned->cycles_of_fifths);
    CHECK(count_kind(report, "parallel-chords") == pinned->parallel_chords);
    check_structure(report);
    AnalysisDocument *document = analysis_document_build(path, &analysis.notes, report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    CHECK(markdown != NULL);
    if (markdown) {
        bool has_section = strstr(markdown, "## Schemas and sequences") != NULL;
        CHECK(has_section == (pinned->schemas + pinned->sequences > 0));
        CHECK((strstr(markdown, "### Sequences") != NULL) == (pinned->sequences > 0));
        for (size_t i = 0; i < report->schema_count; i++) {
            char row[128];
            snprintf(row, sizeof(row), "| %s | %s |", report->schemas[i].name, report->schemas[i].function);
            CHECK(strstr(markdown, row) != NULL);
        }
    }
    free(markdown);
    analysis_document_destroy(document);
    analysis_destroy(&analysis);
}

int main(void) {
    check_fixture(URSATZ_ANALYZER_FIXTURE_DIR);
    static const Pinned PIECES[] = {
        {"Bach 114.mid", 0, 2, 0, 2},   {"Clementi 36.mid", 0, 0, 0, 0}, {"Mozart 545.mid", 0, 8, 4, 4},
        {"Field 1.mid", 0, 0, 0, 0},    {"Field 5.mid", 0, 0, 0, 0},
    };
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        check_piece(URSATZ_ANALYZER_FIXTURE_DIR, &PIECES[i]);
    }
    return failures == 0 ? 0 : 1;
}
