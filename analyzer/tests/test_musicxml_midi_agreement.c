#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * The five reference pieces (Ursatz framework-v1 corpus) are analyzed from
 * their MIDI files and from their MusicXML versions (tests/musicxml, see its
 * README for provenance), and the two analysis documents are compared.
 *
 * Exact agreement is required on the key regions (tonic, mode, measure
 * span), the measure count, the meter and the pickup. The chords (root pitch
 * class, quality, inversion) and the outer-voice scale degrees are compared
 * segment by segment and must agree at least as often as the thresholds
 * below. The thresholds are a tolerance for the two inputs differing in what
 * they know -- MIDI has no pitch spelling and its timing is a performance's,
 * the score's is exact -- not a target to be loosened: the measured
 * agreement is printed for every piece.
 */

#define CHORD_AGREEMENT_MIN 0.90
#define OUTER_DEGREE_AGREEMENT_MIN 0.95

static const struct {
    const char *midi;
    const char *musicxml;
} PIECES[] = {
    {"Bach 114.mid", "bach-114.musicxml"},       {"Mozart 545.mid", "mozart-545.musicxml"},
    {"Clementi 36.mid", "clementi-36.musicxml"}, {"Field 1.mid", "field-1.musicxml"},
    {"Field 5.mid", "field-5.musicxml"},
};

static const char *string(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static bool same(const cJSON *a, const cJSON *b) {
    return cJSON_Compare(a, b, true);
}

/* The pitch class of a name such as "Db", "C#" or "B##", so enharmonic spellings of one chord root compare equal. */
static int pitch_class_of_name(const char *name) {
    static const int LETTER[7] = {9, 11, 0, 2, 4, 5, 7};
    assert(name[0] >= 'A' && name[0] <= 'G');
    int pitch_class = LETTER[name[0] - 'A'];
    for (const char *c = name + 1; *c; c++) {
        pitch_class += *c == '#' ? 1 : -1;
    }
    return ((pitch_class % 12) + 12) % 12;
}

static bool optional_string_equal(const cJSON *a, const cJSON *b) {
    if (cJSON_IsNull(a) || cJSON_IsNull(b)) {
        return cJSON_IsNull(a) && cJSON_IsNull(b);
    }
    return strcmp(a->valuestring, b->valuestring) == 0;
}

static void compare_meter(const cJSON *midi, const cJSON *xml, const char *piece) {
    const cJSON *midi_meter = test_json_require(midi, "meter");
    const cJSON *xml_meter = test_json_require(xml, "meter");
    assert(same(test_json_require(midi_meter, "time_signature"), test_json_require(xml_meter, "time_signature")));
    assert(same(test_json_require(midi_meter, "measure_count"), test_json_require(xml_meter, "measure_count")));
    assert(same(test_json_require(midi_meter, "last_measure"), test_json_require(xml_meter, "last_measure")));
    assert(cJSON_IsObject(test_json_require(midi_meter, "pickup")) == cJSON_IsObject(test_json_require(xml_meter, "pickup")));
    assert(cJSON_GetArraySize(test_json_require(midi_meter, "changes")) == cJSON_GetArraySize(test_json_require(xml_meter, "changes")));

    /* Notated for the score; embedded (or inferred, when the file has no time signature) for MIDI. */
    assert(strcmp(string(xml_meter, "source"), "notated") == 0);
    const char *midi_source = string(midi_meter, "source");
    assert(strcmp(midi_source, "file") == 0 || strcmp(midi_source, "inferred") == 0);
    const cJSON *midi_input = test_json_require(test_json_require(midi, "metadata"), "input");
    const cJSON *xml_input = test_json_require(test_json_require(xml, "metadata"), "input");
    assert(strcmp(string(xml_input, "meter"), "notated") == 0);
    assert(strcmp(string(midi_input, "meter"), strcmp(midi_source, "file") == 0 ? "embedded" : "inferred") == 0);
    printf("%s: %ld measures, meter agrees\n", piece, (long)test_json_require(xml_meter, "measure_count")->valuedouble);
}

static void compare_keys(const cJSON *midi, const cJSON *xml) {
    const cJSON *midi_regions = test_json_require(test_json_require(midi, "keys"), "regions");
    const cJSON *xml_regions = test_json_require(test_json_require(xml, "keys"), "regions");
    assert(cJSON_GetArraySize(midi_regions) == cJSON_GetArraySize(xml_regions));
    assert(cJSON_GetArraySize(midi_regions) > 0);
    for (int i = 0; i < cJSON_GetArraySize(midi_regions); i++) {
        const cJSON *a = cJSON_GetArrayItem(midi_regions, i);
        const cJSON *b = cJSON_GetArrayItem(xml_regions, i);
        assert(strcmp(string(a, "tonic"), string(b, "tonic")) == 0);
        assert(strcmp(string(a, "mode"), string(b, "mode")) == 0);
        assert(same(test_json_require(a, "measures"), test_json_require(b, "measures")));
    }
    const cJSON *midi_input = test_json_require(test_json_require(midi, "metadata"), "input");
    const cJSON *xml_input = test_json_require(test_json_require(xml, "metadata"), "input");
    bool midi_notates_key = cJSON_GetArraySize(test_json_require(test_json_require(midi, "metadata"), "key_signatures")) > 0;
    assert(strcmp(string(midi_input, "key"), midi_notates_key ? "embedded" : "inferred") == 0);
    assert(strcmp(string(xml_input, "key"), midi_notates_key ? "notated" : "inferred") == 0);
}

static const cJSON *outer_degree(const cJSON *chord, const char *line) {
    return test_json_require(test_json_require(test_json_require(chord, "outer"), line), "degree");
}

static void compare_chords(const cJSON *midi, const cJSON *xml, const char *piece) {
    const cJSON *midi_chords = test_json_require(midi, "chords");
    const cJSON *xml_chords = test_json_require(xml, "chords");
    int total = cJSON_GetArraySize(midi_chords);
    assert(total > 0);

    /* Both inputs share the meter, so the segments normally line up one for one; match by start in case they do not. */
    int chord_matches = 0;
    int degree_matches = 0;
    int degree_total = 0;
    for (int i = 0; i < total; i++) {
        const cJSON *a = cJSON_GetArrayItem(midi_chords, i);
        const cJSON *start = test_json_require(test_json_require(a, "span"), "start");
        const cJSON *b = NULL;
        const cJSON *candidate;
        cJSON_ArrayForEach(candidate, xml_chords) {
            if (same(test_json_require(test_json_require(candidate, "span"), "start"), start)) {
                b = candidate;
                break;
            }
        }
        if (!b) {
            degree_total += 2;
            continue;
        }
        const cJSON *a_root = test_json_require(a, "root");
        const cJSON *b_root = test_json_require(b, "root");
        bool roots = cJSON_IsNull(a_root) || cJSON_IsNull(b_root)
                         ? cJSON_IsNull(a_root) && cJSON_IsNull(b_root)
                         : pitch_class_of_name(a_root->valuestring) == pitch_class_of_name(b_root->valuestring);
        if (roots && optional_string_equal(test_json_require(a, "quality"), test_json_require(b, "quality")) &&
            same(test_json_require(a, "inversion"), test_json_require(b, "inversion"))) {
            chord_matches++;
        }
        static const char *const LINES[2] = {"soprano", "bass"};
        for (int line = 0; line < 2; line++) {
            degree_total++;
            degree_matches += optional_string_equal(outer_degree(a, LINES[line]), outer_degree(b, LINES[line]));
        }
    }
    double chord_agreement = (double)chord_matches / total;
    double degree_agreement = (double)degree_matches / degree_total;
    printf("%s: chords %d/%d agree (%.1f%%; %d MIDI, %d MusicXML segments), outer-voice degrees %d/%d agree "
           "(%.1f%%)\n",
           piece, chord_matches, total, 100.0 * chord_agreement, total, cJSON_GetArraySize(xml_chords),
           degree_matches, degree_total, 100.0 * degree_agreement);
    if (chord_agreement < CHORD_AGREEMENT_MIN || degree_agreement < OUTER_DEGREE_AGREEMENT_MIN) {
        fprintf(stderr, "%s: agreement is below the tolerance (chords %.2f, outer degrees %.2f)\n", piece,
                CHORD_AGREEMENT_MIN, OUTER_DEGREE_AGREEMENT_MIN);
        exit(1);
    }
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char midi_path[512];
        char xml_path[512];
        snprintf(midi_path, sizeof(midi_path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, PIECES[i].midi);
        snprintf(xml_path, sizeof(xml_path), "%s/%s", URSATZ_MUSICXML_DIR, PIECES[i].musicxml);
        cJSON *midi = test_analyze_midi(midi_path);
        cJSON *xml = test_analyze_midi(xml_path);

        const cJSON *xml_input = test_json_require(test_json_require(xml, "metadata"), "input");
        const cJSON *midi_input = test_json_require(test_json_require(midi, "metadata"), "input");
        assert(strcmp(string(xml_input, "format"), "musicxml") == 0);
        assert(strcmp(string(xml_input, "source_type"), "score") == 0);
        assert(strcmp(string(midi_input, "format"), "midi") == 0);
        assert(strcmp(string(midi_input, "source_type"), "performance") == 0);
        assert(same(test_json_require(test_json_require(midi, "metadata"), "note_count"), test_json_require(test_json_require(xml, "metadata"), "note_count")));

        compare_meter(midi, xml, PIECES[i].musicxml);
        compare_keys(midi, xml);
        compare_chords(midi, xml, PIECES[i].musicxml);
        cJSON_Delete(midi);
        cJSON_Delete(xml);
    }
    return 0;
}
