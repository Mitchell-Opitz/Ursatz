#include "io/smf_reader.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_u16(const unsigned char *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_u32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

SmfReaderResult smf_reader_parse(const unsigned char *data, size_t size, SmfHeader *out_header,
                                  SmfTrack **out_tracks, size_t *out_track_count) {
    if (data == NULL || out_header == NULL || out_tracks == NULL || out_track_count == NULL) {
        return SMF_READER_ERR_NULL_BUFFER;
    }

    /* MThd chunk: 4-byte magic, 4-byte length (must be 6), 6 bytes of
       format/track_count/division. */
    if (size < 14) {
        return SMF_READER_ERR_TRUNCATED;
    }
    if (memcmp(data, "MThd", 4) != 0) {
        return SMF_READER_ERR_BAD_HEADER;
    }
    uint32_t header_length = read_u32(data + 4);
    if (header_length != 6) {
        return SMF_READER_ERR_BAD_HEADER;
    }

    uint16_t format = read_u16(data + 8);
    uint16_t track_count = read_u16(data + 10);
    uint16_t raw_division = read_u16(data + 12);

    if (format != 0 && format != 1) {
        return SMF_READER_ERR_UNSUPPORTED_FORMAT;
    }
    if (raw_division & 0x8000u) {
        return SMF_READER_ERR_UNSUPPORTED_DIVISION;
    }
    if (format == 0 && track_count != 1) {
        return SMF_READER_ERR_BAD_HEADER;
    }

    SmfTrack *tracks = NULL;
    if (track_count > 0) {
        tracks = malloc(sizeof(SmfTrack) * track_count);
        if (tracks == NULL) {
            return SMF_READER_ERR_TRUNCATED;
        }
    }

    size_t offset = 14;
    for (uint16_t i = 0; i < track_count; i++) {
        if (offset + 8 > size) {
            free(tracks);
            return SMF_READER_ERR_BAD_TRACK;
        }
        if (memcmp(data + offset, "MTrk", 4) != 0) {
            free(tracks);
            return SMF_READER_ERR_BAD_TRACK;
        }
        uint32_t track_length = read_u32(data + offset + 4);
        offset += 8;
        if (offset + track_length > size) {
            free(tracks);
            return SMF_READER_ERR_BAD_TRACK;
        }
        tracks[i].data = data + offset;
        tracks[i].size = track_length;
        offset += track_length;
    }

    out_header->format = format;
    out_header->track_count = track_count;
    out_header->division = raw_division;
    *out_tracks = tracks;
    *out_track_count = track_count;
    return SMF_READER_OK;
}
