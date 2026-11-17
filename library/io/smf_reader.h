#ifndef MUSIC_IO_SMF_READER_H
#define MUSIC_IO_SMF_READER_H

#include <stddef.h>
#include <stdint.h>

/*
 * smf_reader: byte-level Standard MIDI File (SMF) chunk parsing. Reads
 * the header chunk (MThd) and yields track chunk (MTrk) byte ranges
 * within the caller's buffer, without interpreting event bytes -- that
 * is midi_event_decoder's job.
 *
 * Fails cleanly on anything that is not a well-formed SMF type 0 or
 * type 1 file: bad magic, truncated chunks, unsupported format (type
 * 2), or SMPTE-format division. No best-effort recovery is attempted.
 */

typedef enum {
    SMF_READER_OK = 0,
    SMF_READER_ERR_NULL_BUFFER,
    SMF_READER_ERR_TRUNCATED,
    SMF_READER_ERR_BAD_HEADER,
    SMF_READER_ERR_UNSUPPORTED_FORMAT,
    SMF_READER_ERR_UNSUPPORTED_DIVISION,
    SMF_READER_ERR_BAD_TRACK
} SmfReaderResult;

/* format is 0 or 1 (2 is rejected as SMF_READER_ERR_UNSUPPORTED_FORMAT).
   division is the header's ticks-per-quarter-note (PPQN); the SMPTE
   form (top bit of the raw 16-bit field set) is rejected as
   SMF_READER_ERR_UNSUPPORTED_DIVISION rather than decoded. */
typedef struct {
    uint16_t format;
    uint16_t track_count;
    uint16_t division;
} SmfHeader;

/* A single track chunk's event bytes within the caller's buffer (not
   owned; valid only as long as the original buffer is). */
typedef struct {
    const unsigned char *data;
    size_t size;
} SmfTrack;

/*
 * Parses data[0..size) as an SMF header chunk followed by header->
 * track_count MTrk chunks. On success, *out_tracks is a newly allocated
 * array of *out_track_count SmfTrack entries (caller frees the array
 * with free(); the SmfTrack entries themselves point into data and are
 * not separately owned).
 */
SmfReaderResult smf_reader_parse(const unsigned char *data, size_t size, SmfHeader *out_header,
                                  SmfTrack **out_tracks, size_t *out_track_count);

#endif
