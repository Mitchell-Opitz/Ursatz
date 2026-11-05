#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * Analysis document test: the test_analysis_pipeline fixture (C-E-G,
 * then D, F, A, B one at a time, then C-E-G-D at tick 480; division 96,
 * no key signature) runs through the pipeline, the rendered document is
 * parsed with cJSON, and its sections, claim shapes and values are
 * checked against the existing report expectations.
 */

static void append_bytes(unsigned char *buf, size_t *pos, const void *bytes, size_t len) {
    memcpy(buf + *pos, bytes, len);
    *pos += len;
}

static long number(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsNumber(item));
    return (long)item->valuedouble;
}

static const char *string(const cJSON *object, const char *key) {
    const cJSON *item = test_json_require(object, key);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static void check_rational(const cJSON *rational, long num, long den) {
    assert(cJSON_IsObject(rational));
    assert(number(rational, "num") == num);
    assert(number(rational, "den") == den);
}

/* Required claim fields and their types; returns the note_ids count. */
static int check_claim(const cJSON *claim) {
    assert(cJSON_IsObject(claim));
    assert(strlen(string(claim, "framework")) > 0);
    const cJSON *span = test_json_require(claim, "span");
    assert(cJSON_IsObject(span));
    assert(cJSON_IsObject(test_json_require(span, "start")));
    assert(cJSON_IsObject(test_json_require(span, "end")));
    const cJSON *note_ids = test_json_require(claim, "note_ids");
    assert(cJSON_IsArray(note_ids));
    const cJSON *id;
    cJSON_ArrayForEach(id, note_ids) {
        assert(cJSON_IsNumber(id));
        assert(id->valuedouble >= 0);
    }
    string(claim, "label");
    const cJSON *uncertainty = test_json_require(claim, "uncertainty");
    assert(cJSON_IsNull(uncertainty) || cJSON_IsNumber(uncertainty));
    return cJSON_GetArraySize(note_ids);
}

int main(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
        0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
        0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
        0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
        0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
        0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
        0x00, 0x90, 0x3E, 0x64, /* delta0  note-on  D4 (62) */
        0x60, 0x80, 0x3E, 0x40, /* delta96 note-off D4 */
        0x00, 0x90, 0x41, 0x64, /* delta0  note-on  F4 (65) */
        0x60, 0x80, 0x41, 0x40, /* delta96 note-off F4 */
        0x00, 0x90, 0x45, 0x64, /* delta0  note-on  A4 (69) */
        0x60, 0x80, 0x45, 0x40, /* delta96 note-off A4 */
        0x00, 0x90, 0x47, 0x64, /* delta0  note-on  B4 (71) */
        0x60, 0x80, 0x47, 0x40, /* delta96 note-off B4 */
        0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
        0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
        0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
        0x00, 0x90, 0x4A, 0x64, /* delta0  note-on  D5 (74) */
        0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
        0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
        0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
        0x00, 0x80, 0x4A, 0x40, /* delta0  note-off D5 */
        0x00, 0xFF, 0x2F, 0x00  /* end of track */
    };

    unsigned char buf[256];
    size_t size = test_smf_type0(buf, 96, track, sizeof(track));

    char path[512];
    test_temp_path(path, sizeof(path), "document_fixture.mid");
    char expected_document_path[512];
    test_temp_path(expected_document_path, sizeof(expected_document_path), "document_fixture.analysis.json");
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(buf, 1, size, file) == size);
    fclose(file);

    ImportedNotes notes;
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));

    char *json = analysis_document_render(path, &notes, &report);
    assert(json != NULL);
    assert(strncmp(json, "{\n  \"schema_version\": 9,\n", 25) == 0);
    assert(json[strlen(json) - 1] == '\n');

    char *again = analysis_document_render(path, &notes, &report);
    assert(again != NULL);
    assert(strcmp(json, again) == 0);
    free(again);

    char *document_path = derive_analysis_document_path(path);
    assert(strcmp(document_path, expected_document_path) == 0);
    free(document_path);

    cJSON *document = cJSON_Parse(json);
    assert(document != NULL);
    assert(number(document, "schema_version") == URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION);
    assert(URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION == 9);

    const cJSON *metadata = test_json_require(document, "metadata");
    assert(cJSON_IsObject(metadata));
    assert(strcmp(string(metadata, "source_file"), strrchr(path, '/') + 1) == 0);
    assert(strlen(string(metadata, "analyzer_version")) > 0);
    assert(strlen(string(metadata, "ursatz_commit")) > 0);
    assert(number(metadata, "note_count") == 11);
    assert(number(metadata, "ticks_per_quarter") == 96);
    assert(cJSON_IsTrue(test_json_require(metadata, "key_found")));

    /* The fixture has no time signature event: the library's 4/4 is
       reported, marked inferred. */
    const cJSON *meter = test_json_require(document, "meter");
    assert(cJSON_IsObject(meter));
    assert(strcmp(string(meter, "source"), "inferred") == 0);
    assert(strcmp(string(meter, "framework"), "ursatz-default-meter") == 0);
    assert(strcmp(string(meter, "label"), "4/4") == 0);
    assert(number(test_json_require(meter, "time_signature"), "numerator") == 4);
    assert(number(test_json_require(meter, "time_signature"), "denominator") == 4);
    assert(cJSON_IsNull(test_json_require(meter, "pickup")));
    assert(cJSON_GetArraySize(test_json_require(meter, "changes")) == 0);
    /* The form is the tree of the phrases' sections, or null when the library found no phrases. */
    assert(cJSON_GetArraySize(test_json_require(document, "phrases")) == 0 ? cJSON_IsNull(test_json_require(document, "form"))
                                                                : cJSON_IsObject(test_json_require(document, "form")));
    assert(cJSON_IsObject(test_json_require(document, "harmonic_rhythm")));
    assert(cJSON_IsObject(test_json_require(document, "markings")));
    assert(cJSON_IsNull(test_json_require(document, "counterpoint_check")));
    static const char *const EMPTY_SECTIONS[] = {"cadences", "motives", "schemas", "progressions"};
    for (size_t i = 0; i < sizeof(EMPTY_SECTIONS) / sizeof(EMPTY_SECTIONS[0]); i++) {
        const cJSON *section = test_json_require(document, EMPTY_SECTIONS[i]);
        assert(cJSON_IsArray(section));
        assert(cJSON_GetArraySize(section) == 0);
    }

    /* Texture: the passages in time order, each with its measures and texture, then the changes between them. */
    const cJSON *texture = test_json_require(document, "texture");
    assert(cJSON_IsArray(texture));
    int passages = 0;
    int changes = 0;
    for (int i = 0; i < cJSON_GetArraySize(texture); i++) {
        const cJSON *entry = cJSON_GetArrayItem(texture, i);
        check_claim(entry);
        if (strcmp(string(entry, "label"), "passage") == 0) {
            assert(changes == 0);
            passages++;
            assert(number(test_json_require(entry, "measures"), "last") >= number(test_json_require(entry, "measures"), "first"));
            assert(number(test_json_require(entry, "features"), "voices_max") >= 1);
        } else {
            assert(strcmp(string(entry, "label"), "change") == 0);
            changes++;
            assert(strcmp(string(entry, "from_texture"), string(entry, "to_texture")) != 0);
        }
    }
    assert(passages == 0 ? changes == 0 : changes == passages - 1);

    /* Voice leading: a list of claims; the motion claim counts every pair of voices and its totals add up. */
    const cJSON *voice_leading = test_json_require(document, "voice_leading");
    assert(cJSON_IsArray(voice_leading));
    for (int i = 0; i < cJSON_GetArraySize(voice_leading); i++) {
        const cJSON *entry = cJSON_GetArrayItem(voice_leading, i);
        check_claim(entry);
        if (strcmp(string(entry, "label"), "violation") == 0) {
            assert(strncmp(string(entry, "rule"), "parallel-", 9) == 0 || strcmp(string(entry, "rule"), "direct-perfects") == 0);
            assert(number(entry, "to_chord") == number(entry, "from_chord") + 1);
            assert(number(entry, "upper_voice") < number(entry, "lower_voice"));
            continue;
        }
        if (strcmp(string(entry, "label"), "resolution") == 0) {
            assert(number(entry, "to_chord") > number(entry, "from_chord"));
            continue;
        }
        if (strcmp(string(entry, "label"), "doubling") == 0) {
            assert(number(entry, "voices") >= 3);
            assert(cJSON_GetArraySize(test_json_require(entry, "gaps")) == (int)number(entry, "voices") - 1);
            continue;
        }
        assert(strcmp(string(entry, "label"), "motions") == 0);
        const cJSON *totals = test_json_require(entry, "totals");
        double sum = number(totals, "contrary") + number(totals, "similar") + number(totals, "parallel") +
                     number(totals, "oblique") + number(totals, "static");
        assert(sum == number(entry, "motion_count"));
        assert(cJSON_GetArraySize(test_json_require(entry, "pairs")) ==
               (int)(number(entry, "voices") * (number(entry, "voices") - 1) / 2));
    }

    /* Melody: one claim per phrase with melody notes, in phrase order, each with its range, climax and contour. */
    const cJSON *melody = test_json_require(document, "melody");
    assert(cJSON_IsArray(melody));
    for (int i = 0; i < cJSON_GetArraySize(melody); i++) {
        const cJSON *entry = cJSON_GetArrayItem(melody, i);
        check_claim(entry);
        assert(strcmp(string(entry, "label"), "melody") == 0);
        assert(number(entry, "index") < cJSON_GetArraySize(test_json_require(document, "phrases")));
        assert(number(entry, "note_count") == cJSON_GetArraySize(test_json_require(entry, "note_ids")));
        assert(number(test_json_require(test_json_require(entry, "range"), "highest"), "pitch") >=
               number(test_json_require(test_json_require(entry, "range"), "lowest"), "pitch"));
        assert(strcmp(string(test_json_require(entry, "climax"), "label"), "climax") == 0);
        assert(cJSON_GetArraySize(test_json_require(test_json_require(entry, "contour"), "directions")) < number(entry, "note_count"));
    }

    /* Rhythm: one flat list of labelled claims, the densities first and then the syncopations, hemiolas and hypermeters. The
       density claims come phrase by phrase, all notes first and then each line that has a note in it, and the lines' notes
       add up to the phrase's. */
    const cJSON *rhythm = test_json_require(document, "rhythm");
    assert(cJSON_IsArray(rhythm));
    int phrase_count = cJSON_GetArraySize(test_json_require(document, "phrases"));
    double all_notes = 0;
    double line_notes = 0;
    int all_seen = 0;
    int hypermeters = 0;
    int syncopations_seen = 0;
    for (int i = 0; i < cJSON_GetArraySize(rhythm); i++) {
        const cJSON *entry = cJSON_GetArrayItem(rhythm, i);
        const char *label = string(entry, "label");
        check_claim(entry);
        if (strcmp(label, "meter-fit") == 0) {
            /* the meter check, over no notes, first in the section */
            assert(i == 0);
            assert(cJSON_GetArraySize(test_json_require(entry, "note_ids")) == 0);
            assert(cJSON_IsNull(test_json_require(entry, "alternative")) || cJSON_IsObject(test_json_require(entry, "alternative")));
        } else if (strcmp(label, "density") == 0) {
            assert(syncopations_seen == 0 && hypermeters == 0);
            assert(number(entry, "phrase") < phrase_count);
            assert(number(entry, "attacks") <= number(entry, "notes"));
            assert(number(entry, "notes") == cJSON_GetArraySize(test_json_require(entry, "note_ids")));
            assert(number(entry, "on_beat_share") >= 0 && number(entry, "on_beat_share") <= 1);
            assert(number(entry, "on_beat_share") + number(entry, "off_beat_share") <= 1.000001);
            if (strcmp(string(entry, "line"), "all") == 0) {
                assert(all_seen == 0 || line_notes == all_notes);
                all_notes = number(entry, "notes");
                line_notes = 0;
                all_seen++;
            } else {
                line_notes += number(entry, "notes");
            }
        } else if (strcmp(label, "syncopation") == 0) {
            syncopations_seen++;
            assert(cJSON_GetArraySize(test_json_require(entry, "note_ids")) == 1);
            assert(number(entry, "strength") >= 1 && number(entry, "strength") <= 4);
            assert(number(entry, "beat") >= 1);
        } else if (strcmp(label, "hemiola") == 0) {
            assert(number(test_json_require(entry, "evidence"), "agreeing") >= 2);
            assert(number(test_json_require(entry, "evidence"), "agreeing") <= number(test_json_require(entry, "evidence"), "available"));
        } else if (strcmp(label, "tuplet") == 0) {
            /* a run of two or more attacks; the library's BORROWED rule: a triplet in a simple meter, a duplet in a
               compound one */
            assert(number(entry, "attacks") >= 2);
            assert(cJSON_IsNull(test_json_require(entry, "uncertainty")));
            const char *name = string(entry, "name");
            const char *meter = string(entry, "meter");
            bool borrowed = (strcmp(name, "triplet") == 0 && strcmp(meter, "simple") == 0) ||
                            (strcmp(name, "duplet") == 0 && strcmp(meter, "compound") == 0);
            assert(cJSON_IsTrue(test_json_require(entry, "borrowed")) == borrowed);
        } else {
            assert(strcmp(label, "hypermeter") == 0);
            hypermeters++;
            const cJSON *measures = test_json_require(entry, "hypermeasures");
            assert(cJSON_GetArraySize(measures) > 0);
            assert(cJSON_GetArraySize(measures) == cJSON_GetArraySize(test_json_require(entry, "hyperdownbeats")));
            /* the hypermeasures follow one another from the phrase's first measure (the library fills the last one
               to its length, so it can run past the phrase's measures) */
            double next = number(test_json_require(entry, "measures"), "first");
            for (int h = 0; h < cJSON_GetArraySize(measures); h++) {
                const cJSON *hypermeasure = cJSON_GetArrayItem(measures, h);
                assert(number(test_json_require(hypermeasure, "measures"), "first") == next);
                next = number(test_json_require(hypermeasure, "measures"), "last") + 1;
            }
            assert(next >= number(test_json_require(entry, "measures"), "last") + 1);
        }
    }
    assert(line_notes == all_notes);
    assert(all_seen == (phrase_count > 0 && cJSON_GetArraySize(rhythm) > 0 ? phrase_count : 0));
    assert(hypermeters <= phrase_count);

    /* Phrases: one claim each in time order, with measures, length, cadence (an index or null) and signal. */
    const cJSON *phrases = test_json_require(document, "phrases");
    assert(cJSON_IsArray(phrases));
    for (int i = 0; i < cJSON_GetArraySize(phrases); i++) {
        const cJSON *phrase = cJSON_GetArrayItem(phrases, i);
        check_claim(phrase);
        assert(number(phrase, "index") == i);
        assert(number(test_json_require(phrase, "measures"), "first") <= number(test_json_require(phrase, "measures"), "last"));
        assert(number(phrase, "length") > 0);
        assert(cJSON_IsNull(test_json_require(phrase, "cadence")) || cJSON_IsNumber(test_json_require(phrase, "cadence")));
        assert(strlen(string(phrase, "signal")) > 0);
        assert(strcmp(string(phrase, "role"), i == 0 ? "beginning" : (i + 1 == cJSON_GetArraySize(phrases) ? "ending" : "middle")) == 0);
    }

    /* Keys: the two-measure piece is one key region, running barline to barline. */
    const cJSON *keys = test_json_require(document, "keys");
    assert(cJSON_IsObject(keys));
    const cJSON *regions = test_json_require(keys, "regions");
    assert(cJSON_IsArray(regions));
    assert(cJSON_GetArraySize(regions) == 1);
    const cJSON *key = cJSON_GetArrayItem(regions, 0);
    assert(check_claim(key) == 11);
    assert(strcmp(string(key, "label"), "C major") == 0);
    assert(strcmp(string(key, "tonic"), "C") == 0);
    assert(strcmp(string(key, "mode"), "major") == 0);
    assert(strcmp(string(key, "source"), "profile") == 0);
    assert(number(key, "index") == 0);
    assert(number(test_json_require(key, "measures"), "first") == 1);
    assert(number(test_json_require(key, "measures"), "last") == 2);
    check_rational(test_json_require(test_json_require(key, "span"), "start"), 0, 1);
    check_rational(test_json_require(test_json_require(key, "span"), "end"), 2, 1);
    const cJSON *key_location = test_json_require(key, "location");
    assert(number(test_json_require(key_location, "start"), "measure") == 1);
    assert(number(test_json_require(key_location, "start"), "beat") == 1);
    check_rational(test_json_require(test_json_require(key_location, "start"), "subdivision"), 0, 1);
    assert(number(test_json_require(key_location, "end"), "measure") == 3);
    assert(number(test_json_require(key_location, "end"), "beat") == 1);

    /* The home key is the only region, which is also the opening and closing one. */
    const cJSON *home = test_json_require(keys, "home_key");
    assert(strcmp(string(home, "rule"), "longest_total_duration") == 0);
    assert(cJSON_GetArraySize(test_json_require(home, "region_indices")) == 1);
    assert(cJSON_GetArrayItem(test_json_require(home, "region_indices"), 0)->valuedouble == 0);
    assert(strcmp(string(home, "label"), "C major") == 0);
    check_rational(test_json_require(home, "duration"), 2, 1);
    assert(number(keys, "opening_region") == 0);
    assert(number(keys, "closing_region") == 0);

    /* The file has no key signature, so none is reported and nothing disagrees. */
    assert(cJSON_GetArraySize(test_json_require(metadata, "key_signatures")) == 0);
    assert(cJSON_GetArraySize(test_json_require(document, "notes")) == 0);

    /* Chords: one claim per report entry, matching test_analysis_pipeline. */
    const cJSON *chords = test_json_require(document, "chords");
    assert(cJSON_IsArray(chords));
    assert(cJSON_GetArraySize(chords) == 6);
    static const char *const CHORD_LABELS[] = {"I", "ii", "IV", "vi", "vii\xC2\xB0", "I"};
    static const int NOTE_COUNTS[] = {3, 1, 1, 1, 1, 4};
    for (int i = 0; i < 6; i++) {
        const cJSON *chord = cJSON_GetArrayItem(chords, i);
        int note_id_count = check_claim(chord);
        assert(strcmp(string(chord, "label"), CHORD_LABELS[i]) == 0);
        assert(strcmp(string(chord, "key_label"), "C major") == 0);
        assert(strcmp(string(chord, "framework"), string(key, "framework")) == 0);
        assert(note_id_count == NOTE_COUNTS[i]);
        if (strcmp(CHORD_LABELS[i], "no match") != 0) {
            assert(note_id_count > 0);
        }
    }
    const cJSON *first_span = test_json_require(cJSON_GetArrayItem(chords, 0), "span");
    check_rational(test_json_require(first_span, "start"), 0, 1);
    check_rational(test_json_require(first_span, "end"), 1, 4);
    const cJSON *last_span = test_json_require(cJSON_GetArrayItem(chords, 5), "span");
    check_rational(test_json_require(last_span, "start"), 5, 4);
    check_rational(test_json_require(last_span, "end"), 3, 2);

    cJSON_Delete(document);
    free(json);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    remove(path);

    printf("test_analysis_document: ok\n");
    return 0;
}
