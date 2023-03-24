#ifndef MUSIC_IO_MIDI_EVENT_DECODER_H
#define MUSIC_IO_MIDI_EVENT_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "io/smf_reader.h"
#include "time/time_signature.h"

/*
 * midi_event_decoder: walks the decoded byte stream of a single MTrk
 * chunk (as produced by smf_reader) and extracts note-on/note-off
 * events only. All other MIDI event types (controller/CC, program
 * change, pitch bend, aftertouch, tempo and other meta-events, sysex)
 * are recognized only enough to skip their bytes correctly -- they are
 * never decoded into MidiNoteEvent and never treated as an error.
 *
 * Running status (an omitted status byte reusing the previous event's
 * status) is handled, since real-world SMF files rely on it.
 *
 * A note-on with velocity 0 is treated as a note-off, per the MIDI
 * spec.
 *
 * Time signature meta-events (0xFF 0x58) are read separately, by
 * midi_event_decoder_collect_time_signatures below. midi_event_decoder_decode
 * still skips them, so its output and signature are unchanged.
 */

typedef enum {
    MIDI_NOTE_ON = 0,
    MIDI_NOTE_OFF = 1
} MidiNoteEventKind;

typedef struct {
    MidiNoteEventKind kind;
    uint32_t tick;   /* absolute tick position, accumulated from delta-times */
    uint8_t channel; /* 0-15 */
    uint8_t note;    /* 0-127 */
    uint8_t velocity;
} MidiNoteEvent;

typedef enum {
    MIDI_EVENT_DECODER_OK = 0,
    MIDI_EVENT_DECODER_ERR_NULL_TRACK,
    MIDI_EVENT_DECODER_ERR_TRUNCATED,
    MIDI_EVENT_DECODER_ERR_ALLOC
} MidiEventDecoderResult;

/*
 * Decodes track_data[0..track_size) into a newly allocated array of
 * *out_count MidiNoteEvent entries (caller frees with free()), in the
 * order encountered (which is tick order, since delta-times cannot be
 * negative).
 */
MidiEventDecoderResult midi_event_decoder_decode(const unsigned char *track_data, size_t track_size,
                                                  MidiNoteEvent **out_events, size_t *out_count);

/*
 * Time signature meta-events: 0xFF 0x58, length 4, payload nn dd cc bb. nn is the top number and
 * dd the bottom number as a power of two (dd = 3 is an eighth-note bottom, so nn dd = 6 3 is 6/8).
 * cc (MIDI clocks per metronome click) and bb (32nd notes per quarter) are not read except that
 * the event must be 4 bytes long. The result is a TimeSignature, which makes no claim about beats.
 *
 * Collected from every track and merged in time order. Order: by tick, then track index, then byte
 * order within a track. At most one event per tick: an exact duplicate (same tick, same
 * signature) is merged, and if different signatures share a tick the last one in track order (then
 * byte order) wins and the tick is reported in conflict_ticks. The library does not print the
 * warning; the caller decides what to do with conflict_ticks.
 *
 * Malformed events are skipped, as midi_key_signature_scan_all skips its malformed events: a length
 * other than 4, nn outside 1..TIME_SIGNATURE_MAX_TOP_NUMBER, or dd above
 * TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT (a bottom number the TimeSignature type cannot hold).
 * A track whose bytes are structurally malformed fails the collect with TRUNCATED, as
 * midi_event_decoder_decode does. An SMPTE division never reaches this function: smf_reader_parse
 * rejects it when the tracks are read. No time signature events is OK with zero events.
 */

typedef struct {
    TimeSignature *time_signature; /* owned */
    uint64_t tick;                 /* absolute tick position */
    size_t track_index;            /* the track of the event that was kept */
} MidiTimeSignatureEvent;

typedef struct {
    MidiTimeSignatureEvent *events; /* owned; NULL when count is 0 */
    size_t count;
    uint64_t *conflict_ticks; /* owned; NULL when conflict_count is 0 */
    size_t conflict_count;
} MidiTimeSignatureEvents;

/* On OK, *out owns its events (midi_time_signature_events_destroy). Otherwise *out is untouched. */
MidiEventDecoderResult midi_event_decoder_collect_time_signatures(const SmfTrack *tracks, size_t track_count,
                                                                    MidiTimeSignatureEvents *out);

/* Destroys every event's TimeSignature, and the lists. NULL is a no-op. */
void midi_time_signature_events_destroy(MidiTimeSignatureEvents *events);

#endif
