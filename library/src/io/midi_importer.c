#include "io/midi_importer.h"

/* Populated by later commits in this module. Present now so the
   library target and architecture-gate test have something to build
   against from the first commit. */

MidiImportResult midi_importer_import(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                       size_t *out_count) {
    (void)data;
    (void)size;
    (void)out_events;
    (void)out_count;
    return MIDI_IMPORT_ERR_ALLOC;
}
