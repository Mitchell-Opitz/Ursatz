#include <ursatz_analyzer/score_export.h>

#include <stdlib.h>
#include <string.h>

#include "export/musicxml_export.h"
#include "export/score_quantize.h"

static MusicXmlExportMode export_mode_from_hint(MidiModeHint hint) {
    switch (hint) {
        case MIDI_MODE_HINT_MAJOR:
            return MUSICXML_EXPORT_MODE_MAJOR;
        case MIDI_MODE_HINT_MINOR:
            return MUSICXML_EXPORT_MODE_MINOR;
        case MIDI_MODE_HINT_UNKNOWN:
        default:
            return MUSICXML_EXPORT_MODE_UNKNOWN;
    }
}

static AnalyzerStatus status_of(MusicXmlExportResult result) {
    switch (result) {
        case MUSICXML_EXPORT_OK:
            return ANALYZER_OK;
        case MUSICXML_EXPORT_ERR_NULL_ARG:
            return ANALYZER_ERR_ARGUMENT;
        case MUSICXML_EXPORT_ERR_ALLOC:
            return ANALYZER_ERR_ALLOC;
        default:
            return ANALYZER_ERR_LIBRARY;
    }
}

AnalyzerStatus analyzer_score_export_musicxml(const ImportedNotes *notes, char **xml) {
    if (!xml) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *xml = NULL;
    if (!notes || (notes->count > 0 && (!notes->events || !notes->pitch_values))) {
        return ANALYZER_ERR_ARGUMENT;
    }

    /* The parts, with their programs, and the key signatures of part 0, as the importer read them. */
    size_t part_count = notes->source.part_count;
    size_t key_count = notes->key_signature_events.count;
    MusicXmlExportPart *parts = part_count > 0 ? calloc(part_count, sizeof(MusicXmlExportPart)) : NULL;
    MusicXmlExportKeySignature *keys = key_count > 0 ? calloc(key_count, sizeof(MusicXmlExportKeySignature)) : NULL;
    if ((part_count > 0 && !parts) || (key_count > 0 && !keys)) {
        free(parts);
        free(keys);
        return ANALYZER_ERR_ALLOC;
    }
    for (size_t i = 0; i < part_count; i++) {
        parts[i].id = notes->source.parts[i].id;
        parts[i].name = notes->source.parts[i].name;
        parts[i].staff_count = notes->source.parts[i].staff_count;
        parts[i].midi_program = notes->source.parts[i].midi_program;
    }
    for (size_t i = 0; i < key_count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        keys[i].sharps_flats = event->sharps_flats;
        keys[i].mode = export_mode_from_hint(event->mode_hint);
        keys[i].position = event->position;
        keys[i].part_index = 0;
    }

    const ScoreQuantizeOptions quantize = score_quantize_options_default();
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = notes->events;
    score.pitch_values = notes->pitch_values;
    score.count = notes->count;
    score.part_index = notes->source.part_index;
    score.staff = notes->source.staff;
    score.parts = parts;
    score.part_count = part_count;
    score.meter_map = notes->meter.meter_map;
    score.key_signatures = keys;
    score.key_signature_count = key_count;
    score.quantize = &quantize;

    char *text = NULL;
    AnalyzerStatus status = status_of(musicxml_export_write_score(&score, &text));
    free(parts);
    free(keys);
    if (status != ANALYZER_OK) {
        return status;
    }
    *xml = text;
    return ANALYZER_OK;
}
