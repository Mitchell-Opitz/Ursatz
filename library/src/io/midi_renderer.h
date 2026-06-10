#ifndef MUSIC_IO_MIDI_RENDERER_H
#define MUSIC_IO_MIDI_RENDERER_H

#include <stddef.h>

#include "events/note_event.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/tempo_map.h"

/*
 * MIDIRenderer (L9, music-io, Registry "MIDIRenderer", Canonical -> MIDI): writes
 * NoteEvents, and optionally a MeterMap, a TempoMap and key signature events, to a
 * Standard MIDI File of format 1. It is the inverse of MIDIImporter
 * (io/midi_importer.h), which reads what this writes. See docs/modules/midi_renderer.md.
 *
 * INPUTS. All borrowed and never mutated; nothing is read from persistence.
 *   - events / pitch_values / count: count NoteEvents and, index-paired with them, the
 *     PitchNumericValue of each note's pitch -- the pairing midi_importer_import returns.
 *     The renderer needs no PitchSpelling. count may be 0 (events and pitch_values may
 *     then be NULL): the file is the conductor track alone.
 *   - meter_map, tempo_map: optional. With none supplied the file carries no time
 *     signature or tempo event, i.e. the SMF defaults (4/4, 120 quarter notes per
 *     minute) apply; the renderer never invents them.
 *   - key_signatures / key_signature_count: optional MidiKeySignatureEvents, the type
 *     midi_key_signature_scan_all returns (position, sf byte, mi byte). None supplied
 *     means no key signature event is written.
 *   - division: ticks per quarter note, 1 to 0x7FFF. SMPTE division is out of scope.
 *
 * EXACT TICKS. Time is tempo-independent: a position or duration is a whole-note
 * fraction, and a tick is a quarter-note fraction of 1/division, so
 * tick = position * 4 * division. The renderer is exact: a note position or duration, or
 * a meter, tempo or key signature position, that is not an integer number of ticks at
 * this division is MIDI_RENDER_ERR_NOT_REPRESENTABLE. It never rounds.
 *
 * OUTPUT. The caller owns the returned buffer and frees it with free(). On any result
 * other than MIDI_RENDER_OK neither *out_data nor *out_size is written. The same inputs
 * always give the same bytes.
 *   - Header chunk: format 1, the track count, the division.
 *   - Track 0, the conductor: a set-tempo meta-event (0xFF 0x51) at the start tick of each
 *     TempoMap region, a time signature meta-event (0xFF 0x58) at the start tick of each
 *     MeterMap region, and a key signature meta-event (0xFF 0x59) for each supplied key
 *     signature event, in tick order (tempo, then time signature, then key signature at an
 *     equal tick; supplied order within a kind). A region the map marks defaulted is
 *     written like any other region, so it comes back from the importer as a declared one.
 *     The time signature's denominator is written as a power of two, with the MIDI default
 *     24 MIDI clocks per metronome click and 8 notated 32nd notes per quarter, for every
 *     signature.
 *   - One further track per distinct voice reference (by the voice's target id), holding
 *     that voice's notes; notes with no voice share one track. Tracks follow the order in
 *     which their voices first sound (earliest start tick, then input order). Track i
 *     (counting from 1) plays on channel i - 1, skipping channel 9 (percussion), so at
 *     most 15 voice tracks exist: more is MIDI_RENDER_ERR_TOO_MANY_VOICES, never a reused
 *     channel.
 *   - Every note is a note-on (velocity 64, the fixed default: dynamics are not modeled)
 *     and a note-off (velocity 0). At an equal tick every note-off is written before every
 *     note-on, so a note ending where another starts on the same pitch is released first;
 *     otherwise events keep the input order.
 *   - Every track ends with the End of Track meta-event (0xFF 0x2F). Delta times are
 *     variable-length quantities. Running status is NEVER used: every event carries its
 *     own status byte.
 *
 * NOT REPRESENTABLE (MIDI_RENDER_ERR_NOT_REPRESENTABLE): a position or duration that is
 * not an integer number of ticks; a negative position; a zero or negative duration; a tick
 * beyond the 28-bit variable-length quantity range (0x0FFFFFFF); a pitch that is not an
 * integer MIDI note number from 0 to 127; two notes of the same pitch that overlap within
 * one voice's track (MIDI cannot pair their note-offs unambiguously); a tempo of 0 or above
 * 0xFFFFFF microseconds per quarter note; a time signature whose top number is above 255;
 * a key signature sf outside -7 to 7.
 *
 * What it does not do: velocity or dynamics, articulation, pedal or control events,
 * program changes, sysex, SMPTE division, format 0 or 2, track names, audio playback.
 */

typedef enum {
    MIDI_RENDER_OK = 0,
    MIDI_RENDER_ERR_NULL_ARG,
    MIDI_RENDER_ERR_BAD_DIVISION,
    MIDI_RENDER_ERR_NOT_REPRESENTABLE,
    MIDI_RENDER_ERR_TOO_MANY_VOICES,
    MIDI_RENDER_ERR_ALLOC
} MidiRenderResult;

/* The largest division the renderer writes (the top bit of the header field means SMPTE). */
#define MIDI_RENDER_MAX_DIVISION 0x7FFF

/* Velocity of every note-on, and the number of voice tracks that fit on the 15 non-percussion channels. */
#define MIDI_RENDER_NOTE_VELOCITY 64
#define MIDI_RENDER_MAX_VOICE_TRACKS 15

/*
 * The name of the track holding one voice's notes: voice_id is the voice reference's target id value
 * (entity_id_value), NULL for the track of notes with no voice. name is written as given, as a
 * Sequence/Track Name meta-event (0xFF 0x03) at tick 0, first in its track. A name for a voice with no
 * notes is ignored. Two names for one voice are MIDI_RENDER_ERR_NOT_REPRESENTABLE; a NULL name is
 * MIDI_RENDER_ERR_NULL_ARG.
 */
typedef struct {
    const char *voice_id;
    const char *name;
} MidiTrackName;

/*
 * Optional extras for midi_renderer_render_with_options; every member is optional (NULL, 0).
 * conductor_name names track 0.
 */
typedef struct {
    const char *conductor_name;
    const MidiTrackName *track_names;
    size_t track_name_count;
} MidiRenderOptions;

/*
 * Renders to a newly allocated SMF buffer (see above). events and pitch_values are each
 * count entries, index-paired; no entry may be NULL.
 */
MidiRenderResult midi_renderer_render(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                      const MeterMap *meter_map, const TempoMap *tempo_map,
                                      const MidiKeySignatureEvent *key_signatures, size_t key_signature_count,
                                      unsigned division, unsigned char **out_data, size_t *out_size);

/*
 * midi_renderer_render with options (NULL options: the same bytes as midi_renderer_render, which calls this).
 */
MidiRenderResult midi_renderer_render_with_options(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                   size_t count, const MeterMap *meter_map, const TempoMap *tempo_map,
                                                   const MidiKeySignatureEvent *key_signatures,
                                                   size_t key_signature_count, const MidiRenderOptions *options,
                                                   unsigned division, unsigned char **out_data, size_t *out_size);

#endif
