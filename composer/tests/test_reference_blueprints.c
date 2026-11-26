#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Gate 2 of P69b (R-740, docs/blueprints.md): the full blueprint of each reference theme (fixtures/references) through the theme API at
 * seed 1, composed as `ursatz theme <file> --seed 1 --midi <file>` composes it, renders a MIDI file whose notes equal the reference
 * MIDI's. A note is its onset, duration and pitch in quarter notes, as compare_notes.py reads them: channels, tracks, velocities, tempo
 * and controllers are ignored, and a note_on is paired with the next note_off (or note_on at velocity 0) of its channel and pitch.
 * ref-6-holst and ref-7-debussy have no blueprint: planed chords move to P70 and Debussy is outside gate (a) (R-742).
 */

typedef struct {
    const char *name;
    size_t notes; /* the reference's notes, measured with compare_notes.py */
} Reference;

static const Reference REFERENCES[] = {
    {"hand-A", 93},      {"hand-B", 89},  {"ref-1-megaman", 107},      {"ref-2-ff7", 90},
    {"ref-3-banjo", 96}, {"ref-4-hollowknight", 69}, {"ref-5-shostakovich", 105},
};
#define REFERENCE_COUNT (sizeof REFERENCES / sizeof REFERENCES[0])

#define NOTES_MAX 512
#define OPEN_MAX 256

typedef struct {
    long long onset; /* in ticks times the other file's division, so that two files compare exactly */
    long long duration;
    int pitch;
} Note;

typedef struct {
    int channel;
    int pitch;
    long long tick;
} Open;

static uint32_t big_endian(const uint8_t *bytes, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count; i++) {
        value = value << 8 | bytes[i];
    }
    return value;
}

/* A variable-length quantity at *at, which it moves past. */
static uint32_t read_vlq(const uint8_t *data, size_t end, size_t *at) {
    uint32_t value = 0;
    uint8_t byte;
    do {
        assert(*at < end);
        byte = data[(*at)++];
        value = value << 7 | (byte & 0x7f);
    } while (byte & 0x80);
    return value;
}

/* Closes the earliest open note of channel and pitch at tick, if there is one. */
static void close_note(Open *open, size_t *open_count, int channel, int pitch, long long tick, long long scale, Note *out,
                       size_t *count) {
    for (size_t i = 0; i < *open_count; i++) {
        if (open[i].channel == channel && open[i].pitch == pitch) {
            assert(*count < NOTES_MAX);
            out[*count].onset = open[i].tick * scale;
            out[*count].duration = (tick - open[i].tick) * scale;
            out[*count].pitch = pitch;
            (*count)++;
            memmove(&open[i], &open[i + 1], (*open_count - i - 1) * sizeof(Open));
            (*open_count)--;
            return;
        }
    }
}

static int by_note(const void *a, const void *b) {
    const Note *x = a;
    const Note *y = b;
    if (x->onset != y->onset) {
        return x->onset < y->onset ? -1 : 1;
    }
    if (x->duration != y->duration) {
        return x->duration < y->duration ? -1 : 1;
    }
    return (x->pitch > y->pitch) - (x->pitch < y->pitch);
}

/* The division (ticks a quarter) of a Standard MIDI File. */
static long long division_of(const uint8_t *data, size_t size) {
    assert(size >= 14 && memcmp(data, "MThd", 4) == 0);
    long long division = big_endian(data + 12, 2);
    assert(division > 0 && division < 0x8000);
    return division;
}

/* The notes of a Standard MIDI File, onsets and durations multiplied by scale, sorted; their count. */
static size_t midi_notes(const uint8_t *data, size_t size, long long scale, Note *out) {
    size_t count = 0;
    size_t at = 8 + big_endian(data + 4, 4);
    size_t tracks = big_endian(data + 10, 2);
    for (size_t track = 0; track < tracks; track++) {
        assert(at + 8 <= size && memcmp(data + at, "MTrk", 4) == 0);
        size_t end = at + 8 + big_endian(data + at + 4, 4);
        assert(end <= size);
        at += 8;
        static Open open[OPEN_MAX];
        size_t open_count = 0;
        long long tick = 0;
        uint8_t status = 0;
        while (at < end) {
            tick += read_vlq(data, end, &at);
            assert(at < end);
            if (data[at] & 0x80) {
                status = data[at++];
            }
            if (status == 0xff) {
                at++;
                uint32_t length = read_vlq(data, end, &at);
                at += length;
                status = 0;
                continue;
            }
            if (status == 0xf0 || status == 0xf7) {
                uint32_t length = read_vlq(data, end, &at);
                at += length;
                status = 0;
                continue;
            }
            int kind = status & 0xf0;
            int channel = status & 0x0f;
            size_t data_bytes = kind == 0xc0 || kind == 0xd0 ? 1 : 2;
            assert(kind >= 0x80 && at + data_bytes <= end);
            if (kind == 0x90 && data[at + 1] > 0) {
                assert(open_count < OPEN_MAX);
                open[open_count++] = (Open){channel, data[at], tick};
            } else if (kind == 0x80 || kind == 0x90) {
                close_note(open, &open_count, channel, data[at], tick, scale, out, &count);
            }
            at += data_bytes;
        }
        at = end;
    }
    qsort(out, count, sizeof(Note), by_note);
    return count;
}

/* The full blueprint of the reference as the CLI sends it: seed 1, a MIDI file, no check. */
static char *request_text(const Reference *reference) {
    char path[512];
    snprintf(path, sizeof path, COMPOSER_FIXTURE_DIR "/references/%s.full.json", reference->name);
    char *text = test_must_read_file(path, NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_IsObject(request));
    assert(cJSON_AddStringToObject(request, "seed", "1") && cJSON_AddBoolToObject(request, "check", false) &&
           cJSON_AddBoolToObject(request, "midi", true) && cJSON_AddBoolToObject(request, "musicxml", false) &&
           cJSON_AddBoolToObject(request, "plain_midi", false) && cJSON_AddBoolToObject(request, "plans", false));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static void print_note(const char *what, const Note *note, long long quarter) {
    fprintf(stderr, "  %s %g %g %d\n", what, (double)note->onset / (double)quarter, (double)note->duration / (double)quarter,
            note->pitch);
}

static void check_reference(const Reference *reference) {
    char *request = request_text(reference);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), request, NULL, &response);
    free(request);
    if (status != URSATZ_OK) {
        fprintf(stderr, "%s: %s at %s: %s\n", reference->name, test_status_name(status), response.error.path, response.error.message);
    }
    assert(status == URSATZ_OK && response.midi);
    char path[512];
    snprintf(path, sizeof path, COMPOSER_FIXTURE_DIR "/references/%s.mid", reference->name);
    size_t size = 0;
    uint8_t *expected_file = (uint8_t *)test_must_read_file(path, &size);
    long long expected_division = division_of(expected_file, size);
    long long realized_division = division_of(response.midi, response.midi_size);
    /* Both in ticks of expected_division * realized_division a quarter. */
    static Note expected[NOTES_MAX];
    static Note realized[NOTES_MAX];
    size_t expected_count = midi_notes(expected_file, size, realized_division, expected);
    size_t realized_count = midi_notes(response.midi, response.midi_size, expected_division, realized);
    long long quarter = expected_division * realized_division;
    bool same = expected_count == realized_count;
    for (size_t i = 0; same && i < expected_count; i++) {
        same = by_note(&expected[i], &realized[i]) == 0;
    }
    if (!same) {
        fprintf(stderr, "%s: the reference has %zu notes, the blueprint renders %zu\n", reference->name, expected_count, realized_count);
        for (size_t i = 0; i < expected_count && i < realized_count; i++) {
            if (by_note(&expected[i], &realized[i]) != 0) {
                print_note("reference", &expected[i], quarter);
                print_note("rendered ", &realized[i], quarter);
                break;
            }
        }
    }
    assert(same);
    assert(expected_count == reference->notes);
    printf("%s: same %zu\n", reference->name, expected_count);
    free(expected_file);
    composer_response_destroy(&response);
}

int main(void) {
    for (size_t i = 0; i < REFERENCE_COUNT; i++) {
        check_reference(&REFERENCES[i]);
    }
    printf("test_reference_blueprints: ok (%zu references)\n", REFERENCE_COUNT);
    return 0;
}
