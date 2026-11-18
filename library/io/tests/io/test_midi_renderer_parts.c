#include "io/tests/io/midi_render_test_util.h"

/*
 * Performer part tracks (MidiRenderOptions.parts): two voices in one part give one track with two channels in first-sound order and
 * a program change on each; the notes per channel equal a render with no parts; controller 64 at a pedal's ticks on every channel of
 * the part and the order of the events at an equal tick; the refusals; a part with no notes still has its track.
 */

typedef struct {
    uint32_t tick;
    unsigned char status;
    unsigned char first;
    unsigned char second;
    int meta; /* the meta type, or -1 */
    char text[64];
} Ev;

/* Reads a track's events into out (at most 256). */
static size_t read_events(const SmfTrack *track, Ev *out) {
    size_t position = 0, count = 0;
    uint32_t tick = 0;
    while (position < track->size) {
        tick += test_read_vlq(track, &position);
        Ev *e = &out[count++];
        memset(e, 0, sizeof(*e));
        e->tick = tick;
        e->meta = -1;
        unsigned char status = track->data[position++];
        e->status = status;
        if (status == 0xFF) {
            e->meta = track->data[position++];
            uint32_t length = test_read_vlq(track, &position);
            if (length < sizeof(e->text)) {
                memcpy(e->text, track->data + position, length);
            }
            position += length;
        } else if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
            e->first = track->data[position++];
        } else {
            e->first = track->data[position++];
            e->second = track->data[position++];
        }
    }
    return count;
}

typedef struct {
    unsigned channel;
    uint32_t on;
    uint32_t off;
    unsigned key;
} Played;

static int compare_played(const void *left, const void *right) {
    const Played *a = left, *b = right;
    if (a->channel != b->channel) {
        return a->channel < b->channel ? -1 : 1;
    }
    if (a->on != b->on) {
        return a->on < b->on ? -1 : 1;
    }
    return a->key < b->key ? -1 : (a->key > b->key);
}

/* Every (channel, on, off, key) of the tracks from `first` on, sorted. */
static size_t played_notes(const TestSmf *smf, size_t first, Played *out) {
    size_t count = 0;
    for (size_t t = first; t < smf->track_count; t++) {
        Ev events[256];
        size_t n = read_events(&smf->tracks[t], events);
        for (size_t i = 0; i < n; i++) {
            if ((events[i].status & 0xF0) != 0x90) {
                continue;
            }
            out[count].channel = events[i].status & 0x0F;
            out[count].on = events[i].tick;
            out[count].key = events[i].first;
            for (size_t j = i + 1; j < n; j++) {
                if ((events[j].status & 0xF0) == 0x80 && events[j].first == events[i].first &&
                    (events[j].status & 0x0F) == out[count].channel) {
                    out[count].off = events[j].tick;
                    break;
                }
            }
            count++;
        }
    }
    qsort(out, count, sizeof(Played), compare_played);
    return count;
}

static MusicalTime t(long long n, long long d) {
    return test_time_value(n, d);
}

static TimeSpan span(long long a, long long b, long long c, long long d) {
    TimeSpan s;
    s.start = t(a, b);
    s.end = t(c, d);
    return s;
}

static MidiRenderResult render(const TestNotes *notes, const MidiRenderOptions *options, unsigned char **data, size_t *size) {
    return midi_renderer_render_with_options(notes->events, notes->pitches, notes->count, NULL, NULL, NULL, 0, options, 480, data, size);
}

static void test_one_part_two_voices(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "va"); /* voice a sounds first: channel 0 */
    test_notes_add(&notes, 1, 4, 1, 4, 64, "va");
    test_notes_add(&notes, 1, 8, 1, 2, 55, "vb"); /* voice b: channel 1 */
    const char *voices[2] = {"vb", "va"};         /* the part lists them in another order: channels still follow the first sound */
    MidiRenderPart part = {"Piano", 0, voices, 2, NULL, 0};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.parts = &part;
    options.part_count = 1;
    unsigned char *data = NULL, *plain = NULL;
    size_t size = 0, plain_size = 0;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    assert(render(&notes, NULL, &plain, &plain_size) == MIDI_RENDER_OK);
    TestSmf smf, plain_smf;
    test_smf_parse(data, size, &smf);
    test_smf_parse(plain, plain_size, &plain_smf);
    assert(smf.track_count == 2 && plain_smf.track_count == 3);
    Ev events[256];
    size_t n = read_events(&smf.tracks[1], events);
    /* the name, then a program change on each channel, then the notes */
    assert(events[0].meta == 0x03 && strcmp(events[0].text, "Piano") == 0 && events[0].tick == 0);
    assert(events[1].status == 0xC0 && events[1].first == 0 && events[2].status == 0xC1 && events[2].first == 0);
    size_t note_ons = 0;
    for (size_t i = 3; i < n; i++) {
        note_ons += (events[i].status & 0xF0) == 0x90;
    }
    assert(note_ons == 3);
    /* the notes per channel are those of a render with no parts */
    Played with_parts[16], without[16];
    size_t a = played_notes(&smf, 1, with_parts), b = played_notes(&plain_smf, 1, without);
    assert(a == 3 && b == 3 && memcmp(with_parts, without, sizeof(Played) * 3) == 0);
    free(smf.tracks);
    free(plain_smf.tracks);
    free(data);
    free(plain);
    test_notes_free(&notes);
}

static void test_two_parts_channels_and_programs(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 1, 2, 1, 4, 60, "x"); /* part A's voice sounds second */
    test_notes_add(&notes, 0, 1, 1, 4, 72, "y"); /* part B's voice sounds first: channel 0 */
    const char *a_voices[1] = {"x"};
    const char *b_voices[1] = {"y"};
    MidiRenderPart parts[2] = {{"Strings", 48, a_voices, 1, NULL, 0}, {NULL, 73, b_voices, 1, NULL, 0}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.parts = parts;
    options.part_count = 2;
    options.conductor_name = "Piece";
    unsigned char *data = NULL;
    size_t size = 0;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 3);
    Ev events[256];
    read_events(&smf.tracks[1], events); /* part A: name, then program 48 on channel 1 */
    assert(events[0].meta == 0x03 && strcmp(events[0].text, "Strings") == 0);
    assert(events[1].status == 0xC1 && events[1].first == 48 && events[2].status == 0x91);
    read_events(&smf.tracks[2], events); /* part B: no name; program 73 on channel 0 */
    assert(events[0].status == 0xC0 && events[0].first == 73 && events[1].status == 0x90 && events[1].first == 72);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

static void test_pedal(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "va");
    test_notes_add(&notes, 1, 4, 1, 4, 62, "va");
    test_notes_add(&notes, 1, 4, 1, 4, 48, "vb");
    const char *voices[2] = {"va", "vb"};
    TimeSpan pedals[2] = {span(0, 1, 1, 4), span(1, 4, 1, 2)};
    MidiRenderPart part = {"Piano", 0, voices, 2, pedals, 2};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.parts = &part;
    options.part_count = 1;
    unsigned char *data = NULL;
    size_t size = 0;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    Ev events[256];
    size_t n = read_events(&smf.tracks[1], events);
    size_t down = 0, up = 0;
    for (size_t i = 0; i < n; i++) {
        if ((events[i].status & 0xF0) == 0xB0 && events[i].first == 64) {
            if (events[i].second == 127) {
                down++;
            } else {
                up++;
            }
        }
    }
    assert(down == 4 && up == 4); /* two spans on two channels */
    /* at tick 480: the note-off, then pedal up, then pedal down, then the note-ons */
    size_t at = 0;
    while (at < n && events[at].tick != 480) {
        at++;
    }
    assert((events[at].status & 0xF0) == 0x80);
    size_t k = at;
    while ((events[k].status & 0xF0) == 0x80) {
        k++;
    }
    assert((events[k].status & 0xF0) == 0xB0 && events[k].second == 0 && events[k + 1].second == 0);
    assert((events[k + 2].status & 0xF0) == 0xB0 && events[k + 2].second == 127 && events[k + 3].second == 127);
    assert((events[k + 4].status & 0xF0) == 0x90);
    /* the pedal on both channels at tick 0, down; and up at 960 */
    assert(events[3].tick == 0 && (events[3].status & 0xF0) == 0xB0 + 0);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

static void test_refusals(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "va");
    test_notes_add(&notes, 0, 1, 1, 4, 64, "vb");
    unsigned char *data = NULL;
    size_t size = 0;
    const char *both[2] = {"va", "vb"}, *only_a[1] = {"va"}, *only_b[1] = {"vb"};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));

    /* a voice in two parts */
    MidiRenderPart twice[2] = {{"A", 0, both, 2, NULL, 0}, {"B", 0, only_b, 1, NULL, 0}};
    options.parts = twice;
    options.part_count = 2;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE && data == NULL);
    /* a voice no part lists */
    MidiRenderPart lacking = {"A", 0, only_a, 1, NULL, 0};
    options.parts = &lacking;
    options.part_count = 1;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    /* a program outside 0 to 127 */
    MidiRenderPart high = {"A", 128, both, 2, NULL, 0};
    options.parts = &high;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    high.program = -1;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    /* a pedal whose tick is not whole at 480 (1/7 of a whole note) */
    TimeSpan odd = span(1, 7, 1, 2);
    MidiRenderPart pedaled = {"A", 0, both, 2, &odd, 1};
    options.parts = &pedaled;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    /* NULL arrays */
    MidiRenderPart null_voices = {"A", 0, NULL, 2, NULL, 0};
    options.parts = &null_voices;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NULL_ARG);
    options.parts = NULL;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NULL_ARG);
    test_notes_free(&notes);

    /* a note with no voice */
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);
    options.parts = &pedaled;
    options.part_count = 1;
    pedaled.pedal_count = 0;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    /* a same-pitch overlap in one voice is refused, on two channels of one part it is not */
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 2, 60, "va");
    test_notes_add(&notes, 1, 4, 1, 2, 60, "va");
    options.parts = &lacking;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 2, 60, "va");
    test_notes_add(&notes, 1, 4, 1, 2, 60, "vb");
    options.parts = &pedaled;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    free(data);
    data = NULL;
    test_notes_free(&notes);

    /* more than 15 channels */
    test_notes_init(&notes);
    char names[16][8];
    const char *voices[16];
    for (int i = 0; i < 16; i++) {
        snprintf(names[i], sizeof(names[i]), "v%d", i);
        voices[i] = names[i];
        test_notes_add(&notes, i, 16, 1, 16, 60 + i, names[i]);
    }
    MidiRenderPart wide = {"Wide", 0, voices, 16, NULL, 0};
    options.parts = &wide;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_ERR_TOO_MANY_VOICES && data == NULL);
    wide.voice_count = 15;
    notes.count = 15; /* the sixteenth voice goes unplayed */
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    Ev events[256];
    size_t n = read_events(&smf.tracks[1], events);
    unsigned programs = 0, channel9 = 0;
    for (size_t i = 0; i < n; i++) {
        programs += (events[i].status & 0xF0) == 0xC0;
        channel9 += (events[i].status & 0x0F) == 9 && (events[i].status & 0xF0) != 0xFF;
    }
    assert(programs == 15 && channel9 == 0);
    free(smf.tracks);
    free(data);
    notes.count = 16;
    test_notes_free(&notes);
}

static void test_part_without_notes_has_a_track(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "va");
    const char *a_voices[1] = {"va"};
    const char *none[1] = {"unused"};
    MidiRenderPart parts[2] = {{"A", 0, a_voices, 1, NULL, 0}, {"Empty", 0, none, 1, NULL, 0}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.parts = parts;
    options.part_count = 2;
    unsigned char *data = NULL;
    size_t size = 0;
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 3);
    Ev events[16];
    size_t n = read_events(&smf.tracks[2], events);
    assert(n == 2 && events[0].meta == 0x03 && strcmp(events[0].text, "Empty") == 0 && events[1].meta == 0x2F);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
    /* parts with no notes at all: one track per part */
    test_notes_init(&notes);
    assert(render(&notes, &options, &data, &size) == MIDI_RENDER_OK);
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 3);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

int main(void) {
    test_one_part_two_voices();
    test_two_parts_channels_and_programs();
    test_pedal();
    test_refusals();
    test_part_without_notes_has_a_track();
    printf("test_midi_renderer_parts: all tests passed\n");
    return 0;
}
