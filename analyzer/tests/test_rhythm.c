#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer_internal/analysis_document.h>
#include <io/imported_score.h>

#include "cJSON.h"

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The rhythm and meter of the report.
 *
 * Field 5 (Nocturne No. 5 in B-flat major, in 12/8), the measures of tests/fixtures/annotations/field5_rhythm.txt, a
 * DRAFT hand reading that Ursatz's tests/integration/test_rhythm_field5.c scores the rhythm adapter against. Ursatz
 * scores the annotated phrase spans; the Analyzer reads the phrases the form adapter detects from the whole chain, so
 * the hyperdownbeats are compared against those, and the adapter cannot report an elided cadence (the form adapter does
 * not), so it has no reinterpretation to match. Run with no argument it asserts the structure of the claims and the
 * floors of what is measured:
 *
 *   syncopations   the annotated one (measure 14, beat 2) is found
 *   hemiolas       12/8 has none in standard-meter, and none is annotated
 *   hyperdownbeats 5 of the 8 annotated are the first measure of a hypermeasure (measures 18, 20 and 22 are missed;
 *                  measured at 243bd316)
 *
 * The target is what Ursatz's test asserts: at least 80% of the hyperdownbeats and every reinterpretation. It is
 * printed and does not gate (R-698): the hyperdownbeats are 5 of 8, and the reinterpretation of measure 9 (the phrase
 * ending there is elided in the annotation) is not reported.
 *
 * Run as `constructed` it checks small pieces built here, on which the right answer is known: eighth-note triplets in
 * 4/4 (one run of six attacks, a borrowed division at the beat), a duplet in 6/8, and a hemiola in 3/4.
 */

#define FLOOR_HYPERDOWNBEATS 5
#define BEAT_TOLERANCE 0.01

/* --- the analysis of a file --- */

typedef struct {
    ImportedScore notes;
    ChainResult report;
    cJSON *document;
} Analysis;

static bool analyze(const char *path, Analysis *analysis) {
    memset(analysis, 0, sizeof(*analysis));
    if (imported_score_import_file(path, IMPORTED_FORMAT_MIDI, &analysis->notes, NULL) != IMPORTED_SCORE_OK) {
        return false;
    }
    if (!test_analyze_report(&analysis->notes, &analysis->report)) {
        imported_score_destroy(&analysis->notes);
        return false;
    }
    char *json = test_render_document(path, &analysis->notes, &analysis->report);
    analysis->document = json ? cJSON_Parse(json) : NULL;
    free(json);
    return analysis->document != NULL;
}

static void analysis_destroy(Analysis *analysis) {
    cJSON_Delete(analysis->document);
    test_release_report(&analysis->report);
    imported_score_destroy(&analysis->notes);
}

/* The rhythm claims with the label, in document order. */
static size_t claims_of(const cJSON *document, const char *label, const cJSON **out, size_t capacity) {
    size_t count = 0;
    const cJSON *claim;
    cJSON_ArrayForEach(claim, test_json_member(document, "rhythm")) {
        if (strcmp(test_json_string(claim, "label"), label) == 0 && count < capacity) {
            out[count++] = claim;
        }
    }
    return count;
}

/* --- the structure of the claims --- */

static void check_structure(const Analysis *analysis) {
    const ChainResult *report = &analysis->report;
    const cJSON *rhythm = test_json_member(analysis->document, "rhythm");
    size_t expected = report->density_count + report->syncopation_count + report->hemiola_count +
                      report->hypermeter_count + report->tuplet_count + (report->meter_check.present ? 1 : 0);
    CHECK(cJSON_IsArray(rhythm) && (size_t)cJSON_GetArraySize(rhythm) == expected);
    CHECK(report->density_count > 0);

    /* one profile of all notes per phrase, then one per line that has a note in it, whose notes add up */
    size_t phrases = 0;
    size_t all_notes = 0;
    size_t line_notes = 0;
    for (size_t i = 0; i < report->density_count; i++) {
        const AnalysisDensity *density = &report->densities[i];
        CHECK(density->phrase < report->phrase_count);
        CHECK(density->attacks <= density->notes && density->note_id_count == density->notes);
        CHECK(density->on_beat_share >= 0.0 && density->on_beat_share + density->off_beat_share <= 1.000001);
        if (density->line < 0) {
            CHECK(phrases == 0 || line_notes == all_notes);
            phrases++;
            all_notes = density->notes;
            line_notes = 0;
        } else {
            line_notes += density->notes;
        }
        unsigned counted = 0;
        for (unsigned b = 0; b < density->bin_count; b++) {
            counted += density->bins[b].count;
            CHECK(b == 0 || density->bins[b].value.num * (long)density->bins[b - 1].value.den >
                                density->bins[b - 1].value.num * (long)density->bins[b].value.den);
        }
        CHECK(density->bins_truncated || counted == density->notes);
    }
    CHECK(phrases == report->phrase_count && (phrases == 0 || line_notes == all_notes));

    /* a syncopation is one note, held through a stronger level than it was attacked at */
    for (size_t i = 0; i < report->syncopation_count; i++) {
        const AnalysisSyncopation *syncopation = &report->syncopations[i];
        CHECK(syncopation->note < analysis->notes.count && syncopation->strength >= 1 && syncopation->strength <= 4);
        CHECK(strcmp(syncopation->attack_level, syncopation->through_level) != 0);
    }

    /* the hypermeasures of a phrase follow one another from its first measure, and the first of each is a hyperdownbeat */
    for (size_t i = 0; i < report->hypermeter_count; i++) {
        const AnalysisHypermeter *hypermeter = &report->hypermeters[i];
        CHECK(hypermeter->phrase < report->phrase_count && (hypermeter->length == 2 || hypermeter->length == 4));
        CHECK(hypermeter->hypermeasure_count > 0 && hypermeter->hypermeasures[0].first_measure == hypermeter->first_measure);
        for (size_t h = 1; h < hypermeter->hypermeasure_count; h++) {
            CHECK(hypermeter->hypermeasures[h].first_measure == hypermeter->hypermeasures[h - 1].last_measure + 1);
        }
    }
}

/* --- the annotation of Field 5 --- */

typedef struct {
    struct {
        int measure;
        double beat;
    } syncopations[64];
    size_t syncopation_count;
    int hemiolas[16];
    size_t hemiola_count;
    int hyperdownbeats[64];
    size_t hyperdownbeat_count;
    int reinterpretations[16];
    size_t reinterpretation_count;
} Annotation;

static bool load_annotation(const char *path, Annotation *annotation) {
    memset(annotation, 0, sizeof(*annotation));
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        int a = 0;
        int b = 0;
        double beat = 0.0;
        if (sscanf(line, "syncopation %d %lf", &a, &beat) == 2 && annotation->syncopation_count < 64) {
            annotation->syncopations[annotation->syncopation_count].measure = a;
            annotation->syncopations[annotation->syncopation_count++].beat = beat;
        } else if (sscanf(line, "hemiola %d %d", &a, &b) == 2 && annotation->hemiola_count < 16) {
            annotation->hemiolas[annotation->hemiola_count++] = a;
        } else if (sscanf(line, "hyperdownbeat %d", &a) == 1 && annotation->hyperdownbeat_count < 64) {
            annotation->hyperdownbeats[annotation->hyperdownbeat_count++] = a;
        } else if (sscanf(line, "reinterpretation %d", &a) == 1 && annotation->reinterpretation_count < 16) {
            annotation->reinterpretations[annotation->reinterpretation_count++] = a;
        }
    }
    fclose(file);
    return annotation->syncopation_count > 0 && annotation->hyperdownbeat_count > 0;
}

static void check_field5(const Analysis *analysis, const Annotation *annotation) {
    const ChainResult *report = &analysis->report;
    size_t sync_hits = 0;
    for (size_t i = 0; i < annotation->syncopation_count; i++) {
        bool found = false;
        for (size_t s = 0; s < report->syncopation_count && !found; s++) {
            found = report->syncopations[s].measure == annotation->syncopations[i].measure &&
                    fabs(report->syncopations[s].beat - annotation->syncopations[i].beat) < BEAT_TOLERANCE;
        }
        sync_hits += found ? 1 : 0;
        if (!found) {
            printf("  missed syncopation at measure %d beat %.2f\n", annotation->syncopations[i].measure,
                   annotation->syncopations[i].beat);
        }
    }
    size_t hemiola_hits = 0;
    for (size_t i = 0; i < annotation->hemiola_count; i++) {
        for (size_t h = 0; h < report->hemiola_count; h++) {
            if (report->hemiolas[h].first_measure == annotation->hemiolas[i]) {
                hemiola_hits++;
                break;
            }
        }
    }
    size_t hyper_hits = 0;
    for (size_t i = 0; i < annotation->hyperdownbeat_count; i++) {
        bool found = false;
        for (size_t p = 0; p < report->hypermeter_count && !found; p++) {
            for (size_t h = 0; h < report->hypermeters[p].hypermeasure_count && !found; h++) {
                found = report->hypermeters[p].hypermeasures[h].first_measure == annotation->hyperdownbeats[i];
            }
        }
        hyper_hits += found ? 1 : 0;
        if (!found) {
            printf("  missed hyperdownbeat at measure %d\n", annotation->hyperdownbeats[i]);
        }
    }
    printf("Field 5: %zu densities, %zu syncopations (annotated found %zu of %zu), %zu hemiolas (%zu of %zu annotated), "
           "%zu hypermeters (hyperdownbeats found %zu of %zu)\n",
           report->density_count, report->syncopation_count, sync_hits, annotation->syncopation_count,
           report->hemiola_count, hemiola_hits, annotation->hemiola_count, report->hypermeter_count, hyper_hits,
           annotation->hyperdownbeat_count);
    printf("reinterpretations: %zu annotated, not reported (the chain has no reinterpretation member, chain_result.h)\n",
           annotation->reinterpretation_count);
    CHECK(sync_hits == annotation->syncopation_count);
    CHECK(hemiola_hits == annotation->hemiola_count);
    CHECK(hyper_hits >= FLOOR_HYPERDOWNBEATS);
    puts("target: 80% of the hyperdownbeats and every reinterpretation (not gating)");
}

/* --- the constructed pieces --- */

typedef struct {
    unsigned start; /* ticks */
    unsigned length;
    unsigned char pitch;
} TestNote;

#define TICKS_PER_QUARTER 480

static void put_variable(unsigned char *buf, size_t *pos, unsigned value) {
    unsigned char bytes[5];
    int count = 0;
    bytes[count++] = value & 0x7f;
    while ((value >>= 7) > 0) {
        bytes[count++] = (unsigned char)((value & 0x7f) | 0x80);
    }
    while (count > 0) {
        buf[(*pos)++] = bytes[--count];
    }
}

/* Writes a type 0 file of the notes to the path: a time signature of numerator over 2^exponent, when numerator is not
   0 (otherwise the library's 4/4), and a note on and off for each note, in time order. */
static bool write_midi(const char *path, unsigned numerator, unsigned exponent, const TestNote *notes, size_t count) {
    unsigned char track[4096];
    size_t pos = 0;
    if (numerator > 0) {
        const unsigned char meter[] = {0x00, 0xFF, 0x58, 0x04, (unsigned char)numerator, (unsigned char)exponent, 24, 8};
        memcpy(track, meter, sizeof(meter));
        pos = sizeof(meter);
    }
    /* the events: a note on at its start and a note off at its end, in order of time (an off before an on) */
    unsigned last = 0;
    size_t on = 0;
    size_t off = 0;
    while (on < count || off < count) {
        bool next_on = on < count && (off >= count || notes[on].start < notes[off].start + notes[off].length);
        unsigned time = next_on ? notes[on].start : notes[off].start + notes[off].length;
        put_variable(track, &pos, time - last);
        track[pos++] = next_on ? 0x90 : 0x80;
        track[pos++] = next_on ? notes[on].pitch : notes[off].pitch;
        track[pos++] = next_on ? 80 : 0;
        on += next_on ? 1 : 0;
        off += next_on ? 0 : 1;
        last = time;
    }
    const unsigned char end[] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(track + pos, end, sizeof(end));
    pos += sizeof(end);
    unsigned char file_bytes[4096 + 64];
    size_t file_size = test_smf_type0(file_bytes, TICKS_PER_QUARTER, track, pos);
    return test_write_file(path, (const char *)file_bytes, file_size);
}

/* Four measures of 4/4: eighth-note triplets on the first two beats of the first, then quarter notes. */
static void check_triplets(void) {
    char *path = test_temp_path("test_triplets.mid");
    TestNote notes[32];
    size_t count = 0;
    for (unsigned i = 0; i < 6; i++) {
        notes[count++] = (TestNote){i * 160, 160, (unsigned char)(72 + i % 3)};
    }
    notes[count++] = (TestNote){960, 480, 67};
    notes[count++] = (TestNote){1440, 480, 65};
    for (unsigned measure = 1; measure < 4; measure++) {
        for (unsigned beat = 0; beat < 4; beat++) {
            notes[count++] = (TestNote){measure * 1920 + beat * 480, 480, (unsigned char)(60 + beat)};
        }
    }
    CHECK(write_midi(path, 0, 0, notes, count));
    Analysis analysis;
    CHECK(analyze(path, &analysis));
    const cJSON *claims[8];
    size_t found = claims_of(analysis.document, "tuplet", claims, 8);
    CHECK(found == 1);
    if (found == 1) {
        CHECK(test_json_number(test_json_member(claims[0], "ratio"), "actual", -1e9) == 3 && test_json_number(test_json_member(claims[0], "ratio"), "normal", -1e9) == 2);
        CHECK(strcmp(test_json_string(claims[0], "name"), "triplet") == 0 && strcmp(test_json_string(claims[0], "meter"), "simple") == 0);
        CHECK(cJSON_IsTrue(test_json_member(claims[0], "borrowed")));
        CHECK(strcmp(test_json_string(claims[0], "level"), "beat") == 0 && cJSON_IsTrue(test_json_member(claims[0], "level_typical")));
        CHECK(test_json_number(claims[0], "attacks", -1e9) == 6 && cJSON_GetArraySize(test_json_member(claims[0], "note_ids")) == 6);
        CHECK(test_json_number(claims[0], "measure", -1e9) == 1 && fabs(test_json_number(claims[0], "beat", -1e9) - 1.0) < BEAT_TOLERANCE);
    }
    check_structure(&analysis);
    analysis_destroy(&analysis);
    remove(path);
    free(path);
}

/* Three measures of 6/8: a duplet on the first beat of the first (two notes of a dotted eighth), then eighths. */
static void check_duplet(void) {
    char *path = test_temp_path("test_duplet.mid");
    TestNote notes[32];
    size_t count = 0;
    notes[count++] = (TestNote){0, 360, 72};
    notes[count++] = (TestNote){360, 360, 74};
    for (unsigned i = 0; i < 3; i++) {
        notes[count++] = (TestNote){720 + i * 240, 240, (unsigned char)(72 - 1 - (int)i)};
    }
    for (unsigned measure = 1; measure < 3; measure++) {
        for (unsigned i = 0; i < 6; i++) {
            notes[count++] = (TestNote){measure * 1440 + i * 240, 240, (unsigned char)(60 + i)};
        }
    }
    CHECK(write_midi(path, 6, 3, notes, count));
    Analysis analysis;
    CHECK(analyze(path, &analysis));
    const cJSON *claims[8];
    size_t found = claims_of(analysis.document, "tuplet", claims, 8);
    CHECK(found == 1);
    if (found == 1) {
        CHECK(test_json_number(test_json_member(claims[0], "ratio"), "actual", -1e9) == 2 && test_json_number(test_json_member(claims[0], "ratio"), "normal", -1e9) == 3);
        CHECK(strcmp(test_json_string(claims[0], "name"), "duplet") == 0 && strcmp(test_json_string(claims[0], "meter"), "compound") == 0);
        CHECK(cJSON_IsTrue(test_json_member(claims[0], "borrowed")));
        CHECK(strcmp(test_json_string(claims[0], "level"), "beat") == 0);
        CHECK(test_json_number(claims[0], "attacks", -1e9) == 2);
    }
    check_structure(&analysis);
    analysis_destroy(&analysis);
    remove(path);
    free(path);
}

/* Six measures of 3/4: quarter notes, then two measures of three half notes (a hemiola), then quarter notes. */
static void check_hemiola(void) {
    char *path = test_temp_path("test_hemiola.mid");
    static const unsigned char quarters[] = {72, 74, 76, 77, 79, 81, 74, 72, 71, 72, 74, 72};
    static const unsigned char halves[] = {79, 77, 76};
    TestNote notes[32];
    size_t count = 0;
    for (unsigned i = 0; i < 6; i++) {
        notes[count++] = (TestNote){i * 480, 480, quarters[i]};
    }
    for (unsigned i = 0; i < 3; i++) {
        notes[count++] = (TestNote){2880 + i * 960, 960, halves[i]};
    }
    for (unsigned i = 0; i < 6; i++) {
        notes[count++] = (TestNote){5760 + i * 480, 480, quarters[6 + i]};
    }
    CHECK(write_midi(path, 3, 2, notes, count));
    Analysis analysis;
    CHECK(analyze(path, &analysis));
    const cJSON *claims[8];
    size_t found = claims_of(analysis.document, "hemiola", claims, 8);
    CHECK(found == 1);
    if (found == 1) {
        CHECK(test_json_number(test_json_member(claims[0], "measures"), "first", -1e9) == 3 && test_json_number(test_json_member(claims[0], "measures"), "last", -1e9) == 4);
        CHECK(test_json_number(test_json_member(claims[0], "evidence"), "agreeing", -1e9) >= 2);
        CHECK(cJSON_GetArraySize(test_json_member(claims[0], "note_ids")) == 3);
    }
    check_structure(&analysis);
    analysis_destroy(&analysis);
    remove(path);
    free(path);
}

int main(int argc, char **argv) {
    bool constructed = argc > 1 && strcmp(argv[1], "constructed") == 0;
    if (constructed) {
        check_triplets();
        check_duplet();
        check_hemiola();
    } else {
        char path[1024];
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/Field 5.mid", URSATZ_FIXTURES_DIR);
        Analysis analysis;
        Annotation annotation;
        if (!analyze(path, &analysis)) {
            fprintf(stderr, "cannot analyze %s\n", path);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/annotations/field5_rhythm.txt", URSATZ_FIXTURES_DIR);
        if (!load_annotation(path, &annotation)) {
            fprintf(stderr, "cannot read %s\n", path);
            analysis_destroy(&analysis);
            return 1;
        }
        check_structure(&analysis);
        check_field5(&analysis, &annotation);
        analysis_destroy(&analysis);
    }
    return test_finish("test_rhythm");
}
