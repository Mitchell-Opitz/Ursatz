#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * Phrases, ideas, subphrases and the phrase model on the five reference pieces (Ursatz's framework-v1-corpus).
 *
 * What is checked is what the document promises, whatever the library finds:
 *   - the phrases tile the analysed range in order: each begins at the measure after the one before ended (and no
 *     earlier than its span), and the first begins no later than the first chord;
 *   - the cadence of a phrase (an index into "cadences", or null) arrives inside the phrase, in its last measure, and
 *     ends no later than the phrase does; a phrase with no cadence says so in its signal;
 *   - the ideas lie inside their phrase, in order, and fill its measures exactly; the subphrases do the same;
 *   - every note id exists, once each and in order; every location is valid;
 *   - the phrase model labels are Tb, PD, D or Te, in that order within a phrase, on chords inside the phrase, and a
 *     chord claim carries the label the phrase lists for it;
 *   - the second pass: the phrases the document reports are the ones found on the final key regions.
 *
 * The phrase counts are what the chain gives (the library's, not hand readings; the hand readings are Ursatz's own
 * test, test_form_reference_pieces). They are asserted so a change in the chain shows; see the README.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct {
    const char *name;
    const char *file;
    int phrases; /* what the chain gives */
} Piece;

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid", 2},
                               {"Clementi 36", "Clementi 36.mid", 10},
                               {"Mozart 545", "Mozart 545.mid", 22},
                               {"Field 1", "Field 1.mid", 10},
                               {"Field 5", "Field 5.mid", 9}};

static bool position_valid(const cJSON *position) {
    return cJSON_IsObject(position) && test_json_number(position, "measure") >= 0 && test_json_number(position, "beat") >= 1;
}

static void check_note_ids(const cJSON *claim, size_t note_count, bool may_be_empty) {
    const cJSON *ids = test_json_member(claim, "note_ids");
    CHECK(cJSON_IsArray(ids));
    CHECK(may_be_empty || cJSON_GetArraySize(ids) > 0);
    double last = -1.0;
    const cJSON *id;
    cJSON_ArrayForEach(id, ids) {
        CHECK(id->valuedouble > last && id->valuedouble < (double)note_count);
        last = id->valuedouble;
    }
}

static void check_claim(const cJSON *claim, size_t note_count, bool may_be_empty) {
    CHECK(strcmp(test_json_member(claim, "framework")->valuestring, "standard-form-v1") == 0);
    CHECK(position_valid(test_json_member(test_json_member(claim, "location"), "start")) &&
          position_valid(test_json_member(test_json_member(claim, "location"), "end")));
    CHECK(test_rational_of(test_json_member(test_json_member(claim, "span"), "start")) < test_rational_of(test_json_member(test_json_member(claim, "span"), "end")));
    check_note_ids(claim, note_count, may_be_empty);
}

static int rank_of(const char *label) {
    static const char *const ORDER[] = {"Tb", "PD", "D", "Te"};
    for (int i = 0; i < 4; i++) {
        if (strcmp(label, ORDER[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/* The ideas fill the phrase proper: the first begins with it, each follows the one before, the last ends with it. */
static void check_ideas(const cJSON *phrase, size_t note_count) {
    const cJSON *ideas = test_json_member(phrase, "ideas");
    const cJSON *span = test_json_member(phrase, "span");
    const cJSON *measures = test_json_member(phrase, "measures");
    int expected_next = (int)test_json_number(measures, "first");
    int index = 0;
    const cJSON *idea;
    cJSON_ArrayForEach(idea, ideas) {
        check_claim(idea, note_count, true);
        CHECK(test_within(test_json_member(idea, "span"), span));
        CHECK((int)test_json_number(idea, "index") == index++);
        const cJSON *idea_measures = test_json_member(idea, "measures");
        /* the first idea of a phrase that opens on the pickup starts at the pickup, measure 0 */
        CHECK((int)test_json_number(idea_measures, "first") == expected_next ||
              (index == 1 && (int)test_json_number(idea_measures, "first") == 0));
        CHECK(test_json_number(idea_measures, "last") >= test_json_number(idea_measures, "first"));
        expected_next = (int)test_json_number(idea_measures, "last") + 1;
        const char *label = test_json_member(idea, "label")->valuestring;
        CHECK(strcmp(label, "basic-idea") == 0 || strcmp(label, "contrasting-idea") == 0 ||
              strcmp(label, "fragment") == 0 || strcmp(label, "unit") == 0);
        const cJSON *similarity = test_json_member(idea, "similarity");
        CHECK(cJSON_IsNull(similarity) || (similarity->valuedouble >= 0 && similarity->valuedouble <= 1));
        CHECK(index > 1 || cJSON_IsNull(similarity));
    }
    if (cJSON_GetArraySize(ideas) > 0) {
        CHECK(expected_next == (int)test_json_number(measures, "last") + 1);
    }

    const cJSON *subphrases = test_json_member(phrase, "subphrases");
    CHECK(cJSON_GetArraySize(subphrases) == 0 || cJSON_GetArraySize(subphrases) == 2);
    int idea_total = 0;
    index = 0;
    const cJSON *subphrase;
    cJSON_ArrayForEach(subphrase, subphrases) {
        check_claim(subphrase, note_count, true);
        CHECK(test_within(test_json_member(subphrase, "span"), span));
        CHECK((int)test_json_number(subphrase, "index") == index++);
        CHECK((int)test_json_number(subphrase, "first_idea") == idea_total);
        idea_total += (int)test_json_number(subphrase, "idea_count");
        CHECK(strcmp(test_json_member(subphrase, "label")->valuestring, "subphrase") == 0);
    }
    if (cJSON_GetArraySize(subphrases) > 0) {
        CHECK(idea_total == cJSON_GetArraySize(ideas));
    }
}

static void check_phrase_model(const cJSON *document, const cJSON *phrase, int phrase_start_chord,
                               int phrase_end_chord) {
    const cJSON *chords = test_json_member(document, "chords");
    int last_rank = -1;
    const cJSON *segment;
    cJSON_ArrayForEach(segment, test_json_member(phrase, "phrase_model")) {
        int chord_index = (int)test_json_number(segment, "chord");
        const char *label = test_json_member(segment, "label")->valuestring;
        int rank = rank_of(label);
        CHECK(rank >= 0);
        CHECK(rank >= last_rank);
        last_rank = rank;
        CHECK(chord_index >= phrase_start_chord && chord_index < phrase_end_chord);
        const cJSON *chord = cJSON_GetArrayItem(chords, chord_index);
        CHECK(chord != NULL && cJSON_IsString(test_json_member(chord, "phrase_model")) &&
              strcmp(test_json_member(chord, "phrase_model")->valuestring, label) == 0);
    }
}

/* The index of the first chord whose span starts at or after `time`, or the chord count. */
static int first_chord_at(const cJSON *chords, double time) {
    int i = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (test_rational_of(test_json_member(test_json_member(chord, "span"), "start")) >= time - 1e-12) {
            return i;
        }
        i++;
    }
    return i;
}

static void check_phrases(const Piece *piece, const ImportedNotes *notes, const cJSON *document) {
    const cJSON *phrases = test_json_member(document, "phrases");
    const cJSON *cadences = test_json_member(document, "cadences");
    const cJSON *chords = test_json_member(document, "chords");
    int count = cJSON_GetArraySize(phrases);
    CHECK(count > 0);
    if (piece->phrases >= 0) {
        CHECK(count == piece->phrases);
    }
    int idea_count = 0;
    const cJSON *counted;
    cJSON_ArrayForEach(counted, phrases) {
        idea_count += cJSON_GetArraySize(test_json_member(counted, "ideas"));
    }
    printf("%s: %d phrases, %d ideas\n", piece->name, count, idea_count);

    double previous_end = -1.0;
    int previous_last_measure = 0;
    int index = 0;
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, phrases) {
        const cJSON *span = test_json_member(phrase, "span");
        const cJSON *measures = test_json_member(phrase, "measures");
        check_claim(phrase, notes->count, false);
        CHECK((int)test_json_number(phrase, "index") == index);
        CHECK(strcmp(test_json_member(phrase, "label")->valuestring, "phrase") == 0);

        /* tiling */
        double start = test_rational_of(test_json_member(span, "start"));
        double end = test_rational_of(test_json_member(span, "end"));
        if (index == 0) {
            CHECK(start <= test_rational_of(test_json_member(test_json_member(cJSON_GetArrayItem(chords, 0), "span"), "start")) + 1e-12);
        } else {
            /* the next phrase begins at the measure after the last of the one before; that phrase's span ends at its
               cadence's arrival chord (snapped to the beat), so the spans may leave the rest of that measure out */
            CHECK(start >= previous_end - 1e-12);
            CHECK((int)test_json_number(measures, "first") == previous_last_measure + 1);
        }
        previous_end = end;
        previous_last_measure = (int)test_json_number(measures, "last") + (int)test_json_number(phrase, "extension_measures");
        CHECK(test_json_number(measures, "last") >= test_json_number(measures, "first"));
        CHECK(test_json_number(phrase, "length") > 0);

        /* the cadence ends the phrase */
        const cJSON *cadence_index = test_json_member(phrase, "cadence");
        const char *signal = test_json_member(phrase, "signal")->valuestring;
        if (cJSON_IsNumber(cadence_index)) {
            CHECK(strcmp(signal, "cadence") == 0);
            const cJSON *cadence = cJSON_GetArrayItem(cadences, (int)cadence_index->valuedouble);
            CHECK(cadence != NULL);
            if (cadence) {
                const cJSON *arrival = test_json_member(cadence, "arrival");
                CHECK(test_rational_of(test_json_member(test_json_member(arrival, "span"), "start")) >= start - 1e-12);
                CHECK(test_rational_of(test_json_member(test_json_member(arrival, "span"), "end")) <= end + 1e-12);
                int arrival_measure = (int)test_json_number(test_json_member(test_json_member(arrival, "location"), "start"), "measure");
                CHECK(arrival_measure >= (int)test_json_number(measures, "first") && arrival_measure <= (int)test_json_number(measures, "last"));
                /* the phrase proper ends where the arrival chord does, snapped to the next beat: under a measure later;
                   an extension (a post-cadential extension or a codetta) lies after that */
                double arrival_end = test_rational_of(test_json_member(test_json_member(arrival, "span"), "end"));
                CHECK(end >= arrival_end - 1e-12);
                CHECK(test_json_number(phrase, "extension_measures") > 0 || end - arrival_end < 1.0);
            }
        } else {
            CHECK(cJSON_IsNull(cadence_index));
            CHECK(strcmp(signal, "cadence") != 0);
        }
        const char *role = test_json_member(phrase, "role")->valuestring;
        CHECK(strcmp(role, index == 0 ? "beginning" : (index == count - 1 ? "ending" : "middle")) == 0);

        check_ideas(phrase, notes->count);
        int first_chord = first_chord_at(chords, start);
        int end_chord = first_chord_at(chords, end);
        check_phrase_model(document, phrase, first_chord, end_chord);
        index++;
    }
    /* the last phrase reaches the last chord */
    const cJSON *last_chord = cJSON_GetArrayItem(chords, cJSON_GetArraySize(chords) - 1);
    CHECK(previous_end >= test_rational_of(test_json_member(test_json_member(last_chord, "span"), "start")) - 1e-12);

    /* a labelled chord is inside some phrase */
    const cJSON *chord;
    int chord_index = 0;
    cJSON_ArrayForEach(chord, chords) {
        if (cJSON_IsString(test_json_member(chord, "phrase_model"))) {
            bool listed = false;
            const cJSON *p;
            cJSON_ArrayForEach(p, phrases) {
                const cJSON *segment;
                cJSON_ArrayForEach(segment, test_json_member(p, "phrase_model")) {
                    listed = listed || (int)test_json_number(segment, "chord") == chord_index;
                }
            }
            CHECK(listed);
        }
        chord_index++;
    }
}

static void run_piece(const Piece *piece) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, piece->file);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "%s: cannot analyze %s\n", piece->name, path);
        failures++;
        return;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    if (document) {
        CHECK((size_t)cJSON_GetArraySize(test_json_member(document, "phrases")) == report.phrase_count);
        check_phrases(piece, &notes, document);
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        run_piece(&PIECES[i]);
    }
    if (failures > 0) {
        fprintf(stderr, "test_phrases: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_phrases: ok\n");
    return 0;
}
