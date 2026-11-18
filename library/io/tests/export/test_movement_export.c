#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_renderer.h"
#include "io/movement_export.h"
#include "io/musicxml_export.h"
#include "io/smf_reader.h"
#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/tuning_system.h"
#include "model/time/time_span.h"
#include "realization/piano_reduction.h"

/*
 * Movement export: a movement with a melody and a bass line, reduced to piano, exports one track named "Piano" with channels 0 and 1
 * and one part of two staves with the performer note ids as xml:ids, its notes equal to an export of the same notes as two voice
 * tracks; a pedal in both formats; a work's title; the refusals.
 */

static MusicalTime at(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static Duration length(long long n, long long d) {
    Duration v;
    assert(duration_make(n, d, &v) == DURATION_OK);
    return v;
}

static TimeSpan span(long long a, long long b, long long c, long long d) {
    TimeSpan s;
    s.start = at(a, b);
    s.end = at(c, d);
    return s;
}

static void add_note(Line *line, const char *id, long long onset_num, long long onset_den, long long dur_num, long long dur_den, int midi,
                     bool spelled, PitchLetter letter, int accidental) {
    LineNote note;
    memset(&note, 0, sizeof(note));
    strcpy(note.id, id);
    note.onset = at(onset_num, onset_den);
    note.duration = length(dur_num, dur_den);
    note.midi = midi;
    note.spelled = spelled;
    note.letter = letter;
    note.accidental = accidental;
    assert(line_add_note(line, &note) == REALIZATION_OK);
}

static const Instrument PIANO = {"omt.instrument.piano", "Piano", "keyboard", 0, 2, {INSTRUMENT_CLEF_TREBLE, INSTRUMENT_CLEF_BASS}, 21, 108,
                                 false, 0, 0, 0, true};

/* melody: C5 D5 E5 F5 on the quarters of bar 1; bass: C3 and G3 on the halves. */
static Movement *make_movement(const char *title) {
    Movement *m = NULL;
    assert(movement_create("m1", title, &m) == REALIZATION_OK);
    Line *melody = NULL, *bass = NULL;
    assert(line_create("melody", LINE_ROLE_MELODY, "Melody", &melody) == REALIZATION_OK);
    assert(line_create("bass", LINE_ROLE_BASS, "Bass", &bass) == REALIZATION_OK);
    add_note(melody, "n1", 0, 1, 1, 4, 72, true, PITCH_LETTER_C, 0);
    add_note(melody, "n2", 1, 4, 1, 4, 74, true, PITCH_LETTER_D, 0);
    add_note(melody, "n3", 1, 2, 1, 4, 76, true, PITCH_LETTER_E, 0);
    add_note(melody, "n4", 3, 4, 1, 4, 77, true, PITCH_LETTER_F, 0);
    add_note(bass, "b1", 0, 1, 1, 2, 48, true, PITCH_LETTER_C, 0);
    add_note(bass, "b2", 1, 2, 1, 2, 55, true, PITCH_LETTER_G, 0);
    assert(movement_add_line(m, melody) == REALIZATION_OK && movement_add_line(m, bass) == REALIZATION_OK);
    assert(movement_reduce_to_piano(m, &PIANO, "r") == REALIZATION_OK);
    return m;
}

/* --- reading what was written --- */

typedef struct {
    unsigned channel;
    uint32_t on, off;
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

static uint32_t vlq(const SmfTrack *track, size_t *position) {
    uint32_t value = 0;
    for (;;) {
        unsigned char byte = track->data[(*position)++];
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return value;
        }
    }
}

typedef struct {
    uint32_t tick;
    unsigned char status, first, second;
    int meta;
    char text[64];
} Ev;

static size_t events_of(const SmfTrack *track, Ev *out) {
    size_t position = 0, count = 0;
    uint32_t tick = 0;
    while (position < track->size) {
        tick += vlq(track, &position);
        Ev *e = &out[count++];
        memset(e, 0, sizeof(*e));
        e->tick = tick;
        e->meta = -1;
        e->status = track->data[position++];
        if (e->status == 0xFF) {
            e->meta = track->data[position++];
            uint32_t n = vlq(track, &position);
            if (n < sizeof(e->text)) {
                memcpy(e->text, track->data + position, n);
            }
            position += n;
        } else if ((e->status & 0xF0) == 0xC0) {
            e->first = track->data[position++];
        } else {
            e->first = track->data[position++];
            e->second = track->data[position++];
        }
    }
    return count;
}

static size_t played_of(const SmfTrack *tracks, size_t first, size_t count, Played *out) {
    size_t total = 0;
    for (size_t t = first; t < count; t++) {
        Ev events[128];
        size_t n = events_of(&tracks[t], events);
        for (size_t i = 0; i < n; i++) {
            if ((events[i].status & 0xF0) != 0x90) {
                continue;
            }
            out[total].channel = events[i].status & 0x0F;
            out[total].on = events[i].tick;
            out[total].key = events[i].first;
            for (size_t j = i + 1; j < n; j++) {
                if ((events[j].status & 0xF0) == 0x80 && events[j].first == events[i].first && (events[j].status & 0x0F) == out[total].channel) {
                    out[total].off = events[j].tick;
                    break;
                }
            }
            total++;
        }
    }
    qsort(out, total, sizeof(Played), compare_played);
    return total;
}

/* The same notes as two voice tracks: the pre-parts layout. */
static unsigned char *voice_track_midi(size_t *size) {
    static const struct {
        long onset_n, onset_d, dur_d;
        int midi;
        const char *voice;
    } NOTES[6] = {{0, 1, 4, 72, "v1"}, {1, 4, 4, 74, "v1"}, {1, 2, 4, 76, "v1"}, {3, 4, 4, 77, "v1"}, {0, 1, 2, 48, "v2"}, {1, 2, 2, 55, "v2"}};
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    assert(entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK && tuning_system_create(tuning_id, "MIDI", &tuning) == TUNING_SYSTEM_OK);
    NoteEvent *events[6];
    PitchNumericValue *values[6];
    for (int i = 0; i < 6; i++) {
        char id[8];
        snprintf(id, sizeof(id), "e%d", i);
        EntityID *eid = NULL, *vid = NULL;
        assert(entity_id_create(id, &eid) == ENTITY_ID_OK && entity_id_create(NOTES[i].voice, &vid) == ENTITY_ID_OK);
        VoiceReference *voice = NULL;
        assert(voice_reference_create(vid, &voice) == VOICE_REFERENCE_OK);
        assert(pitch_numeric_value_create(tuning, NOTES[i].midi, 1, &values[i]) == PITCH_NUMERIC_VALUE_OK);
        PitchIdentity *identity = NULL;
        assert(pitch_numeric_value_to_identity(values[i], &identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
        ProvenanceReference *provenance = NULL;
        assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
        assert(note_event_create(eid, at(NOTES[i].onset_n, NOTES[i].onset_d), length(1, NOTES[i].dur_d), identity, NULL, voice, provenance,
                                 &events[i]) == NOTE_EVENT_OK);
    }
    unsigned char *data = NULL;
    assert(midi_renderer_render_with_options(events, values, 6, NULL, NULL, NULL, 0, NULL, 480, &data, size) == MIDI_RENDER_OK);
    for (int i = 0; i < 6; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    tuning_system_destroy(tuning);
    return data;
}

static size_t count_of(const char *haystack, const char *needle) {
    size_t n = 0;
    for (const char *p = strstr(haystack, needle); p != NULL; p = strstr(p + strlen(needle), needle)) {
        n++;
    }
    return n;
}

/* The (step, alter, octave, duration) of every <note> in order, as "C0 4 2" strings, into out. */
static size_t note_facts(const char *xml, char out[64][24]) {
    size_t n = 0;
    for (const char *note = strstr(xml, "<note"); note != NULL && n < 64; note = strstr(note + 1, "<note")) {
        const char *end = strstr(note, "</note>");
        const char *step = strstr(note, "<step>"), *octave = strstr(note, "<octave>"), *duration = strstr(note, "<duration>");
        if (step == NULL || step > end) {
            continue;
        }
        const char *alter = strstr(note, "<alter>");
        snprintf(out[n], 24, "%c%d o%d d%d", step[6], alter != NULL && alter < end ? atoi(alter + 7) : 0, atoi(octave + 8), atoi(duration + 10));
        n++;
    }
    return n;
}

static int compare_facts(const void *left, const void *right) {
    return strcmp((const char *)left, (const char *)right);
}

static void test_midi(void) {
    Movement *m = make_movement("Allegro");
    unsigned char *data = NULL;
    size_t size = 0;
    char message[128];
    assert(movement_export_midi(m, NULL, &data, &size, message, sizeof(message)) == MOVEMENT_EXPORT_OK && message[0] == '\0');
    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    assert(smf_reader_parse(data, size, &header, &tracks, &track_count) == SMF_READER_OK);
    assert(header.format == 1 && header.division == 480 && track_count == 2);
    Ev events[128];
    size_t n = events_of(&tracks[0], events);
    assert(events[0].meta == 0x03 && strcmp(events[0].text, "Allegro") == 0 && n >= 2);
    n = events_of(&tracks[1], events);
    assert(events[0].meta == 0x03 && strcmp(events[0].text, "Piano") == 0);
    assert(events[1].status == 0xC0 && events[1].first == 0 && events[2].status == 0xC1 && events[2].first == 0);
    /* the notes per channel equal those of the same notes as two voice tracks */
    size_t plain_size = 0;
    unsigned char *plain = voice_track_midi(&plain_size);
    SmfHeader plain_header;
    SmfTrack *plain_tracks = NULL;
    size_t plain_count = 0;
    assert(smf_reader_parse(plain, plain_size, &plain_header, &plain_tracks, &plain_count) == SMF_READER_OK && plain_count == 3);
    Played a[16], b[16];
    size_t na = played_of(tracks, 1, track_count, a), nb = played_of(plain_tracks, 1, plain_count, b);
    assert(na == 6 && nb == 6 && memcmp(a, b, sizeof(Played) * 6) == 0);
    assert(a[0].channel == 0 && a[5].channel == 1);

    /* without track names: none, and the program changes stay */
    unsigned char *bare = NULL;
    size_t bare_size = 0;
    MovementMidiOptions no_names = {false};
    assert(movement_export_midi(m, &no_names, &bare, &bare_size, NULL, 0) == MOVEMENT_EXPORT_OK);
    SmfTrack *bare_tracks = NULL;
    size_t bare_count = 0;
    assert(smf_reader_parse(bare, bare_size, &header, &bare_tracks, &bare_count) == SMF_READER_OK && bare_count == 2);
    n = events_of(&bare_tracks[1], events);
    assert(events[0].status == 0xC0 && events[0].meta == -1);
    n = events_of(&bare_tracks[0], events);
    for (size_t i = 0; i < n; i++) {
        assert(events[i].meta != 0x03);
    }
    free(bare_tracks);
    free(bare);
    free(plain_tracks);
    free(plain);
    free(tracks);
    free(data);
    movement_destroy(m);

    /* a movement with no title is named "piece" */
    Movement *untitled = make_movement(NULL);
    assert(movement_export_midi(untitled, NULL, &data, &size, NULL, 0) == MOVEMENT_EXPORT_OK);
    assert(smf_reader_parse(data, size, &header, &tracks, &track_count) == SMF_READER_OK);
    events_of(&tracks[0], events);
    assert(events[0].meta == 0x03 && strcmp(events[0].text, "piece") == 0);
    free(tracks);
    free(data);
    movement_destroy(untitled);
}

static void test_musicxml(void) {
    Movement *m = make_movement("Allegro");
    Work *work = NULL;
    assert(work_create("w1", "Sonata", "Anon", &work) == REALIZATION_OK);
    char *xml = NULL;
    char message[128];
    assert(movement_export_musicxml(NULL, m, &xml, message, sizeof(message)) == MOVEMENT_EXPORT_OK && message[0] == '\0');
    assert(count_of(xml, "<part id=\"") == 1 && strstr(xml, "<staves>2</staves>") != NULL && strstr(xml, "<work-title>") == NULL);
    assert(strstr(xml, "xml:id=\"piano-n1\"") != NULL && strstr(xml, "xml:id=\"piano-b2\"") != NULL);
    assert(count_of(xml, "<staff>1</staff>") >= 4 && count_of(xml, "<staff>2</staff>") >= 2);
    assert(strstr(xml, "<part-name>Piano</part-name>") != NULL && strstr(xml, "<midi-program>1</midi-program>") != NULL);
    char (*facts)[24] = calloc(64, 24);
    size_t count = note_facts(xml, facts);
    assert(count == 6);
    qsort(facts, count, 24, compare_facts);
    /* the same notes as two voices of one part exported directly: pitch, octave and duration agree */
    static const char *const expected[6] = {"C0 o3 d2", "C0 o5 d1", "D0 o5 d1", "E0 o5 d1", "F0 o5 d1", "G0 o3 d2"};
    char (*sorted)[24] = calloc(6, 24);
    for (int i = 0; i < 6; i++) {
        strcpy(sorted[i], expected[i]);
    }
    qsort(sorted, 6, 24, compare_facts);
    for (size_t i = 0; i < 6; i++) {
        assert(strcmp(facts[i], sorted[i]) == 0);
    }
    free(sorted);
    free(facts);
    free(xml);

    /* a work: title and composer */
    assert(movement_export_musicxml(work, m, &xml, NULL, 0) == MOVEMENT_EXPORT_OK);
    assert(strstr(xml, "<work-title>Sonata</work-title>") != NULL && strstr(xml, "<creator type=\"composer\">Anon</creator>") != NULL);
    free(xml);
    work_destroy(work);
    movement_destroy(m);
}

static void test_pedal_in_both_formats(void) {
    Movement *m = make_movement("Allegro");
    PedalMarking pedal = {PEDAL_SUSTAIN, span(0, 1, 1, 2)};
    assert(performer_part_add_pedal(movement_part_named(m, "piano"), &pedal) == REALIZATION_OK);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(movement_export_midi(m, NULL, &data, &size, NULL, 0) == MOVEMENT_EXPORT_OK);
    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    assert(smf_reader_parse(data, size, &header, &tracks, &track_count) == SMF_READER_OK);
    Ev events[128];
    size_t n = events_of(&tracks[1], events), down = 0, up = 0;
    for (size_t i = 0; i < n; i++) {
        if ((events[i].status & 0xF0) == 0xB0 && events[i].first == 64) {
            if (events[i].second == 127) {
                assert(events[i].tick == 0);
                down++;
            } else {
                assert(events[i].second == 0 && events[i].tick == 960);
                up++;
            }
        }
    }
    assert(down == 2 && up == 2); /* both channels */
    free(tracks);
    free(data);
    char *xml = NULL;
    assert(movement_export_musicxml(NULL, m, &xml, NULL, 0) == MOVEMENT_EXPORT_OK);
    assert(count_of(xml, "<pedal type=\"start\" line=\"yes\"/>") == 1 && count_of(xml, "<pedal type=\"stop\" line=\"yes\"/>") == 1);
    const char *start = strstr(xml, "<pedal type=\"start\"");
    assert(strncmp(strstr(start, "</direction-type>") + strlen("</direction-type>\n"), "        <staff>2</staff>", 22) == 0); /* under the bass staff */
    free(xml);
    movement_destroy(m);
}

static void test_unspelled_notes_use_sharps(void) {
    Movement *m = NULL;
    assert(movement_create("m1", NULL, &m) == REALIZATION_OK);
    Line *line = NULL;
    assert(line_create("melody", LINE_ROLE_MELODY, NULL, &line) == REALIZATION_OK);
    add_note(line, "n1", 0, 1, 1, 4, 61, false, PITCH_LETTER_C, 0);
    add_note(line, "n2", 1, 4, 1, 4, 70, false, PITCH_LETTER_C, 0);
    assert(movement_add_line(m, line) == REALIZATION_OK && movement_reduce_to_piano(m, &PIANO, "") == REALIZATION_OK);
    char *xml = NULL;
    assert(movement_export_musicxml(NULL, m, &xml, NULL, 0) == MOVEMENT_EXPORT_OK);
    assert(strstr(xml, "<step>C</step>\n          <alter>1</alter>\n          <octave>4</octave>") != NULL);
    assert(strstr(xml, "<step>A</step>\n          <alter>1</alter>\n          <octave>4</octave>") != NULL);
    assert(strstr(xml, "<staves>") == NULL); /* a motif stays on one staff */
    free(xml);
    movement_destroy(m);
}

static void test_refusals(void) {
    Movement *m = make_movement("Allegro");
    unsigned char *data = (unsigned char *)(uintptr_t)1;
    char *xml = (char *)(uintptr_t)1;
    size_t size = 0;
    char message[128];
    assert(movement_export_midi(NULL, NULL, &data, &size, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NULL);
    assert(movement_export_midi(m, NULL, NULL, &size, NULL, 0) == MOVEMENT_EXPORT_ERR_NULL);
    assert(movement_export_musicxml(NULL, NULL, &xml, NULL, 0) == MOVEMENT_EXPORT_ERR_NULL);
    assert(movement_add_played_span(m, span(0, 1, 1, 1)) == REALIZATION_OK);
    assert(movement_export_midi(m, NULL, &data, &size, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NOT_BUILT && strstr(message, "played") != NULL);
    assert(movement_export_musicxml(NULL, m, &xml, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NOT_BUILT);
    assert(data == (unsigned char *)(uintptr_t)1 && xml == (char *)(uintptr_t)1);
    movement_destroy(m);

    Movement *empty = NULL;
    assert(movement_create("m2", NULL, &empty) == REALIZATION_OK);
    assert(movement_export_midi(empty, NULL, &data, &size, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NO_NOTES);
    assert(movement_export_musicxml(NULL, empty, &xml, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NO_NOTES);
    movement_destroy(empty);

    /* a time MIDI cannot write: a whole-note fraction whose denominator needs more than the largest division */
    Movement *fine = NULL;
    assert(movement_create("m3", NULL, &fine) == REALIZATION_OK);
    Line *line = NULL;
    assert(line_create("melody", LINE_ROLE_MELODY, NULL, &line) == REALIZATION_OK);
    add_note(line, "n1", 1, 40001, 1, 4, 60, false, PITCH_LETTER_C, 0);
    assert(movement_add_line(fine, line) == REALIZATION_OK && movement_reduce_to_piano(fine, &PIANO, "") == REALIZATION_OK);
    assert(movement_export_midi(fine, NULL, &data, &size, message, sizeof(message)) == MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE);
    movement_destroy(fine);
}

int main(void) {
    test_midi();
    test_musicxml();
    test_pedal_in_both_formats();
    test_unspelled_notes_use_sharps();
    test_refusals();
    printf("test_movement_export: all tests passed\n");
    return 0;
}
