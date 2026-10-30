#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * Themes and expansions: two constructed pieces the library hears as a sentence and as a period, a third as a compound
 * period, and the five reference pieces.
 *
 * The constructed pieces are eight (or sixteen) measures of block chords in C major (tests/musicxml/README.md). What is
 * checked in them is what the library says, field by field: the form, the names of the two parts, their measures and
 * the cadences that end them. In the reference pieces the library finds hybrids only (see the README, Known limits), so
 * what is checked there is structure, not counts the library cannot reach: the themes are in order, each lies inside its
 * phrases, its parts lie inside it and follow each other, a part's cadence arrives in the part, a compound period's
 * halves are themes it spans, and every expansion names a theme and a phrase that exist and lies inside its phrase.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void check_claim(const cJSON *claim, size_t note_count) {
    CHECK(strcmp(test_json_string(claim, "framework"), "standard-form-v1") == 0);
    CHECK(test_rational_of(test_json_member(test_json_member(claim, "span"), "start")) < test_rational_of(test_json_member(test_json_member(claim, "span"), "end")));
    const cJSON *ids = test_json_member(claim, "note_ids");
    CHECK(cJSON_IsArray(ids));
    double last = -1.0;
    const cJSON *id;
    cJSON_ArrayForEach(id, ids) {
        CHECK(id->valuedouble > last && id->valuedouble < (double)note_count);
        last = id->valuedouble;
    }
}

static bool analyze(const char *path, bool midi, ImportedNotes *notes, AnalysisReport *report, cJSON **document) {
    bool imported = midi ? midi_pipeline_import_file(path, notes) == MIDI_PIPELINE_OK
                         : musicxml_pipeline_import_file(path, notes) == MUSICXML_PIPELINE_OK;
    if (!imported || !run_analysis_pipeline(notes, report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        failures++;
        return false;
    }
    char *json = analysis_document_render(path, notes, report);
    *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(*document != NULL);
    return *document != NULL;
}

static const cJSON *theme_at(const cJSON *document, int index) {
    return cJSON_GetArrayItem(test_json_member(document, "themes"), index);
}

/* The measures object {first, last} equals first..last. */
static bool measures_are(const cJSON *measures, int first, int last) {
    return (int)test_json_number(measures, "first") == first && (int)test_json_number(measures, "last") == last;
}

/* The type of the cadence the part ends with, or "none". */
static const char *part_cadence(const cJSON *document, const cJSON *part) {
    const cJSON *index = test_json_member(part, "cadence");
    if (!cJSON_IsNumber(index)) {
        return "none";
    }
    return test_json_string(cJSON_GetArrayItem(test_json_member(document, "cadences"), (int)index->valuedouble), "type");
}

static void check_sentence(const char *dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/sentence.musicxml", dir);
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = NULL;
    if (!analyze(path, false, &notes, &report, &document)) {
        return;
    }
    CHECK(cJSON_GetArraySize(test_json_member(document, "themes")) == 1);
    const cJSON *theme = theme_at(document, 0);
    CHECK(theme != NULL);
    if (theme) {
        check_claim(theme, notes.count);
        CHECK(strcmp(test_json_string(theme, "label"), "sentence") == 0);
        CHECK(strcmp(test_json_string(theme, "beginning"), "presentation") == 0);
        CHECK(strcmp(test_json_string(theme, "ending"), "continuation") == 0);
        CHECK(strcmp(test_json_string(theme, "commonality"), "very-common") == 0);
        CHECK((int)test_json_number(theme, "index") == 0);
        CHECK((int)test_json_number(test_json_member(theme, "phrases"), "first") == 0 && (int)test_json_number(test_json_member(theme, "phrases"), "count") == 1);
        CHECK(measures_are(test_json_member(theme, "measures"), 1, 8) && (int)test_json_number(theme, "length") == 8);
        CHECK(cJSON_IsFalse(test_json_member(theme, "parts_are_phrases")));
        CHECK(cJSON_IsFalse(test_json_member(theme, "part_of_compound")));
        CHECK(test_json_member(theme, "halves") == NULL);
        CHECK(cJSON_IsArray(test_json_member(theme, "traits")));
        const cJSON *parts = test_json_member(theme, "parts");
        CHECK(cJSON_GetArraySize(parts) == 2);
        const cJSON *presentation = cJSON_GetArrayItem(parts, 0);
        const cJSON *continuation = cJSON_GetArrayItem(parts, 1);
        CHECK(strcmp(test_json_string(presentation, "name"), "presentation") == 0);
        CHECK(measures_are(test_json_member(presentation, "measures"), 1, 4) && (int)test_json_number(presentation, "length") == 4);
        CHECK(strcmp(part_cadence(document, presentation), "none") == 0);
        CHECK(strcmp(test_json_string(continuation, "name"), "continuation") == 0);
        CHECK(measures_are(test_json_member(continuation, "measures"), 5, 8) && (int)test_json_number(continuation, "length") == 4);
        CHECK(strcmp(part_cadence(document, continuation), "perfect-authentic") == 0);
        CHECK(cJSON_IsTrue(test_json_member(theme, "cadences_usual")));
        CHECK(cJSON_IsTrue(test_json_member(theme, "lengths_usual")));
    }
    /* a balanced sentence is neither expanded nor contracted */
    CHECK(test_json_member(cJSON_GetArrayItem(test_json_member(document, "phrases"), 0), "expansions") == NULL);
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

static void check_period(const char *dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/period.musicxml", dir);
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = NULL;
    if (!analyze(path, false, &notes, &report, &document)) {
        return;
    }
    CHECK(cJSON_GetArraySize(test_json_member(document, "themes")) == 1);
    CHECK(cJSON_GetArraySize(test_json_member(document, "phrases")) == 2);
    const cJSON *theme = theme_at(document, 0);
    CHECK(theme != NULL);
    if (theme) {
        check_claim(theme, notes.count);
        CHECK(strcmp(test_json_string(theme, "label"), "period") == 0);
        CHECK(strcmp(test_json_string(theme, "beginning"), "antecedent") == 0);
        CHECK(strcmp(test_json_string(theme, "ending"), "consequent") == 0);
        CHECK(strcmp(test_json_string(theme, "commonality"), "very-common") == 0);
        CHECK((int)test_json_number(test_json_member(theme, "phrases"), "first") == 0 && (int)test_json_number(test_json_member(theme, "phrases"), "count") == 2);
        CHECK(measures_are(test_json_member(theme, "measures"), 1, 8) && (int)test_json_number(theme, "length") == 8);
        CHECK(cJSON_IsTrue(test_json_member(theme, "parts_are_phrases")));
        const cJSON *parts = test_json_member(theme, "parts");
        const cJSON *antecedent = cJSON_GetArrayItem(parts, 0);
        const cJSON *consequent = cJSON_GetArrayItem(parts, 1);
        CHECK(strcmp(test_json_string(antecedent, "name"), "antecedent") == 0);
        CHECK(measures_are(test_json_member(antecedent, "measures"), 1, 4));
        /* the constructed antecedent ends on an imperfect authentic cadence: the block chords give no half cadence the
           library accepts (V followed by I reads as an authentic cadence), see tests/musicxml/README.md */
        CHECK(strcmp(part_cadence(document, antecedent), "imperfect-authentic") == 0);
        CHECK(strcmp(test_json_string(consequent, "name"), "consequent") == 0);
        CHECK(measures_are(test_json_member(consequent, "measures"), 5, 8));
        CHECK(strcmp(part_cadence(document, consequent), "perfect-authentic") == 0);
        /* each part ends with the cadence of its phrase */
        const cJSON *phrases = test_json_member(document, "phrases");
        CHECK(test_json_number(antecedent, "cadence") == test_json_number(cJSON_GetArrayItem(phrases, 0), "cadence"));
        CHECK(test_json_number(consequent, "cadence") == test_json_number(cJSON_GetArrayItem(phrases, 1), "cadence"));
        CHECK(cJSON_IsFalse(test_json_member(theme, "cadences_usual")));
    }
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

static void check_compound_period(const char *dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/compound-period.musicxml", dir);
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = NULL;
    if (!analyze(path, false, &notes, &report, &document)) {
        return;
    }
    /* two sentences, then the compound period they make up */
    CHECK(cJSON_GetArraySize(test_json_member(document, "themes")) == 3);
    const cJSON *first = theme_at(document, 0);
    const cJSON *second = theme_at(document, 1);
    const cJSON *compound = theme_at(document, 2);
    if (first && second && compound) {
        CHECK(strcmp(test_json_string(first, "label"), "sentence") == 0 && cJSON_IsTrue(test_json_member(first, "part_of_compound")));
        CHECK(strcmp(test_json_string(second, "label"), "sentence") == 0 && cJSON_IsTrue(test_json_member(second, "part_of_compound")));
        CHECK(measures_are(test_json_member(first, "measures"), 1, 8) && measures_are(test_json_member(second, "measures"), 9, 16));
        CHECK(strcmp(test_json_string(compound, "label"), "compound-period") == 0);
        CHECK(cJSON_IsNull(test_json_member(compound, "beginning")) && cJSON_IsNull(test_json_member(compound, "ending")));
        CHECK(measures_are(test_json_member(compound, "measures"), 1, 16));
        const cJSON *halves = test_json_member(compound, "halves");
        CHECK(cJSON_GetArraySize(halves) == 2);
        CHECK((int)cJSON_GetArrayItem(halves, 0)->valuedouble == 0 && (int)cJSON_GetArrayItem(halves, 1)->valuedouble == 1);
        CHECK(test_json_member(first, "halves") == NULL);
        /* the second half's cadence is the stronger one */
        const cJSON *parts = test_json_member(compound, "parts");
        CHECK(strcmp(part_cadence(document, cJSON_GetArrayItem(parts, 0)), "imperfect-authentic") == 0);
        CHECK(strcmp(part_cadence(document, cJSON_GetArrayItem(parts, 1)), "perfect-authentic") == 0);
    }
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

/* The phrase claims of the document, indexed. */
static const cJSON *phrase_at(const cJSON *document, int index) {
    return cJSON_GetArrayItem(test_json_member(document, "phrases"), index);
}

static void check_themes(const cJSON *document, size_t note_count) {
    const cJSON *themes = test_json_member(document, "themes");
    const cJSON *cadences = test_json_member(document, "cadences");
    int phrase_count = cJSON_GetArraySize(test_json_member(document, "phrases"));
    int theme_count = cJSON_GetArraySize(themes);
    int index = 0;
    int last_first_phrase = -1;
    const cJSON *theme;
    cJSON_ArrayForEach(theme, themes) {
        check_claim(theme, note_count);
        CHECK((int)test_json_number(theme, "index") == index);
        const char *form = test_json_string(theme, "label");
        CHECK(strcmp(form, "sentence") == 0 || strcmp(form, "period") == 0 || strcmp(form, "hybrid") == 0 ||
              strcmp(form, "compound-period") == 0 || strcmp(form, "repeated-phrase") == 0);
        int first = (int)test_json_number(test_json_member(theme, "phrases"), "first");
        int count = (int)test_json_number(test_json_member(theme, "phrases"), "count");
        CHECK(first >= 0 && count >= 1 && count <= 2 && first + count <= phrase_count);
        /* themes follow the phrases in order (a compound period after its two sentences, so it may start again) */
        CHECK(first >= last_first_phrase || strcmp(form, "compound-period") == 0);
        last_first_phrase = first;

        /* the theme lies inside its phrases */
        const cJSON *start_phrase = phrase_at(document, first);
        const cJSON *end_phrase = phrase_at(document, first + count - 1);
        CHECK(start_phrase && end_phrase);
        if (start_phrase && end_phrase) {
            CHECK(test_rational_of(test_json_member(test_json_member(theme, "span"), "start")) >=
                  test_rational_of(test_json_member(test_json_member(start_phrase, "span"), "start")) - 1e-12);
            CHECK(test_rational_of(test_json_member(test_json_member(theme, "span"), "end")) <=
                  test_rational_of(test_json_member(test_json_member(end_phrase, "span"), "end")) + 1e-12);
            CHECK((int)test_json_number(test_json_member(theme, "measures"), "first") == (int)test_json_number(test_json_member(start_phrase, "measures"), "first"));
            CHECK((int)test_json_number(test_json_member(theme, "measures"), "last") == (int)test_json_number(test_json_member(end_phrase, "measures"), "last"));
        }
        int first_measure = (int)test_json_number(test_json_member(theme, "measures"), "first");
        int last_measure = (int)test_json_number(test_json_member(theme, "measures"), "last");
        CHECK(last_measure >= first_measure);

        /* the parts lie inside the theme, in order, and a part's cadence arrives in it */
        const cJSON *parts = test_json_member(theme, "parts");
        CHECK(cJSON_GetArraySize(parts) == 2);
        int previous_last = first_measure - 1;
        const cJSON *part;
        cJSON_ArrayForEach(part, parts) {
            const cJSON *measures = test_json_member(part, "measures");
            int part_first = (int)test_json_number(measures, "first");
            int part_last = (int)test_json_number(measures, "last");
            CHECK(part_first >= first_measure && part_last <= last_measure && part_last >= part_first);
            CHECK(part_first > previous_last || strcmp(form, "repeated-phrase") == 0);
            CHECK((int)test_json_number(part, "length") >= 1);
            previous_last = part_last;
            const cJSON *cadence_index = test_json_member(part, "cadence");
            if (cJSON_IsNumber(cadence_index)) {
                const cJSON *cadence = cJSON_GetArrayItem(cadences, (int)cadence_index->valuedouble);
                CHECK(cadence != NULL);
                if (cadence) {
                    const cJSON *arrival_start = test_json_member(test_json_member(test_json_member(cadence, "arrival"), "location"), "start");
                    int arrival = (int)test_json_number(arrival_start, "measure");
                    CHECK(arrival >= part_first && arrival <= part_last);
                }
            }
        }
        CHECK(cJSON_IsFalse(test_json_member(theme, "parts_are_phrases")) == (count == 1) ||
              strcmp(form, "repeated-phrase") == 0 || strcmp(form, "compound-period") == 0);

        /* only a compound period has halves, and they are themes inside it that are part of a compound */
        const cJSON *halves = test_json_member(theme, "halves");
        CHECK((halves != NULL) == (strcmp(form, "compound-period") == 0));
        const cJSON *half;
        cJSON_ArrayForEach(half, halves) {
            int h = (int)half->valuedouble;
            CHECK(h >= 0 && h < theme_count && h < index);
            if (h >= 0 && h < theme_count) {
                CHECK(cJSON_IsTrue(test_json_member(theme_at(document, h), "part_of_compound")));
            }
        }
        index++;
    }
}

static void check_expansions(const cJSON *document) {
    int theme_count = cJSON_GetArraySize(test_json_member(document, "themes"));
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, test_json_member(document, "phrases")) {
        const cJSON *expansions = test_json_member(phrase, "expansions");
        CHECK(expansions == NULL || cJSON_GetArraySize(expansions) > 0);
        const cJSON *expansion;
        cJSON_ArrayForEach(expansion, expansions) {
            const char *kind = test_json_string(expansion, "label");
            const char *change = test_json_string(expansion, "change");
            CHECK(strcmp(kind, "internal") == 0 || strcmp(kind, "external") == 0 || strcmp(kind, "contraction") == 0);
            CHECK(strcmp(change, "expansion") == 0 || strcmp(change, "contraction") == 0);
            CHECK((strcmp(kind, "contraction") == 0) == (strcmp(change, "contraction") == 0));
            const cJSON *theme = test_json_member(expansion, "theme");
            CHECK(cJSON_IsNull(theme) || (theme->valuedouble >= 0 && (int)theme->valuedouble < theme_count));
            CHECK(cJSON_IsNull(theme) || strcmp(kind, "external") == 0 || cJSON_IsNumber(theme));
            int expected = (int)test_json_number(expansion, "expected_length");
            int actual = (int)test_json_number(expansion, "actual_length");
            int changed = (int)test_json_number(expansion, "measures_changed");
            CHECK(changed >= 1);
            CHECK(strcmp(change, "expansion") == 0 ? actual - expected == changed : expected - actual == changed);
            CHECK(test_within(test_json_member(expansion, "span"), test_json_member(phrase, "span")));
            CHECK((strcmp(kind, "external") == 0) == (test_json_member(expansion, "external_kind") != NULL));
            CHECK((int)test_json_number(test_json_member(expansion, "measures"), "first") >= (int)test_json_number(test_json_member(phrase, "measures"), "first"));
            CHECK((int)test_json_number(test_json_member(expansion, "measures"), "last") <=
                  (int)test_json_number(test_json_member(phrase, "measures"), "last") + (int)test_json_number(phrase, "extension_measures"));
        }
    }
}

typedef struct {
    const char *name;
    const char *file;
} Piece;

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid"},
                               {"Clementi 36", "Clementi 36.mid"},
                               {"Mozart 545", "Mozart 545.mid"},
                               {"Field 1", "Field 1.mid"},
                               {"Field 5", "Field 5.mid"}};

static void check_piece(const Piece *piece) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, piece->file);
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = NULL;
    if (!analyze(path, true, &notes, &report, &document)) {
        return;
    }
    CHECK((size_t)cJSON_GetArraySize(test_json_member(document, "themes")) == report.theme_count);
    int expansions = 0;
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, test_json_member(document, "phrases")) {
        expansions += cJSON_GetArraySize(test_json_member(phrase, "expansions"));
    }
    CHECK((size_t)expansions == report.expansion_count);
    printf("%s: %zu themes, %zu expansions or contractions\n", piece->name, report.theme_count, report.expansion_count);
    check_themes(document, notes.count);
    check_expansions(document);
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    check_sentence(URSATZ_MUSICXML_DIR);
    check_period(URSATZ_MUSICXML_DIR);
    check_compound_period(URSATZ_MUSICXML_DIR);
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        check_piece(&PIECES[i]);
    }
    if (failures > 0) {
        fprintf(stderr, "test_themes: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_themes: ok\n");
    return 0;
}
