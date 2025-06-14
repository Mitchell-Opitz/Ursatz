#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

#include "cJSON.h"

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

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static double number(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : -1e9;
}

static const char *text(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static double rational_of(const cJSON *rational) {
    return member(rational, "num")->valuedouble / member(rational, "den")->valuedouble;
}

static bool within(const cJSON *inner, const cJSON *outer) {
    return rational_of(member(inner, "start")) >= rational_of(member(outer, "start")) - 1e-12 &&
           rational_of(member(inner, "end")) <= rational_of(member(outer, "end")) + 1e-12;
}

static void check_claim(const cJSON *claim, size_t note_count) {
    CHECK(strcmp(text(claim, "framework"), "standard-form-v1") == 0);
    CHECK(rational_of(member(member(claim, "span"), "start")) < rational_of(member(member(claim, "span"), "end")));
    const cJSON *ids = member(claim, "note_ids");
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
    return cJSON_GetArrayItem(member(document, "themes"), index);
}

/* The measures object {first, last} equals first..last. */
static bool measures_are(const cJSON *measures, int first, int last) {
    return (int)number(measures, "first") == first && (int)number(measures, "last") == last;
}

/* The type of the cadence the part ends with, or "none". */
static const char *part_cadence(const cJSON *document, const cJSON *part) {
    const cJSON *index = member(part, "cadence");
    if (!cJSON_IsNumber(index)) {
        return "none";
    }
    return text(cJSON_GetArrayItem(member(document, "cadences"), (int)index->valuedouble), "type");
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
    CHECK(cJSON_GetArraySize(member(document, "themes")) == 1);
    const cJSON *theme = theme_at(document, 0);
    CHECK(theme != NULL);
    if (theme) {
        check_claim(theme, notes.count);
        CHECK(strcmp(text(theme, "label"), "sentence") == 0);
        CHECK(strcmp(text(theme, "beginning"), "presentation") == 0);
        CHECK(strcmp(text(theme, "ending"), "continuation") == 0);
        CHECK(strcmp(text(theme, "commonality"), "very-common") == 0);
        CHECK((int)number(theme, "index") == 0);
        CHECK((int)number(member(theme, "phrases"), "first") == 0 && (int)number(member(theme, "phrases"), "count") == 1);
        CHECK(measures_are(member(theme, "measures"), 1, 8) && (int)number(theme, "length") == 8);
        CHECK(cJSON_IsFalse(member(theme, "parts_are_phrases")));
        CHECK(cJSON_IsFalse(member(theme, "part_of_compound")));
        CHECK(member(theme, "halves") == NULL);
        CHECK(cJSON_IsArray(member(theme, "traits")));
        const cJSON *parts = member(theme, "parts");
        CHECK(cJSON_GetArraySize(parts) == 2);
        const cJSON *presentation = cJSON_GetArrayItem(parts, 0);
        const cJSON *continuation = cJSON_GetArrayItem(parts, 1);
        CHECK(strcmp(text(presentation, "name"), "presentation") == 0);
        CHECK(measures_are(member(presentation, "measures"), 1, 4) && (int)number(presentation, "length") == 4);
        CHECK(strcmp(part_cadence(document, presentation), "none") == 0);
        CHECK(strcmp(text(continuation, "name"), "continuation") == 0);
        CHECK(measures_are(member(continuation, "measures"), 5, 8) && (int)number(continuation, "length") == 4);
        CHECK(strcmp(part_cadence(document, continuation), "perfect-authentic") == 0);
        CHECK(cJSON_IsTrue(member(theme, "cadences_usual")));
        CHECK(cJSON_IsTrue(member(theme, "lengths_usual")));
    }
    /* a balanced sentence is neither expanded nor contracted */
    CHECK(member(cJSON_GetArrayItem(member(document, "phrases"), 0), "expansions") == NULL);
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
    CHECK(cJSON_GetArraySize(member(document, "themes")) == 1);
    CHECK(cJSON_GetArraySize(member(document, "phrases")) == 2);
    const cJSON *theme = theme_at(document, 0);
    CHECK(theme != NULL);
    if (theme) {
        check_claim(theme, notes.count);
        CHECK(strcmp(text(theme, "label"), "period") == 0);
        CHECK(strcmp(text(theme, "beginning"), "antecedent") == 0);
        CHECK(strcmp(text(theme, "ending"), "consequent") == 0);
        CHECK(strcmp(text(theme, "commonality"), "very-common") == 0);
        CHECK((int)number(member(theme, "phrases"), "first") == 0 && (int)number(member(theme, "phrases"), "count") == 2);
        CHECK(measures_are(member(theme, "measures"), 1, 8) && (int)number(theme, "length") == 8);
        CHECK(cJSON_IsTrue(member(theme, "parts_are_phrases")));
        const cJSON *parts = member(theme, "parts");
        const cJSON *antecedent = cJSON_GetArrayItem(parts, 0);
        const cJSON *consequent = cJSON_GetArrayItem(parts, 1);
        CHECK(strcmp(text(antecedent, "name"), "antecedent") == 0);
        CHECK(measures_are(member(antecedent, "measures"), 1, 4));
        /* the constructed antecedent ends on an imperfect authentic cadence: the block chords give no half cadence the
           library accepts (V followed by I reads as an authentic cadence), see tests/musicxml/README.md */
        CHECK(strcmp(part_cadence(document, antecedent), "imperfect-authentic") == 0);
        CHECK(strcmp(text(consequent, "name"), "consequent") == 0);
        CHECK(measures_are(member(consequent, "measures"), 5, 8));
        CHECK(strcmp(part_cadence(document, consequent), "perfect-authentic") == 0);
        /* each part ends with the cadence of its phrase */
        const cJSON *phrases = member(document, "phrases");
        CHECK(number(antecedent, "cadence") == number(cJSON_GetArrayItem(phrases, 0), "cadence"));
        CHECK(number(consequent, "cadence") == number(cJSON_GetArrayItem(phrases, 1), "cadence"));
        CHECK(cJSON_IsFalse(member(theme, "cadences_usual")));
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
    CHECK(cJSON_GetArraySize(member(document, "themes")) == 3);
    const cJSON *first = theme_at(document, 0);
    const cJSON *second = theme_at(document, 1);
    const cJSON *compound = theme_at(document, 2);
    if (first && second && compound) {
        CHECK(strcmp(text(first, "label"), "sentence") == 0 && cJSON_IsTrue(member(first, "part_of_compound")));
        CHECK(strcmp(text(second, "label"), "sentence") == 0 && cJSON_IsTrue(member(second, "part_of_compound")));
        CHECK(measures_are(member(first, "measures"), 1, 8) && measures_are(member(second, "measures"), 9, 16));
        CHECK(strcmp(text(compound, "label"), "compound-period") == 0);
        CHECK(cJSON_IsNull(member(compound, "beginning")) && cJSON_IsNull(member(compound, "ending")));
        CHECK(measures_are(member(compound, "measures"), 1, 16));
        const cJSON *halves = member(compound, "halves");
        CHECK(cJSON_GetArraySize(halves) == 2);
        CHECK((int)cJSON_GetArrayItem(halves, 0)->valuedouble == 0 && (int)cJSON_GetArrayItem(halves, 1)->valuedouble == 1);
        CHECK(member(first, "halves") == NULL);
        /* the second half's cadence is the stronger one */
        const cJSON *parts = member(compound, "parts");
        CHECK(strcmp(part_cadence(document, cJSON_GetArrayItem(parts, 0)), "imperfect-authentic") == 0);
        CHECK(strcmp(part_cadence(document, cJSON_GetArrayItem(parts, 1)), "perfect-authentic") == 0);
    }
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

/* The phrase claims of the document, indexed. */
static const cJSON *phrase_at(const cJSON *document, int index) {
    return cJSON_GetArrayItem(member(document, "phrases"), index);
}

static void check_themes(const cJSON *document, size_t note_count) {
    const cJSON *themes = member(document, "themes");
    const cJSON *cadences = member(document, "cadences");
    int phrase_count = cJSON_GetArraySize(member(document, "phrases"));
    int theme_count = cJSON_GetArraySize(themes);
    int index = 0;
    int last_first_phrase = -1;
    const cJSON *theme;
    cJSON_ArrayForEach(theme, themes) {
        check_claim(theme, note_count);
        CHECK((int)number(theme, "index") == index);
        const char *form = text(theme, "label");
        CHECK(strcmp(form, "sentence") == 0 || strcmp(form, "period") == 0 || strcmp(form, "hybrid") == 0 ||
              strcmp(form, "compound-period") == 0 || strcmp(form, "repeated-phrase") == 0);
        int first = (int)number(member(theme, "phrases"), "first");
        int count = (int)number(member(theme, "phrases"), "count");
        CHECK(first >= 0 && count >= 1 && count <= 2 && first + count <= phrase_count);
        /* themes follow the phrases in order (a compound period after its two sentences, so it may start again) */
        CHECK(first >= last_first_phrase || strcmp(form, "compound-period") == 0);
        last_first_phrase = first;

        /* the theme lies inside its phrases */
        const cJSON *start_phrase = phrase_at(document, first);
        const cJSON *end_phrase = phrase_at(document, first + count - 1);
        CHECK(start_phrase && end_phrase);
        if (start_phrase && end_phrase) {
            CHECK(rational_of(member(member(theme, "span"), "start")) >=
                  rational_of(member(member(start_phrase, "span"), "start")) - 1e-12);
            CHECK(rational_of(member(member(theme, "span"), "end")) <=
                  rational_of(member(member(end_phrase, "span"), "end")) + 1e-12);
            CHECK((int)number(member(theme, "measures"), "first") == (int)number(member(start_phrase, "measures"), "first"));
            CHECK((int)number(member(theme, "measures"), "last") == (int)number(member(end_phrase, "measures"), "last"));
        }
        int first_measure = (int)number(member(theme, "measures"), "first");
        int last_measure = (int)number(member(theme, "measures"), "last");
        CHECK(last_measure >= first_measure);

        /* the parts lie inside the theme, in order, and a part's cadence arrives in it */
        const cJSON *parts = member(theme, "parts");
        CHECK(cJSON_GetArraySize(parts) == 2);
        int previous_last = first_measure - 1;
        const cJSON *part;
        cJSON_ArrayForEach(part, parts) {
            const cJSON *measures = member(part, "measures");
            int part_first = (int)number(measures, "first");
            int part_last = (int)number(measures, "last");
            CHECK(part_first >= first_measure && part_last <= last_measure && part_last >= part_first);
            CHECK(part_first > previous_last || strcmp(form, "repeated-phrase") == 0);
            CHECK((int)number(part, "length") >= 1);
            previous_last = part_last;
            const cJSON *cadence_index = member(part, "cadence");
            if (cJSON_IsNumber(cadence_index)) {
                const cJSON *cadence = cJSON_GetArrayItem(cadences, (int)cadence_index->valuedouble);
                CHECK(cadence != NULL);
                if (cadence) {
                    const cJSON *arrival_start = member(member(member(cadence, "arrival"), "location"), "start");
                    int arrival = (int)number(arrival_start, "measure");
                    CHECK(arrival >= part_first && arrival <= part_last);
                }
            }
        }
        CHECK(cJSON_IsFalse(member(theme, "parts_are_phrases")) == (count == 1) ||
              strcmp(form, "repeated-phrase") == 0 || strcmp(form, "compound-period") == 0);

        /* only a compound period has halves, and they are themes inside it that are part of a compound */
        const cJSON *halves = member(theme, "halves");
        CHECK((halves != NULL) == (strcmp(form, "compound-period") == 0));
        const cJSON *half;
        cJSON_ArrayForEach(half, halves) {
            int h = (int)half->valuedouble;
            CHECK(h >= 0 && h < theme_count && h < index);
            if (h >= 0 && h < theme_count) {
                CHECK(cJSON_IsTrue(member(theme_at(document, h), "part_of_compound")));
            }
        }
        index++;
    }
}

static void check_expansions(const cJSON *document) {
    int theme_count = cJSON_GetArraySize(member(document, "themes"));
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, member(document, "phrases")) {
        const cJSON *expansions = member(phrase, "expansions");
        CHECK(expansions == NULL || cJSON_GetArraySize(expansions) > 0);
        const cJSON *expansion;
        cJSON_ArrayForEach(expansion, expansions) {
            const char *kind = text(expansion, "label");
            const char *change = text(expansion, "change");
            CHECK(strcmp(kind, "internal") == 0 || strcmp(kind, "external") == 0 || strcmp(kind, "contraction") == 0);
            CHECK(strcmp(change, "expansion") == 0 || strcmp(change, "contraction") == 0);
            CHECK((strcmp(kind, "contraction") == 0) == (strcmp(change, "contraction") == 0));
            const cJSON *theme = member(expansion, "theme");
            CHECK(cJSON_IsNull(theme) || (theme->valuedouble >= 0 && (int)theme->valuedouble < theme_count));
            CHECK(cJSON_IsNull(theme) || strcmp(kind, "external") == 0 || cJSON_IsNumber(theme));
            int expected = (int)number(expansion, "expected_length");
            int actual = (int)number(expansion, "actual_length");
            int changed = (int)number(expansion, "measures_changed");
            CHECK(changed >= 1);
            CHECK(strcmp(change, "expansion") == 0 ? actual - expected == changed : expected - actual == changed);
            CHECK(within(member(expansion, "span"), member(phrase, "span")));
            CHECK((strcmp(kind, "external") == 0) == (member(expansion, "external_kind") != NULL));
            CHECK((int)number(member(expansion, "measures"), "first") >= (int)number(member(phrase, "measures"), "first"));
            CHECK((int)number(member(expansion, "measures"), "last") <=
                  (int)number(member(phrase, "measures"), "last") + (int)number(phrase, "extension_measures"));
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
    CHECK((size_t)cJSON_GetArraySize(member(document, "themes")) == report.theme_count);
    int expansions = 0;
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, member(document, "phrases")) {
        expansions += cJSON_GetArraySize(member(phrase, "expansions"));
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
