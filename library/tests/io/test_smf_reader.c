#include "io/smf_reader.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * smf_reader tests: valid type-0 and type-1 headers parse and yield the
 * right number of track chunks; malformed/truncated input fails cleanly
 * with a specific error rather than crashing or silently misparsing.
 */

static void append_bytes(unsigned char *buf, size_t *pos, const void *bytes, size_t len) {
    memcpy(buf + *pos, bytes, len);
    *pos += len;
}

static void append_u32(unsigned char *buf, size_t *pos, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                           (unsigned char)v};
    append_bytes(buf, pos, b, 4);
}

static void append_u16(unsigned char *buf, size_t *pos, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v >> 8), (unsigned char)v};
    append_bytes(buf, pos, b, 2);
}

static void test_parses_valid_type0_single_track(void) {
    unsigned char buf[64];
    size_t pos = 0;

    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);   /* format */
    append_u16(buf, &pos, 1);   /* track_count */
    append_u16(buf, &pos, 96);  /* division (PPQN) */

    unsigned char track_body[] = {0x00, 0xFF, 0x2F, 0x00}; /* delta 0, end-of-track meta */
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)sizeof(track_body));
    append_bytes(buf, &pos, track_body, sizeof(track_body));

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, pos, &header, &tracks, &track_count);

    assert(r == SMF_READER_OK);
    assert(header.format == 0);
    assert(header.track_count == 1);
    assert(header.division == 96);
    assert(track_count == 1);
    assert(tracks[0].size == sizeof(track_body));
    assert(memcmp(tracks[0].data, track_body, sizeof(track_body)) == 0);

    free(tracks);
}

static void test_parses_valid_type1_multi_track(void) {
    unsigned char buf[128];
    size_t pos = 0;

    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 1); /* format */
    append_u16(buf, &pos, 2); /* track_count */
    append_u16(buf, &pos, 480);

    unsigned char track_a[] = {0x00, 0xFF, 0x2F, 0x00};
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)sizeof(track_a));
    append_bytes(buf, &pos, track_a, sizeof(track_a));

    unsigned char track_b[] = {0x00, 0x90, 0x3C, 0x40, 0x60, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00};
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)sizeof(track_b));
    append_bytes(buf, &pos, track_b, sizeof(track_b));

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, pos, &header, &tracks, &track_count);

    assert(r == SMF_READER_OK);
    assert(header.format == 1);
    assert(track_count == 2);
    assert(tracks[0].size == sizeof(track_a));
    assert(tracks[1].size == sizeof(track_b));

    free(tracks);
}

static void test_rejects_bad_magic(void) {
    unsigned char buf[14] = {0};
    memcpy(buf, "XXXX", 4);

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, sizeof(buf), &header, &tracks, &track_count);

    assert(r == SMF_READER_ERR_BAD_HEADER);
    assert(tracks == NULL);
}

static void test_rejects_type2(void) {
    unsigned char buf[14];
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 2); /* format 2, unsupported */
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, 96);

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, pos, &header, &tracks, &track_count);

    assert(r == SMF_READER_ERR_UNSUPPORTED_FORMAT);
    assert(tracks == NULL);
}

static void test_rejects_smpte_division(void) {
    unsigned char buf[14];
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, 0xE250); /* top bit set: SMPTE format */

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, pos, &header, &tracks, &track_count);

    assert(r == SMF_READER_ERR_UNSUPPORTED_DIVISION);
    assert(tracks == NULL);
}

static void test_rejects_truncated_header(void) {
    unsigned char buf[10] = {0};
    memcpy(buf, "MThd", 4);

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, sizeof(buf), &header, &tracks, &track_count);

    assert(r == SMF_READER_ERR_TRUNCATED);
    assert(tracks == NULL);
}

static void test_rejects_truncated_track(void) {
    unsigned char buf[64];
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, 96);

    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, 100); /* claims 100 bytes but buffer ends here */

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(buf, pos, &header, &tracks, &track_count);

    assert(r == SMF_READER_ERR_BAD_TRACK);
    assert(tracks == NULL);
}

static void test_rejects_null_buffer(void) {
    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult r = smf_reader_parse(NULL, 0, &header, &tracks, &track_count);
    assert(r == SMF_READER_ERR_NULL_BUFFER);
}

int main(void) {
    test_parses_valid_type0_single_track();
    test_parses_valid_type1_multi_track();
    test_rejects_bad_magic();
    test_rejects_type2();
    test_rejects_smpte_division();
    test_rejects_truncated_header();
    test_rejects_truncated_track();
    test_rejects_null_buffer();
    printf("test_smf_reader: ok\n");
    return 0;
}
