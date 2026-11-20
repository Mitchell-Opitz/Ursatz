#include <ursatz_analyzer/analyzer_api.h>

#include "util/misc_util.h"
#include "export/score_export_internal.h"

#include <stdlib.h>
#include <string.h>

#include "io/musicxml_export.h"
#include "io/score_quantize.h"
#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "model/time/meter_map.h"

/* The longest base id the export writes (its buffer is 64 bytes). */
#define XML_ID_EXPORT_LIMIT 63

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

static UrsatzStatus status_of(MusicXmlExportResult result) {
    switch (result) {
        case MUSICXML_EXPORT_OK:
            return URSATZ_OK;
        case MUSICXML_EXPORT_ERR_NULL_ARG:
            return URSATZ_ERR_REQUEST;
        case MUSICXML_EXPORT_ERR_ALLOC:
            return URSATZ_ERR_ALLOC;
        default:
            return URSATZ_ERR_INTERNAL;
    }
}

/* The export score of an import: its parts and key signatures are the caller's to free (*parts and *keys). */
static UrsatzStatus build_export_score(const ImportedScore *notes, const AnalyzerScoreMetadata *metadata,
                                       const ScoreQuantizeOptions *quantize, MusicXmlExportScore *score,
                                         MusicXmlExportPart **parts_out, MusicXmlExportKeySignature **keys_out) {
    /* The parts, with their programs, and the key signatures of part 0, as the importer read them. */
    size_t part_count = notes->source.part_count;
    size_t key_count = notes->key_signature_events.count;
    MusicXmlExportPart *parts = part_count > 0 ? calloc(part_count, sizeof(MusicXmlExportPart)) : NULL;
    MusicXmlExportKeySignature *keys = key_count > 0 ? calloc(key_count, sizeof(MusicXmlExportKeySignature)) : NULL;
    if ((part_count > 0 && !parts) || (key_count > 0 && !keys)) {
        free(parts);
        free(keys);
        return URSATZ_ERR_ALLOC;
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
    memset(score, 0, sizeof(*score));
    score->events = notes->events;
    score->pitch_values = notes->pitch_values;
    score->count = notes->count;
    score->part_index = notes->source.part_index;
    score->staff = notes->source.staff;
    score->parts = parts;
    score->part_count = part_count;
    score->meter_map = notes->meter.meter_map;
    score->key_signatures = keys;
    score->key_signature_count = key_count;
    score->quantize = quantize;
    score->title = metadata ? metadata->title : NULL;
    score->composer = metadata ? metadata->composer : NULL;
    *parts_out = parts;
    *keys_out = keys;
    return URSATZ_OK;
}

static bool export_arguments_ok(const ImportedScore *notes) {
    return notes && (notes->count == 0 || (notes->events && notes->pitch_values));
}

static UrsatzStatus score_export_musicxml(const ImportedScore *notes, const AnalyzerScoreMetadata *metadata, char **xml) {
    if (!xml) {
        return URSATZ_ERR_REQUEST;
    }
    *xml = NULL;
    if (!export_arguments_ok(notes)) {
        return URSATZ_ERR_REQUEST;
    }
    const ScoreQuantizeOptions quantize = score_quantize_options_default();
    MusicXmlExportScore score;
    MusicXmlExportPart *parts = NULL;
    MusicXmlExportKeySignature *keys = NULL;
    UrsatzStatus status = build_export_score(notes, metadata, &quantize, &score, &parts, &keys);
    if (status != URSATZ_OK) {
        return status;
    }
    char *text = NULL;
    status = status_of(musicxml_export_write_score(&score, &text));
    free(parts);
    free(keys);
    if (status != URSATZ_OK) {
        return status;
    }
    *xml = text;
    return URSATZ_OK;
}

UrsatzStatus analyzer_score_export_musicxml(const ImportedScore *notes, const AnalyzerScoreMetadata *metadata, char **xml,
                                            UrsatzError *error) {
    return analyzer_status_error(error, score_export_musicxml(notes, metadata, xml), "export the score as MusicXML");
}

UrsatzStatus analyzer_score_layout(const ImportedScore *notes, ScoreLayout *out) {
    if (!out) {
        return URSATZ_ERR_REQUEST;
    }
    memset(out, 0, sizeof(*out));
    if (!export_arguments_ok(notes)) {
        return URSATZ_ERR_REQUEST;
    }
    const ScoreQuantizeOptions quantize = score_quantize_options_default();
    MusicXmlExportScore score;
    MusicXmlExportPart *parts = NULL;
    MusicXmlExportKeySignature *keys = NULL;
    UrsatzStatus status = build_export_score(notes, NULL, &quantize, &score, &parts, &keys);
    if (status != URSATZ_OK) {
        return status;
    }
    status = status_of(score_layout_build(&score, out));
    free(parts);
    free(keys);
    return status;
}

static UrsatzStatus score_note_xml_ids(const ImportedScore *notes, char ***ids, size_t *count) {
    if (!ids || !count) {
        return URSATZ_ERR_REQUEST;
    }
    *ids = NULL;
    *count = 0;
    if (!notes || (notes->count > 0 && !notes->events)) {
        return URSATZ_ERR_REQUEST;
    }
    if (notes->count == 0) {
        return URSATZ_OK;
    }

    char **out = calloc(notes->count, sizeof(char *));
    if (!out) {
        return URSATZ_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        const EntityID *id = notes->events[i] ? note_event_id(notes->events[i]) : NULL;
        const char *value = id ? entity_id_value(id) : NULL;
        if (!value) {
            analyzer_score_note_xml_ids_destroy(out, i);
            return URSATZ_ERR_INTERNAL;
        }
        /* The formatter adds at most a leading n and cuts nothing; the export cuts a base id to 63 characters. */
        size_t size = strlen(value) + 2;
        out[i] = malloc(size);
        if (!out[i]) {
            analyzer_score_note_xml_ids_destroy(out, i);
            return URSATZ_ERR_ALLOC;
        }
        if (!musicxml_export_note_xml_id(value, out[i], size)) {
            free(out[i]);
            analyzer_score_note_xml_ids_destroy(out, i);
            return URSATZ_ERR_INTERNAL;
        }
        if (strlen(out[i]) > XML_ID_EXPORT_LIMIT) {
            out[i][XML_ID_EXPORT_LIMIT] = '\0';
        }
    }
    *ids = out;
    *count = notes->count;
    return URSATZ_OK;
}

UrsatzStatus analyzer_score_note_xml_ids(const ImportedScore *notes, char ***ids, size_t *count, UrsatzError *error) {
    return analyzer_status_error(error, score_note_xml_ids(notes, ids, count), "find the note ids of the score");
}

void analyzer_score_note_xml_ids_destroy(char **ids, size_t count) {
    if (!ids) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

bool analyzer_imported_meter_defaulted(const ImportedScore *notes) {
    if (!notes) {
        return false;
    }
    return !notes->meter.meter_map || meter_map_is_defaulted(notes->meter.meter_map);
}
