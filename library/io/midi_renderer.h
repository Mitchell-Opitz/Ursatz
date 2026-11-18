#ifndef MUSIC_IO_MIDI_RENDERER_H
#define MUSIC_IO_MIDI_RENDERER_H

#include <stddef.h>

#include "model/events/note_event.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/time/meter_map.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"

/*
 * MIDIRenderer (L9, music-io, Registry "MIDIRenderer", Canonical -> MIDI): writes
 * NoteEvents, and optionally a MeterMap, a TempoMap and key signature events, to a
 * Standard MIDI File of format 1. It is the inverse of MIDIImporter
 * (io/midi_importer.h), which reads what this writes. See docs/modules/midi_renderer.md.
 *
 * INPUTS. All borrowed and never mutated.
 *   - events / pitch_values / count: count NoteEvents and, index-paired with them, the
 *     PitchNumericValue of each note's pitch -- the pairing a MidiImport holds (midi_importer_import_complete).
 *     The renderer needs no PitchSpelling. count may be 0 (events and pitch_values may
 *     then be NULL): the file is the conductor track alone.
 *   - meter_map, tempo_map: optional. With none supplied the file carries no time
 *     signature or tempo event, i.e. the SMF defaults (4/4, 120 quarter notes per
 *     minute) apply; the renderer never invents them.
 *   - key_signatures / key_signature_count: optional MidiKeySignatureEvents, the type
 *     a MidiImport's key_signatures holds (position, sf byte, mi byte). None supplied
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
 * What it does not do: velocity or dynamics, articulation, control events other than the sustain pedal of a part,
 * program changes other than a part's, sysex, SMPTE division, format 0 or 2, lyrics, cue points, audio playback.
 *
 * Track names and markers: midi_renderer_render_with_options writes a Sequence/Track Name (0xFF 0x03) at tick 0
 * of track 0 and of each named voice track, and markers (0xFF 0x06) in track 0 after the tempo, time signature and
 * key signature events of an equal tick. Without options it writes neither.
 *
 * PARTS (options->parts, R-599): the notes of several voices are grouped into performer part tracks. Track 0 is as above,
 * then one track per part in the given order, named by the part's `name` at tick 0 when it is not NULL (the options'
 * track_names are not read). Each voice id a part lists is one channel; channels go to the voices of all parts together
 * in the order they first sound (earliest start tick, then input order), skipping channel 9, the rule voice tracks follow,
 * so a movement whose voices were tracks keeps each note's channel. Each channel gets a program change (0xC0 | channel,
 * the part's program) at tick 0 after the track name, and each pedal span of a part writes controller 64 with value 127
 * at its start tick and value 0 at its end tick on every channel of the part. At an equal tick a part track holds the
 * note-offs, then controller 64 value 0, then controller 64 value 127, then the note-ons, otherwise in input order.
 * Refused: a voice listed twice, a note with no voice or whose voice no part lists, a program outside 0 to 127, a pedal
 * whose ticks are not whole (all MIDI_RENDER_ERR_NOT_REPRESENTABLE), more than 15 channels (MIDI_RENDER_ERR_TOO_MANY_VOICES).
 * A same-pitch overlap is refused per channel. A part none of whose voices has a note still gets its (empty) track.
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
 * A marker meta-event (0xFF 0x06) in track 0 at position, text written as given. A NULL text is
 * MIDI_RENDER_ERR_NULL_ARG; a position that is negative or not a whole tick, or a text longer than 0x0FFFFFFF
 * bytes, is MIDI_RENDER_ERR_NOT_REPRESENTABLE.
 */
typedef struct {
    MusicalTime position;
    const char *text;
} MidiMarker;

/*
 * A performer part's track (see PARTS above): its name (NULL: none), the General MIDI program (0 to 127) every channel of
 * the part gets, the ids of the voices it holds (voice_ids, voice_count: the voice reference target ids of the notes) and
 * its sustain pedal spans (pedals, pedal_count: whole ticks, start before end, not overlapping). All borrowed.
 */
typedef struct {
    const char *name;
    int program;
    const char *const *voice_ids;
    size_t voice_count;
    const TimeSpan *pedals;
    size_t pedal_count;
} MidiRenderPart;

/*
 * Optional extras for midi_renderer_render_with_options; every member is optional (NULL, 0).
 * conductor_name names track 0. Markers are written at their tick after the tempo, time signature and key
 * signature events of an equal tick, in the order supplied among themselves. With parts the voices are grouped into
 * part tracks (see PARTS above); with part_count 0 the renderer is exactly as before, one track per voice.
 */
typedef struct {
    const char *conductor_name;
    const MidiTrackName *track_names;
    size_t track_name_count;
    const MidiMarker *markers;
    size_t marker_count;
    const MidiRenderPart *parts;
    size_t part_count;
} MidiRenderOptions;

/*
 * Renders to a newly allocated SMF buffer (see above). events and pitch_values are each
 * count entries, index-paired; no entry may be NULL. NULL options write no track names and no markers.
 */
MidiRenderResult midi_renderer_render_with_options(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                   size_t count, const MeterMap *meter_map, const TempoMap *tempo_map,
                                                   const MidiKeySignatureEvent *key_signatures,
                                                   size_t key_signature_count, const MidiRenderOptions *options,
                                                   unsigned division, unsigned char **out_data, size_t *out_size);

#endif
