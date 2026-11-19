#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/imported_score.h"
#include "io/musicxml_export.h"
#include "io/score_layout.h"
#include "io/score_quantize.h"

/*
 * The score layout (io/score_layout.h, R-621) of the Mozart 545 and Bach 114 exports, built as the Analyzer builds the score it
 * exports (the import's parts, meter map and key signatures, notated times), holds what the Analyzer's text export read back from the
 * written MusicXML before it read the layout: the measures, the time signature and clefs, and the events with their notated values,
 * dots, tuplets, ties, voices, staves and pitches. The expected numbers were measured with that reader (score_text_read of the
 * XML musicxml_export_write_score wrote) before the layout existed; a chord counts as one event, as the reader merged it. The layout is
 * also what the document prints: the XML of the same score is written from it, and the document-level checks (test_musicxml_score
 * and the export goldens) hold the text.
 */

typedef struct {
    const char *file;
    size_t parts, measures, events, pitches, rests, ties, tuplets, dots, clefs, bass, changed, pickup;
    long long pitch_sum;
    size_t types[8]; /* whole, half, quarter, eighth, 16th, 32nd, 64th, 128th */
} Expected;

static const Expected EXPECTED[] = {
    {"Mozart 545.mid", 1, 146, 3830, 2746, 1084, 18, 12, 332, 17, 8, 1, 0, 194294, {8, 218, 805, 372, 2028, 399, 0, 0}},
    {"Bach 114.mid", 1, 24, 590, 453, 139, 23, 0, 42, 2, 1, 1, 0, 30656, {0, 29, 85, 272, 184, 20, 0, 0}},
};

static void check_file(const Expected *want) {
    char path[512];
    snprintf(path, sizeof path, "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, want->file);
    ImportedScore notes;
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) == IMPORTED_SCORE_OK);

    MusicXmlExportPart *parts = calloc(notes.source.part_count ? notes.source.part_count : 1, sizeof *parts);
    MusicXmlExportKeySignature *keys = calloc(notes.key_signature_events.count ? notes.key_signature_events.count : 1, sizeof *keys);
    assert(parts && keys);
    for (size_t i = 0; i < notes.source.part_count; i++) {
        parts[i].id = notes.source.parts[i].id;
        parts[i].name = notes.source.parts[i].name;
        parts[i].staff_count = notes.source.parts[i].staff_count;
        parts[i].midi_program = notes.source.parts[i].midi_program;
    }
    for (size_t i = 0; i < notes.key_signature_events.count; i++) {
        const MidiKeySignatureEvent *event = &notes.key_signature_events.events[i];
        keys[i].sharps_flats = event->sharps_flats;
        keys[i].mode = event->mode_hint == MIDI_MODE_HINT_MAJOR   ? MUSICXML_EXPORT_MODE_MAJOR
                       : event->mode_hint == MIDI_MODE_HINT_MINOR ? MUSICXML_EXPORT_MODE_MINOR
                                                                  : MUSICXML_EXPORT_MODE_UNKNOWN;
        keys[i].position = event->position;
    }
    ScoreQuantizeOptions quantize = score_quantize_options_default();
    MusicXmlExportScore score;
    memset(&score, 0, sizeof score);
    score.events = notes.events;
    score.pitch_values = notes.pitch_values;
    score.count = notes.count;
    score.part_index = notes.source.part_index;
    score.staff = notes.source.staff;
    score.parts = parts;
    score.part_count = notes.source.part_count;
    score.meter_map = notes.meter.meter_map;
    score.key_signatures = keys;
    score.key_signature_count = notes.key_signature_events.count;
    score.quantize = &quantize;

    ScoreLayout layout;
    assert(score_layout_build(&score, &layout) == MUSICXML_EXPORT_OK);
    assert(layout.divisions > 0 && layout.part_count == want->parts && layout.measure_count == want->measures);

    size_t events = 0, pitches = 0, rests = 0, ties = 0, tuplets = 0, dots = 0, clefs = 0, bass = 0, changed = 0, pickup = 0;
    long long pitch_sum = 0;
    size_t types[8] = {0};
    int beats = 0, beat_type = 0;
    for (size_t m = 0; m < layout.measure_count; m++) {
        const ScoreLayoutMeasure *measure = &layout.measures[m];
        assert(measure->number == layout.measures[0].number + (int)m);
        assert(measure->end > measure->start);
        pickup += measure->implicit;
        if (measure->time_written) {
            changed += m == 0 || measure->beats != beats || measure->beat_type != beat_type;
            beats = measure->beats;
            beat_type = measure->beat_type;
        }
        clefs += measure->clef_count;
        for (size_t c = 0; c < measure->clef_count; c++) {
            bass += measure->clefs[c].bass;
        }
        const ScoreLayoutEvent *previous = NULL;
        bool merged_tie = false;
        for (size_t e = 0; e < measure->event_count; e++) {
            const ScoreLayoutEvent *event = &measure->events[e];
            assert(event->start >= measure->start && event->start + event->duration <= measure->end && event->duration > 0);
            if (!event->rest) {
                pitches++;
                pitch_sum += (event->step - 'A') * 13 + event->alter * 5 + event->octave * 7;
                assert(event->xml_id[0] != '\0');
            }
            if (event->chord && previous != NULL && previous->part == event->part && !previous->rest && !event->rest) {
                ties += event->tie_start && !merged_tie;
                merged_tie = merged_tie || event->tie_start;
                continue;
            }
            previous = event;
            merged_tie = event->tie_start;
            events++;
            rests += event->rest;
            ties += event->tie_start;
            tuplets += event->tuplet_actual != 0;
            dots += (size_t)event->dots;
            assert(event->value >= 0 && event->value <= 8);
            if (event->value > 0) {
                types[event->value - 1]++;
            }
            assert(event->staff >= 1 && event->voice >= 1);
        }
    }
    assert(events == want->events && pitches == want->pitches && rests == want->rests && ties == want->ties);
    assert(tuplets == want->tuplets && dots == want->dots && clefs == want->clefs && bass == want->bass);
    assert(changed == want->changed && pickup == want->pickup && pitch_sum == want->pitch_sum);
    for (int i = 0; i < 8; i++) {
        assert(types[i] == want->types[i]);
    }

    /* The document written from the layout is the document of the same score. */
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK && xml != NULL);
    assert(strstr(xml, "<divisions>") != NULL);
    free(xml);

    score_layout_destroy(&layout);
    assert(layout.measures == NULL && layout.prepared == NULL);
    score_layout_destroy(&layout);
    score_layout_destroy(NULL);
    free(parts);
    free(keys);
    imported_score_destroy(&notes);
}

int main(void) {
    ScoreLayout empty;
    assert(score_layout_build(NULL, &empty) == MUSICXML_EXPORT_ERR_NULL_ARG);
    MusicXmlExportScore none;
    memset(&none, 0, sizeof none);
    assert(score_layout_build(&none, NULL) == MUSICXML_EXPORT_ERR_NULL_ARG);
    for (size_t i = 0; i < sizeof EXPECTED / sizeof EXPECTED[0]; i++) {
        check_file(&EXPECTED[i]);
    }
    puts("test_score_layout: ok");
    return 0;
}
