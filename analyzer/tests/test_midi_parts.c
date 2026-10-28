#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/midi_pipeline.h>

/*
 * MIDI parts: a generated type-1 file whose tracks cover a track with two channels, a single-channel track with a
 * name, the percussion channel, a channel with no program change (a piano) and an unnamed melodic channel. Each
 * (track, channel) with notes is one part, with its name, program and staves, and every note's stream is its part.
 */

typedef struct {
    unsigned char bytes[256];
    size_t size;
} Track;

static void put(Track *track, unsigned char byte) {
    assert(track->size < sizeof track->bytes);
    track->bytes[track->size++] = byte;
}

static void put_delta(Track *track, unsigned delta) {
    assert(delta < 128 * 128);
    if (delta >= 128) {
        put(track, (unsigned char)(0x80 | (delta >> 7)));
    }
    put(track, (unsigned char)(delta & 0x7f));
}

static void put_name(Track *track, const char *name) {
    put(track, 0);
    put(track, 0xff);
    put(track, 0x03);
    put(track, (unsigned char)strlen(name));
    for (const char *c = name; *c; c++) {
        put(track, (unsigned char)*c);
    }
}

static void put_meter(Track *track) {
    const unsigned char meter[] = {0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08};
    for (size_t i = 0; i < sizeof meter; i++) {
        put(track, meter[i]);
    }
}

static void put_program(Track *track, int channel, int program) {
    put(track, 0);
    put(track, (unsigned char)(0xc0 | channel));
    put(track, (unsigned char)program);
}

/* A note starting delta ticks after the previous event and lasting 240 ticks. */
static void put_note(Track *track, unsigned delta, int channel, int pitch) {
    put_delta(track, delta);
    put(track, (unsigned char)(0x90 | channel));
    put(track, (unsigned char)pitch);
    put(track, 80);
    put_delta(track, 240);
    put(track, (unsigned char)(0x80 | channel));
    put(track, (unsigned char)pitch);
    put(track, 0);
}

static void put_end(Track *track) {
    put(track, 0);
    put(track, 0xff);
    put(track, 0x2f);
    put(track, 0);
}

static void write_file(const char *path, const Track *tracks, size_t track_count) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    const unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, (unsigned char)track_count, 0x01, 0xe0};
    assert(fwrite(header, 1, sizeof header, file) == sizeof header);
    for (size_t t = 0; t < track_count; t++) {
        const unsigned char chunk[] = {'M', 'T', 'r', 'k', 0, 0, (unsigned char)(tracks[t].size >> 8),
                                       (unsigned char)(tracks[t].size & 0xff)};
        assert(fwrite(chunk, 1, sizeof chunk, file) == sizeof chunk);
        assert(fwrite(tracks[t].bytes, 1, tracks[t].size, file) == tracks[t].size);
    }
    fclose(file);
}

static void expect_part(const ImportedNotes *notes, size_t part, const char *id, const char *name, int program,
                        int staff_count) {
    const ImportedPart *p = &notes->source.parts[part];
    assert(strcmp(p->id, id) == 0);
    assert(strcmp(p->name, name) == 0);
    assert(p->midi_program == program);
    assert(p->staff_count == staff_count);
}

int main(void) {
    Track tracks[5] = {{{0}, 0}};

    /* Two channels in one track, the track is named: the name and the instrument. */
    put_name(&tracks[0], "Strings");
    put_meter(&tracks[0]);
    put_program(&tracks[0], 0, 40);
    put_program(&tracks[0], 1, 42);
    put_note(&tracks[0], 0, 0, 72);
    put_note(&tracks[0], 240, 1, 55);
    put_end(&tracks[0]);

    /* One channel in a named track: the name alone. */
    put_name(&tracks[1], "Lead");
    put_program(&tracks[1], 0, 73);
    put_note(&tracks[1], 0, 0, 79);
    put_end(&tracks[1]);

    /* The percussion channel, then a piano that never sends a program change, in unnamed tracks. */
    put_note(&tracks[2], 0, 9, 36);
    put_end(&tracks[2]);

    put_note(&tracks[3], 0, 2, 48);
    put_note(&tracks[3], 0, 2, 76);
    put_end(&tracks[3]);

    /* A melodic channel whose program change comes before its first note. */
    put_program(&tracks[4], 3, 56);
    put_note(&tracks[4], 0, 3, 62);
    put_end(&tracks[4]);

    const char *path = "test_midi_parts.mid";
    write_file(path, tracks, 5);
    ImportedNotes notes;
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    remove(path);

    assert(notes.source.format == INPUT_FORMAT_MIDI);
    assert(notes.source.part_count == 6);
    expect_part(&notes, 0, "P1", "Strings - Violin", 40, 1);
    expect_part(&notes, 1, "P2", "Strings - Cello", 42, 1);
    expect_part(&notes, 2, "P3", "Lead", 73, 1);
    expect_part(&notes, 3, "P4", "Percussion", -1, 1);
    expect_part(&notes, 4, "P5", "Acoustic Grand Piano", 0, 2);
    expect_part(&notes, 5, "P6", "Trumpet", 56, 1);

    assert(notes.count == 7);
    assert(notes.source.part_index != NULL && notes.source.staff != NULL);
    for (size_t i = 0; i < notes.count; i++) {
        long pitch = pitch_numeric_value_position_numerator(notes.pitch_values[i]);
        size_t expected_part = 0;
        int expected_staff = 1;
        switch (pitch) {
            case 72: expected_part = 0; break;
            case 55: expected_part = 1; break;
            case 79: expected_part = 2; break;
            case 36: expected_part = 3; break;
            case 48: expected_part = 4; expected_staff = 2; break;
            case 76: expected_part = 4; break;
            case 62: expected_part = 5; break;
            default: assert(!"unexpected pitch");
        }
        assert(notes.streams[i] == expected_part);
        assert(notes.source.part_index[i] == expected_part);
        assert(notes.source.staff[i] == expected_staff);
    }
    imported_notes_destroy(&notes);

    /* A set-tempo event at tick 0 (400000 microseconds per quarter, 150 per minute) is the declared opening tempo; one
       that first comes later leaves the opening undeclared. */
    const unsigned char tempo[] = {0x00, 0xff, 0x51, 0x03, 0x06, 0x1a, 0x80};
    Track tempo_track = {{0}, 0};
    for (size_t i = 0; i < sizeof tempo; i++) {
        put(&tempo_track, tempo[i]);
    }
    put_note(&tempo_track, 0, 0, 60);
    put_end(&tempo_track);
    write_file(path, &tempo_track, 1);
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    remove(path);
    assert(notes.source.tempo_declared);
    assert(notes.source.initial_quarters_per_minute > 149.99 && notes.source.initial_quarters_per_minute < 150.01);
    imported_notes_destroy(&notes);

    Track late_track = {{0}, 0};
    put_note(&late_track, 0, 0, 60);
    put_delta(&late_track, 480);
    for (size_t i = 1; i < sizeof tempo; i++) {
        put(&late_track, tempo[i]);
    }
    put_end(&late_track);
    write_file(path, &late_track, 1);
    assert(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK);
    remove(path);
    assert(!notes.source.tempo_declared);
    imported_notes_destroy(&notes);
    puts("test_midi_parts: ok");
    return 0;
}
