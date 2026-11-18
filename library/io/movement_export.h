#ifndef URSATZ_IO_MOVEMENT_EXPORT_H
#define URSATZ_IO_MOVEMENT_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "realization/movement.h"
#include "realization/work.h"

/*
 * Export of a movement's performer parts (R-599, target architecture 8.6): the movement's notes as a Standard MIDI File and as a
 * MusicXML document, through MIDIRenderer (io/midi_renderer.h) and MusicXmlExport (io/musicxml_export.h). One NoteEvent is made
 * per performer note, parts in order and each part's notes in order: its EntityID is the performer note id (so the id is the
 * MusicXML xml:id), its VoiceReference is "<part id>-v<voice>", its pitch spelling is the note's letter and accidental (a note
 * that is not spelled is written with sharps, as the Composer's render does with no key) and its provenance is absent.
 *
 * Both functions refuse a movement with played spans (MOVEMENT_EXPORT_ERR_NOT_BUILT: unrolling a played order waits for a phase that
 * writes repeats as signs) and one no part of which has a note (MOVEMENT_EXPORT_ERR_NO_NOTES). `message` (may be NULL, with
 * message_size 0) says what refused; it is empty on success.
 *
 * MIDI: track 0 is the conductor, named the movement's title ("piece" when it has none) when `track_names`; then one track per
 * performer part in order, named the part's name when `track_names`, with a program change on each of its channels and the part's
 * sustain pedal as controller 64. A part's voices are the voices its notes use, in voice order, and each is a channel, numbered
 * in the order the voices first sound over the whole movement, skipping channel 9 (so a movement whose lines were tracks keeps
 * each note's channel). The movement's tempo map, meter map, key signatures (a MAJOR mode byte 0, MINOR 1, UNKNOWN 0 with no mode
 * hint) and markers are written as given. The division is 480 ticks per quarter when every time (a note's onset and duration, a
 * pedal, a key signature, a marker, a meter or tempo region start) is a whole tick there, else the smallest that works up to
 * MIDI_RENDER_MAX_DIVISION, else MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE. More than 15 voices over all parts are
 * MOVEMENT_EXPORT_ERR_TOO_MANY_VOICES.
 *
 * MusicXML: one part per performer part (its id and name, its staff count and its General MIDI program), each note on its part's
 * staff, the movement's meter map and tempo map, key signatures in part 0, the sustain pedal of every part (written under the
 * part's last staff, the bass staff of a grand staff), and the work's title and composer when there is a work.
 */

typedef enum {
    MOVEMENT_EXPORT_OK,
    MOVEMENT_EXPORT_ERR_NULL,
    MOVEMENT_EXPORT_ERR_NO_NOTES,
    MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE,
    MOVEMENT_EXPORT_ERR_TOO_MANY_VOICES,
    MOVEMENT_EXPORT_ERR_NOT_BUILT,
    MOVEMENT_EXPORT_ERR_ALLOC
} MovementExportResult;

typedef struct {
    bool track_names; /* the conductor's and each part's name */
} MovementMidiOptions;

/* options may be NULL: track names on. On OK *out_data is a malloc'd buffer the caller free()s; otherwise nothing is written. */
MovementExportResult movement_export_midi(const Movement *movement, const MovementMidiOptions *options, unsigned char **out_data,
                                          size_t *out_size, char *message, size_t message_size);

/* work may be NULL (no title or composer). On OK *out_xml is a malloc'd, NUL-terminated document the caller free()s. */
MovementExportResult movement_export_musicxml(const Work *work, const Movement *movement, char **out_xml, char *message,
                                              size_t message_size);

#endif
