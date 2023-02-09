#ifndef MUSIC_IO_MIDI_KEY_SIGNATURE_H
#define MUSIC_IO_MIDI_KEY_SIGNATURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pitch/key_signature.h"

/*
 * midi_key_signature: scans a Standard MIDI File buffer for the first
 * Key Signature meta-event (0xFF 0x59), independently of
 * midi_event_decoder (which only extracts note-on/note-off events and
 * discards meta-event contents without interpreting them).
 *
 * Reuses smf_reader for header/track chunking, then walks each track's
 * raw bytes with its own minimal delta-time/event-length skipping --
 * deliberately duplicated rather than extending
 * midi_event_decoder_decode's signature, to keep this addition isolated
 * from the existing decoder and its callers.
 *
 * Only the first key signature meta-event encountered (scanning tracks
 * in order, byte order within each track) is reported; later key
 * changes (modulations) are not tracked.
 */

typedef struct {
    bool found;
    int8_t sharps_flats; /* -7..7: negative = flats, positive = sharps, per the MIDI spec's "sf" byte */
    bool is_minor;
} MidiKeySignature;

typedef enum {
    MIDI_KEY_SIGNATURE_OK = 0,
    MIDI_KEY_SIGNATURE_ERR_NULL_BUFFER,
    MIDI_KEY_SIGNATURE_ERR_TRUNCATED,
    MIDI_KEY_SIGNATURE_ERR_BAD_HEADER,
    MIDI_KEY_SIGNATURE_ERR_UNSUPPORTED_FORMAT,
    MIDI_KEY_SIGNATURE_ERR_UNSUPPORTED_DIVISION,
    MIDI_KEY_SIGNATURE_ERR_BAD_TRACK
} MidiKeySignatureResult;

/*
 * Parses data[0..size) as an SMF file (via smf_reader) and scans for a
 * Key Signature meta-event. On MIDI_KEY_SIGNATURE_OK, *out is
 * populated: out->found is true iff a well-formed key signature
 * meta-event was located, in which case out->sharps_flats and
 * out->is_minor carry its payload; out->found is false if the file
 * parses cleanly but contains no such event. Any other return value
 * means the file itself is malformed per smf_reader's rules and *out is
 * left untouched.
 */
MidiKeySignatureResult midi_key_signature_scan(const unsigned char *data, size_t size, MidiKeySignature *out);

/*
 * midi_key_signature_build: the KeySignature a MIDI "sf" (sharps/flats count) value
 * denotes, in the standard circle-of-fifths order. Sharps are added in the order
 * F C G D A E B, flats in the order B E A D G C F: sf = 3 is F# C# G#, sf = -2 is Bb Eb,
 * sf = 0 has no entries. This is a spelling of the signature only. It makes no claim
 * about which key or mode the music is in; the "mi" major/minor byte is a hint carried
 * next to a KeySignature, never part of it.
 */

typedef enum {
    MIDI_KEY_SIGNATURE_BUILD_OK = 0,
    MIDI_KEY_SIGNATURE_BUILD_ERR_NULL_OUT,
    MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE,
    MIDI_KEY_SIGNATURE_BUILD_ERR_ALLOC
} MidiKeySignatureBuildResult;

/* sharps_flats must be in -7..7. On OK, *out is a new KeySignature the caller owns
   (key_signature_destroy); otherwise *out is untouched. */
MidiKeySignatureBuildResult midi_key_signature_build(int sharps_flats, KeySignature **out);

#endif
