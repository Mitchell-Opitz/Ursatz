#include "export/musicxml_export.h"
#include "export/musicxml_score_internal.h"

#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "persistence/persistence_note_events.h"

/*
 * The single-part entry point: loads the persisted NoteEvents and writes them through the score writer (strict: an
 * overlap in a voice still fails, see musicxml_score_internal.h) with no MeterMap, so the layout is meter_map_create_defaulted's one 4/4 region and the output is flagged
 * ursatz:meter-defaulted. All layout lives in musicxml_score.c.
 */

static void free_note_events(NoteEvent **events, size_t count) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

MusicXmlExportResult musicxml_export_write(PersistenceHandle *handle, char **out_xml) {
    if (handle == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }

    NoteEvent **events = NULL;
    size_t count = 0;
    if (persistence_load_note_events(handle, &events, &count) != PERSISTENCE_NOTE_EVENTS_OK) {
        return MUSICXML_EXPORT_ERR_LOAD_FAILED;
    }

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = events;
    score.count = count;
    MusicXmlExportResult result = musicxml_export_write_score_strict(&score, out_xml);

    free_note_events(events, count);
    return result;
}
