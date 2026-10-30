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
 * The form plan: sections, thematic returns and the form tree, for the five reference pieces and the three constructed
 * ones (tests/musicxml/README.md).
 *
 * What is checked is the plan's structure, not music the library cannot reach: every phrase lies in exactly one section,
 * the sections follow each other and tile the phrases, the tree's levels are named form, section, phrase, subphrase and
 * idea, each phrase node holds the subphrases and ideas of its phrase claim, a section is closed only by an authentic
 * cadence, the letters follow from the returns alone, and every return is an earlier theme recalled by a later one. The
 * measured section and return counts and the letters are pinned as the library gives them (README, Known limits): Mozart
 * 545 finds 77 returns, though its themes are hybrids and repeated phrases only.
 */

typedef struct {
    const char *name;
    const char *file; /* in the corpus directory (MIDI) or the MusicXML directory */
    bool midi;
    size_t sections;
    size_t returns;
    const char *labels; /* the section labels, in order */
} Piece;

static const Piece PIECES[] = {
    {"Bach 114", "Bach 114.mid", true, 2, 1, "A A'"},
    {"Clementi 36", "Clementi 36.mid", true, 6, 3, "A B C D E F"},
    {"Mozart 545", "Mozart 545.mid", true, 22, 46,
     "A B C D E F G H I J K L M N L' O K' P Q R S T"},
    {"Field 1", "Field 1.mid", true, 5, 0, "A B C D E"},
    {"Field 5", "Field 5.mid", true, 4, 4, "A B A' C"},
    {"sentence", "sentence.musicxml", false, 1, 0, "A"},
    {"period", "period.musicxml", false, 2, 0, "A B"},
    {"compound period", "compound-period.musicxml", false, 2, 1, "A A'"},
};

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static bool measures_equal(const cJSON *a, const cJSON *b) {
    return test_json_number(a, "first") == test_json_number(b, "first") && test_json_number(a, "last") == test_json_number(b, "last");
}

/* Two rationals {num, den} are the same time. */
static bool same_time(const cJSON *a, const cJSON *b) {
    return test_json_number(a, "num") * test_json_number(b, "den") == test_json_number(b, "num") * test_json_number(a, "den");
}

static double time_of(const cJSON *rational) {
    return test_json_number(rational, "num") / test_json_number(rational, "den");
}

static bool position_valid(const cJSON *position) {
    return cJSON_IsObject(position) && test_json_number(position, "measure") >= 0 && test_json_number(position, "beat") >= 1;
}

static void check_claim(const cJSON *claim, size_t note_count) {
    CHECK(strcmp(test_json_string(claim, "framework"), "standard-form-v1") == 0);
    CHECK(position_valid(test_json_member(test_json_member(claim, "location"), "start")) &&
          position_valid(test_json_member(test_json_member(claim, "location"), "end")));
    CHECK(time_of(test_json_member(test_json_member(claim, "span"), "start")) < time_of(test_json_member(test_json_member(claim, "span"), "end")));
    const cJSON *uncertainty = test_json_member(claim, "uncertainty");
    CHECK(cJSON_IsNumber(uncertainty) && uncertainty->valuedouble >= 0 && uncertainty->valuedouble <= 1);
    const cJSON *ids = test_json_member(claim, "note_ids");
    CHECK(cJSON_IsArray(ids));
    double last = -1.0;
    const cJSON *id;
    cJSON_ArrayForEach(id, ids) {
        CHECK(id->valuedouble > last && id->valuedouble < (double)note_count);
        last = id->valuedouble;
    }
}

/* The idea nodes of a list, each at its level and named by a label of the library's, counted. */
static int check_idea_nodes(const cJSON *ideas, int phrase) {
    int count = 0;
    const cJSON *idea;
    cJSON_ArrayForEach(idea, ideas) {
        CHECK(strcmp(test_json_string(idea, "level"), "idea") == 0);
        CHECK((int)test_json_number(idea, "phrase") == phrase);
        CHECK(strcmp(test_json_string(idea, "label"), "basic-idea") == 0 || strcmp(test_json_string(idea, "label"), "contrasting-idea") == 0 ||
              strcmp(test_json_string(idea, "label"), "fragment") == 0 || strcmp(test_json_string(idea, "label"), "unit") == 0);
        count++;
    }
    return count;
}

/* A phrase node holds the subphrases and ideas of its phrase claim, each once, and repeats its measures. */
static void check_phrase_node(const cJSON *node, const cJSON *phrases, int expected) {
    CHECK(strcmp(test_json_string(node, "level"), "phrase") == 0);
    CHECK((int)test_json_number(node, "phrase") == expected);
    const cJSON *claim = cJSON_GetArrayItem(phrases, expected);
    CHECK(claim != NULL);
    if (!claim) {
        return;
    }
    CHECK(measures_equal(test_json_member(node, "measures"), test_json_member(claim, "measures")));
    CHECK(test_json_number(node, "length") == test_json_number(claim, "length"));
    CHECK(cJSON_IsNull(test_json_member(node, "cadence")) == cJSON_IsNull(test_json_member(claim, "cadence")) &&
          (cJSON_IsNull(test_json_member(node, "cadence")) || test_json_number(node, "cadence") == test_json_number(claim, "cadence")));
    const cJSON *subphrases = test_json_member(node, "subphrases");
    CHECK(cJSON_GetArraySize(subphrases) == cJSON_GetArraySize(test_json_member(claim, "subphrases")));
    int ideas = check_idea_nodes(test_json_member(node, "ideas"), expected);
    const cJSON *subphrase;
    int index = 0;
    cJSON_ArrayForEach(subphrase, subphrases) {
        CHECK(strcmp(test_json_string(subphrase, "level"), "subphrase") == 0);
        CHECK((int)test_json_number(subphrase, "phrase") == expected && (int)test_json_number(subphrase, "index") == index++);
        CHECK(measures_equal(test_json_member(subphrase, "measures"),
                             test_json_member(cJSON_GetArrayItem(test_json_member(claim, "subphrases"), index - 1), "measures")));
        ideas += check_idea_nodes(test_json_member(subphrase, "ideas"), expected);
    }
    CHECK(ideas == cJSON_GetArraySize(test_json_member(claim, "ideas")));
}

static bool is_authentic(const char *type) {
    return strcmp(type, "perfect-authentic") == 0 || strcmp(type, "imperfect-authentic") == 0;
}

/* The letters of a label ("M'" is "M"); at most 4 bytes for the families of 26 * 27 sections. */
static void base_letters(const char *label, char *out) {
    size_t n = strcspn(label, "'");
    memcpy(out, label, n);
    out[n] = '\0';
}

static void check_sections(const cJSON *document, const cJSON *form, size_t note_count) {
    const cJSON *sections = test_json_member(form, "sections");
    const cJSON *phrases = test_json_member(document, "phrases");
    const cJSON *cadences = test_json_member(document, "cadences");
    const cJSON *returns = test_json_member(form, "returns");
    int expected_phrase = 0;
    const cJSON *previous = NULL;
    int index = 0;
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        check_claim(section, note_count);
        CHECK(strcmp(test_json_string(section, "level"), "section") == 0);
        CHECK((int)test_json_number(section, "index") == index);
        CHECK(test_json_number(test_json_member(section, "measures"), "first") <= test_json_number(test_json_member(section, "measures"), "last"));
        /* the sections follow each other by measure, as the phrases do: a phrase ends at its cadence's arrival and the
           next begins with the next measure, so the spans do not overlap */
        if (previous) {
            CHECK(time_of(test_json_member(test_json_member(section, "span"), "start")) >= time_of(test_json_member(test_json_member(previous, "span"), "end")));
            CHECK(test_json_number(test_json_member(section, "measures"), "first") == test_json_number(test_json_member(previous, "measures"), "last") + 1);
        }
        /* every phrase lies in exactly one section: the nodes count the phrases up, in order */
        const cJSON *nodes = test_json_member(section, "phrases");
        CHECK(cJSON_GetArraySize(nodes) > 0);
        const cJSON *node;
        const cJSON *first_phrase = cJSON_GetArrayItem(phrases, expected_phrase);
        cJSON_ArrayForEach(node, nodes) {
            check_phrase_node(node, phrases, expected_phrase++);
        }
        const cJSON *last_phrase = cJSON_GetArrayItem(phrases, expected_phrase - 1);
        CHECK(first_phrase && last_phrase);
        if (first_phrase && last_phrase) {
            CHECK(same_time(test_json_member(test_json_member(section, "span"), "start"), test_json_member(test_json_member(first_phrase, "span"), "start")));
            CHECK(same_time(test_json_member(test_json_member(section, "span"), "end"), test_json_member(test_json_member(last_phrase, "span"), "end")));
            CHECK(test_json_number(test_json_member(section, "measures"), "first") == test_json_number(test_json_member(first_phrase, "measures"), "first"));
            /* it ends with its last phrase's cadence */
            CHECK(cJSON_IsNull(test_json_member(section, "cadence")) == cJSON_IsNull(test_json_member(last_phrase, "cadence")) &&
                  (cJSON_IsNull(test_json_member(section, "cadence")) ||
                   test_json_number(section, "cadence") == test_json_number(last_phrase, "cadence")));
        }
        /* a section is closed by an authentic cadence; only the last may be open */
        if (cJSON_IsTrue(test_json_member(section, "closed"))) {
            const cJSON *cadence = cJSON_GetArrayItem(cadences, (int)test_json_number(section, "cadence"));
            CHECK(cJSON_IsNumber(test_json_member(section, "cadence")) && is_authentic(test_json_string(cadence, "type")));
        } else {
            CHECK(index + 1 == cJSON_GetArraySize(sections));
        }
        CHECK(cJSON_IsString(test_json_member(section, "key")) == cJSON_IsNumber(test_json_member(section, "region")));
        /* the letters follow from the returns alone */
        char base[8];
        base_letters(test_json_string(section, "label"), base);
        const cJSON *from = test_json_member(section, "returns_from");
        if (cJSON_IsNumber(from)) {
            const cJSON *earlier = cJSON_GetArrayItem(sections, (int)from->valuedouble);
            char earlier_base[8];
            base_letters(test_json_string(earlier, "label"), earlier_base);
            CHECK(from->valuedouble < (double)index);
            CHECK(strcmp(base, earlier_base) == 0);
            CHECK(strchr(test_json_string(section, "label"), '\'') != NULL);
            const cJSON *item = cJSON_GetArrayItem(returns, (int)test_json_number(section, "return"));
            CHECK(item != NULL && test_json_number(item, "later_section") == (double)index &&
                  test_json_number(item, "earlier_section") == from->valuedouble);
        } else {
            CHECK(cJSON_IsNull(test_json_member(section, "return")));
            CHECK(strchr(test_json_string(section, "label"), '\'') == NULL);
            for (int k = 0; k < index; k++) {
                char other[8];
                base_letters(test_json_string(cJSON_GetArrayItem(sections, k), "label"), other);
                CHECK(strcmp(base, other) != 0);
            }
        }
        previous = section;
        index++;
    }
    CHECK(expected_phrase == cJSON_GetArraySize(phrases));
    const cJSON *first = cJSON_GetArrayItem(sections, 0);
    CHECK(strcmp(test_json_string(first, "label"), "A") == 0);
    CHECK((int)test_json_number(test_json_member(form, "measures"), "first") == (int)test_json_number(test_json_member(first, "measures"), "first"));
    CHECK((int)test_json_number(test_json_member(form, "measures"), "last") ==
          (int)test_json_number(test_json_member(cJSON_GetArrayItem(sections, index - 1), "measures"), "last"));
}

static const cJSON *theme_phrases(const cJSON *document, const cJSON *item, const char *key, int *first, int *count) {
    const cJSON *theme = cJSON_GetArrayItem(test_json_member(document, "themes"), (int)test_json_number(item, key));
    *first = (int)test_json_number(test_json_member(theme, "phrases"), "first");
    *count = (int)test_json_number(test_json_member(theme, "phrases"), "count");
    return theme;
}

/* The index of the section holding phrase `phrase`, or -1. */
static int section_holding(const cJSON *sections, int phrase) {
    int index = 0;
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *node;
        cJSON_ArrayForEach(node, test_json_member(section, "phrases")) {
            if ((int)test_json_number(node, "phrase") == phrase) {
                return index;
            }
        }
        index++;
    }
    return -1;
}

/* A return is a later theme recalling an earlier one: the earlier theme's phrases come first, each theme lies in the
   section of its first phrase, the similarity is the lower of the two scores and reaches the detection's default
   threshold (0.8), and the returns are ordered by later theme, then earlier. */
static void check_returns(const cJSON *document, const cJSON *form, size_t note_count) {
    const cJSON *returns = test_json_member(form, "returns");
    const cJSON *sections = test_json_member(form, "sections");
    double previous_later = -1;
    double previous_earlier = -1;
    const cJSON *item;
    cJSON_ArrayForEach(item, returns) {
        check_claim(item, note_count);
        CHECK(strcmp(test_json_string(item, "label"), "thematic-return") == 0);
        int earlier_first = 0, earlier_count = 0, later_first = 0, later_count = 0;
        const cJSON *earlier = theme_phrases(document, item, "earlier_theme", &earlier_first, &earlier_count);
        const cJSON *later = theme_phrases(document, item, "later_theme", &later_first, &later_count);
        CHECK(earlier != NULL && later != NULL);
        if (!earlier || !later) {
            continue;
        }
        CHECK(earlier_first + earlier_count <= later_first);
        CHECK(measures_equal(test_json_member(item, "earlier"), test_json_member(earlier, "measures")));
        CHECK(measures_equal(test_json_member(item, "later"), test_json_member(later, "measures")));
        CHECK((int)test_json_number(item, "earlier_section") == section_holding(sections, earlier_first));
        CHECK((int)test_json_number(item, "later_section") == section_holding(sections, later_first));
        CHECK(test_json_number(item, "earlier_section") <= test_json_number(item, "later_section"));
        const cJSON *similarity = test_json_member(item, "similarity");
        double value = test_json_number(similarity, "value");
        double lower = test_json_number(similarity, "contour") < test_json_number(similarity, "rhythm") ? test_json_number(similarity, "contour")
                                                                                      : test_json_number(similarity, "rhythm");
        CHECK(value >= 0.8 && value <= 1.0);
        CHECK(value - lower < 1e-9 && lower - value < 1e-9);
        CHECK(test_json_number(similarity, "window") >= 4);
        CHECK(test_json_number(item, "uncertainty") - (1.0 - value) < 1e-9 && (1.0 - value) - test_json_number(item, "uncertainty") < 1e-9);
        double later_theme = test_json_number(item, "later_theme");
        double earlier_theme = test_json_number(item, "earlier_theme");
        CHECK(later_theme > previous_later || (later_theme == previous_later && earlier_theme > previous_earlier));
        previous_later = later_theme;
        previous_earlier = earlier_theme;
    }
}

static void run_piece(const Piece *piece, const char *midi_dir, const char *musicxml_dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", piece->midi ? midi_dir : musicxml_dir, piece->file);
    ImportedNotes notes;
    AnalysisReport report;
    bool imported = piece->midi ? midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK
                                : musicxml_pipeline_import_file(path, &notes) == MUSICXML_PIPELINE_OK;
    if (!imported || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "%s: cannot analyze %s\n", piece->name, path);
        failures++;
        return;
    }
    CHECK(report.section_count == piece->sections);
    CHECK(report.thematic_return_count == piece->returns);
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    if (document) {
        const cJSON *form = test_json_member(document, "form");
        CHECK(cJSON_IsObject(form));
        if (cJSON_IsObject(form)) {
            CHECK(strcmp(test_json_string(form, "level"), "form") == 0);
            CHECK((size_t)cJSON_GetArraySize(test_json_member(form, "sections")) == piece->sections);
            CHECK((size_t)cJSON_GetArraySize(test_json_member(form, "returns")) == piece->returns);
            char labels[512] = "";
            const cJSON *section;
            cJSON_ArrayForEach(section, test_json_member(form, "sections")) {
                if (strlen(labels) + strlen(test_json_string(section, "label")) + 2 < sizeof(labels)) {
                    strcat(labels, labels[0] ? " " : "");
                    strcat(labels, test_json_string(section, "label"));
                }
            }
            if (strcmp(labels, piece->labels) != 0) {
                fprintf(stderr, "%s: section labels \"%s\", expected \"%s\"\n", piece->name, labels, piece->labels);
                failures++;
            }
            check_sections(document, form, notes.count);
            check_returns(document, form, notes.count);
        }
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    char midi_dir[1024];
    snprintf(midi_dir, sizeof(midi_dir), "%s/framework-v1-corpus", URSATZ_ANALYZER_FIXTURE_DIR);
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        run_piece(&PIECES[i], midi_dir, URSATZ_MUSICXML_DIR);
    }
    if (failures > 0) {
        fprintf(stderr, "test_form_plan: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_form_plan: ok\n");
    return 0;
}
