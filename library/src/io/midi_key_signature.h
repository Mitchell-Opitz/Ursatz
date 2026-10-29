#ifndef MUSIC_IO_MIDI_KEY_SIGNATURE_H
#define MUSIC_IO_MIDI_KEY_SIGNATURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pitch/key_signature.h"
#include "semantics/analysis_key.h"
#include "time/musical_time.h"

/*
 * midi_key_signature: scans a Standard MIDI File buffer for the first
 * Key Signature meta-event (0xFF 0x59), independently of
 * midi_event_decoder (which only extracts note-on/note-off events and
 * discards meta-event contents without interpreting them).
 *
 * Reuses smf_reader for header/track chunking, then walks each track's
 * raw bytes with its own minimal event-length skipping (the delta-time
 * reader is midi_event_decoder_read_vlq), keeping this scan isolated from
 * midi_event_decoder_decode and its callers.
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
 * midi_key_signature_analysis_key: the AnalysisKey a scanned key signature names, through
 * analysis_key_from_fifths (the "mi" byte picks major or minor). False when no key signature was
 * found, for NULL arguments, or for an sf outside -7..7; *out is then untouched. The id is NULL.
 */
bool midi_key_signature_analysis_key(const MidiKeySignature *signature, AnalysisKey *out);

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

/*
 * midi_key_signature_scan_all: every Key Signature meta-event (0xFF 0x59) in an SMF buffer,
 * with its position, in time order. midi_key_signature_scan (above) is kept unchanged for
 * existing callers; it reports the first event in track-then-byte order and returns the raw
 * bytes. This is the function that reads key changes.
 *
 * Each event carries a KeySignature (midi_key_signature_build of its sf byte) and, next to
 * it, a mode hint from the mi byte. The hint is not part of the KeySignature, which makes no
 * claim about key or mode; deciding a key from sf and mi is analysis work.
 *
 * Positions are MusicalTime in whole notes: tick / (4 * division), as MIDIImporter converts.
 * An SMPTE division is rejected as UNSUPPORTED_DIVISION, and so is a division of 0 (no
 * position can be formed).
 *
 * Order: by tick, then track index, then byte order within a track. Events from all tracks are
 * merged. An exact duplicate (same tick, same sf byte, same mi byte) is reported once, on the
 * lowest track. Different events at the same tick are all kept.
 *
 * Malformed events are skipped and scanning continues: a 0xFF 0x59 whose length is not 2, or
 * whose sf is outside -7..7. An mi other than 0 or 1 is kept, with an UNKNOWN hint. A structural
 * SMF error is reported as midi_key_signature_scan reports it. No key signature events is OK
 * with zero events.
 */

typedef enum {
    MIDI_MODE_HINT_MAJOR = 0,
    MIDI_MODE_HINT_MINOR,
    MIDI_MODE_HINT_UNKNOWN
} MidiModeHint;

typedef struct {
    KeySignature *key_signature;
    MidiModeHint mode_hint;
    MusicalTime *position;
    uint64_t tick;
    size_t track_index;
    int8_t sharps_flats; /* the raw sf byte */
    uint8_t mode_byte;   /* the raw mi byte */
} MidiKeySignatureEvent;

typedef struct {
    MidiKeySignatureEvent *events; /* owned; NULL when count is 0 */
    size_t count;
} MidiKeySignatureEvents;

typedef enum {
    MIDI_KEY_SIGNATURE_SCAN_ALL_OK = 0,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_NULL_BUFFER,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_TRUNCATED,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_HEADER,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_FORMAT,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_DIVISION,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_TRACK,
    MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC
} MidiKeySignatureScanAllResult;

/* On OK, *out owns its events (midi_key_signature_events_destroy). Otherwise *out is
   untouched. */
MidiKeySignatureScanAllResult midi_key_signature_scan_all(const unsigned char *data, size_t size,
                                                           MidiKeySignatureEvents *out);

/* Destroys every event's KeySignature and position, and the list. NULL is a no-op. Leaves
   events NULL and count 0. */
void midi_key_signature_events_destroy(MidiKeySignatureEvents *events);

/*
 * Collector (internal to the io module, not part of the public surface): the per-event hook that
 * lets one pass over a file's tracks feed the key signatures too. midi_key_signature_scan_all walks
 * the tracks itself and feeds a collector; midi_importer_import_complete feeds the same collector
 * from the one walk midi_event_decoder_scan makes, so the file is not scanned a second time.
 *
 * midi_key_signature_collector_meta takes any meta-event (type, payload, absolute tick) and keeps it
 * only if it is a well-formed key signature (type 0x59, length 2, sf in -7..7); every other event is
 * ignored and returns 0. It returns -1 on allocation failure.
 *
 * midi_key_signature_collector_finish turns the kept events into the list midi_key_signature_scan_all
 * documents (time order, duplicates merged), with positions tick / (4 * division). A division of 0 is
 * UNSUPPORTED_DIVISION. On OK, *out owns its events. The collector can be destroyed after either call.
 */
typedef struct MidiKeySignatureCollector MidiKeySignatureCollector;

MidiKeySignatureCollector *midi_key_signature_collector_create(void);
void midi_key_signature_collector_destroy(MidiKeySignatureCollector *collector);
int midi_key_signature_collector_meta(MidiKeySignatureCollector *collector, size_t track_index, uint64_t tick,
                                      unsigned char meta_type, const unsigned char *data, uint32_t length);
MidiKeySignatureScanAllResult midi_key_signature_collector_finish(MidiKeySignatureCollector *collector,
                                                                   uint16_t division, MidiKeySignatureEvents *out);

#endif
