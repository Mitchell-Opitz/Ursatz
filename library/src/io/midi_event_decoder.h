#ifndef MUSIC_IO_MIDI_EVENT_DECODER_H
#define MUSIC_IO_MIDI_EVENT_DECODER_H

#include <stddef.h>
#include <stdint.h>

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

#endif
