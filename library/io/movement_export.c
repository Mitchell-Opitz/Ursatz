#include "io/movement_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_renderer.h"
#include "io/musicxml_export.h"
#include "model/core/provenance_reference.h"
#include "model/core/rational.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"

#define PREFERRED_DIVISION 480u
#define VOICE_ID_MAX (REALIZATION_ID_MAX + 16)

static MovementExportResult fail(char *message, size_t message_size, MovementExportResult result, const char *text) {
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size, "%s", text);
    }
    return result;
}

/* --- the NoteEvents of a movement's performer notes --- */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **values;
    size_t count;
    size_t *part_index; /* of each event: its performer part */
    int *staff;         /* of each event */
} Built;

static void built_release(Built *built) {
    for (size_t i = 0; i < built->count; i++) {
        note_event_destroy(built->events[i]);
        pitch_numeric_value_destroy(built->values[i]);
    }
    free(built->events);
    free(built->values);
    free(built->part_index);
    free(built->staff);
    memset(built, 0, sizeof(*built));
}

/* The sharp spellings of the twelve pitch classes: what a note with no spelling is written as. */
static void sharp_spelling(int midi, PitchLetter *letter, int *accidental) {
    pitch_class_spelling(pitch_class_of_midi(midi), -1, letter, accidental);
}

/* One NoteEvent and its pitch value; everything is freed again on failure. */
static bool make_event(const PerformerPart *part, const PerformerNote *note, const TuningSystem *tuning, NoteEvent **out_event,
                       PitchNumericValue **out_value) {
    EntityID *id = NULL;
    PitchNumericValue *value = NULL;
    PitchIdentity *identity = NULL;
    PitchSpelling *spelling = NULL;
    EntityID *voice_id = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    char text[VOICE_ID_MAX];

    PitchLetter letter = note->letter;
    int accidental = note->accidental;
    if (!note->spelled) {
        sharp_spelling(note->midi, &letter, &accidental);
    }
    int octave = (note->midi - (pitch_letter_semitone(letter) + accidental)) / PITCH_SEMITONES_PER_OCTAVE - 1;
    bool ok = entity_id_create(note->id, &id) == ENTITY_ID_OK &&
              pitch_numeric_value_create(tuning, note->midi, 1, &value) == PITCH_NUMERIC_VALUE_OK &&
              pitch_numeric_value_to_identity(value, &identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
              pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK;
    snprintf(text, sizeof(text), "%s-v%d", performer_part_id(part), note->voice);
    ok = ok && entity_id_create(text, &voice_id) == ENTITY_ID_OK && voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
    if (ok) {
        voice_id = NULL;
    }
    ok = ok && provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
    NoteEvent *event = NULL;
    if (ok && note_event_create(id, note->onset, note->duration, identity, spelling, voice, provenance, &event) == NOTE_EVENT_OK) {
        *out_event = event;
        *out_value = value;
        return true;
    }
    entity_id_destroy(id);
    pitch_numeric_value_destroy(value);
    pitch_identity_destroy(identity);
    pitch_spelling_destroy(spelling);
    entity_id_destroy(voice_id);
    voice_reference_destroy(voice);
    provenance_reference_destroy(provenance);
    return false;
}

static size_t total_notes(const Movement *movement) {
    size_t total = 0;
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        total += performer_part_note_count(movement_part_at(movement, p));
    }
    return total;
}

/* The NoteEvents of every part, in part order and each part's note order. */
static MovementExportResult build_events(const Movement *movement, size_t count, Built *built) {
    memset(built, 0, sizeof(*built));
    built->events = calloc(count, sizeof(NoteEvent *));
    built->values = calloc(count, sizeof(PitchNumericValue *));
    built->part_index = calloc(count, sizeof(size_t));
    built->staff = calloc(count, sizeof(int));
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    bool ok = built->events != NULL && built->values != NULL && built->part_index != NULL && built->staff != NULL &&
              entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK;
    if (ok && tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        ok = false;
    }
    for (size_t p = 0; ok && p < movement_part_count(movement); p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        for (size_t k = 0; ok && k < performer_part_note_count(part); k++) {
            const PerformerNote *note = performer_part_note_at(part, k);
            if (make_event(part, note, tuning, &built->events[built->count], &built->values[built->count])) {
                built->part_index[built->count] = p;
                built->staff[built->count] = note->staff;
                built->count++;
            } else {
                ok = false;
            }
        }
    }
    tuning_system_destroy(tuning);
    if (!ok) {
        built_release(built);
        return MOVEMENT_EXPORT_ERR_ALLOC;
    }
    return MOVEMENT_EXPORT_OK;
}

static MovementExportResult check_movement(const Movement *movement, size_t *count, char *message, size_t message_size) {
    if (movement_played_span_count(movement) > 0) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NOT_BUILT,
                    "the movement has played spans: unrolling a played order is not built");
    }
    *count = total_notes(movement);
    if (*count == 0) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NO_NOTES, "no performer part holds a note");
    }
    return MOVEMENT_EXPORT_OK;
}

/* --- MIDI --- */

/* Folds the denominator of one time into the lcm of ticks per quarter it needs. False on overflow. */
static bool need_division_for(unsigned long long denominator, unsigned long long *lcm) {
    unsigned long long factor = denominator / rational_gcd_unsigned(denominator, 4);
    unsigned long long divisor = rational_gcd_unsigned(*lcm, factor);
    unsigned long long grown;
    if (!rational_checked_mul_unsigned(*lcm / divisor, factor, &grown) || grown > MIDI_RENDER_MAX_DIVISION) {
        return false;
    }
    *lcm = grown;
    return true;
}

static bool need_time(MusicalTime time, unsigned long long *lcm) {
    return need_division_for((unsigned long long)time.value.den, lcm);
}

/* 480 when every time is a whole number of ticks at 480, else the smallest division that works; 0 when none fits. */
static unsigned choose_division(const Movement *movement) {
    unsigned long long needed = 1;
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        for (size_t k = 0; k < performer_part_note_count(part); k++) {
            const PerformerNote *note = performer_part_note_at(part, k);
            if (!need_time(note->onset, &needed) || !need_division_for((unsigned long long)note->duration.value.den, &needed)) {
                return 0;
            }
        }
        for (size_t k = 0; k < performer_part_pedal_count(part); k++) {
            const PedalMarking *pedal = performer_part_pedal_at(part, k);
            if (!need_time(pedal->span.start, &needed) || !need_time(pedal->span.end, &needed)) {
                return 0;
            }
        }
    }
    for (size_t i = 0; i < movement_key_signature_count(movement); i++) {
        if (!need_time(movement_key_signature_at(movement, i)->position, &needed)) {
            return 0;
        }
    }
    for (size_t i = 0; i < movement_marker_count(movement); i++) {
        if (!need_time(movement_marker_at(movement, i)->position, &needed)) {
            return 0;
        }
    }
    const MeterMap *meter = movement_meter_map(movement);
    for (size_t i = 0; meter != NULL && i < meter_map_region_count(meter); i++) {
        if (!need_time(meter_map_region_start(meter_map_region(meter, i)), &needed)) {
            return 0;
        }
    }
    const TempoMap *tempo = movement_tempo_map(movement);
    for (size_t i = 0; tempo != NULL && i < tempo_map_region_count(tempo); i++) {
        if (!need_time(tempo_map_region_start(tempo_map_region(tempo, i)), &needed)) {
            return 0;
        }
    }
    return PREFERRED_DIVISION % needed == 0 ? PREFERRED_DIVISION : (unsigned)needed;
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left, b = *(const int *)right;
    return (a > b) - (a < b);
}

typedef struct {
    char (*voice_text)[VOICE_ID_MAX]; /* every voice id of every part */
    const char **voice_ptr;           /* pointers into voice_text, in part order and voice order */
    MidiRenderPart *parts;
    TimeSpan *pedals;
    MidiKeySignatureEvent *keys;
    MidiMarker *markers;
} MidiBuild;

static void midi_build_release(MidiBuild *build) {
    free(build->voice_text);
    free(build->voice_ptr);
    free(build->parts);
    free(build->pedals);
    free(build->keys);
    free(build->markers);
}

static size_t total_pedals(const Movement *movement) {
    size_t total = 0;
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        total += performer_part_pedal_count(movement_part_at(movement, p));
    }
    return total;
}

static MovementExportResult build_midi_parts(const Movement *movement, bool names, size_t note_count, MidiBuild *build) {
    size_t part_count = movement_part_count(movement);
    build->voice_text = calloc(note_count, sizeof(*build->voice_text)); /* never more voices than notes */
    build->voice_ptr = calloc(note_count, sizeof(char *));
    build->parts = calloc(part_count, sizeof(MidiRenderPart));
    build->pedals = calloc(total_pedals(movement) + 1, sizeof(TimeSpan));
    int *voices = calloc(note_count, sizeof(int));
    if (build->voice_text == NULL || build->voice_ptr == NULL || build->parts == NULL || build->pedals == NULL || voices == NULL) {
        free(voices);
        return MOVEMENT_EXPORT_ERR_ALLOC;
    }
    size_t voice_at = 0, pedal_at = 0;
    for (size_t p = 0; p < part_count; p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        size_t n = performer_part_note_count(part), distinct = 0;
        for (size_t k = 0; k < n; k++) {
            voices[k] = performer_part_note_at(part, k)->voice;
        }
        qsort(voices, n, sizeof(int), compare_ints);
        MidiRenderPart *out = &build->parts[p];
        out->name = names ? performer_part_name(part) : NULL;
        out->program = performer_part_midi_program(part);
        out->voice_ids = &build->voice_ptr[voice_at];
        for (size_t k = 0; k < n; k++) {
            if (k > 0 && voices[k] == voices[k - 1]) {
                continue;
            }
            snprintf(build->voice_text[voice_at], VOICE_ID_MAX, "%s-v%d", performer_part_id(part), voices[k]);
            build->voice_ptr[voice_at] = build->voice_text[voice_at];
            voice_at++;
            distinct++;
        }
        out->voice_count = distinct;
        out->pedals = &build->pedals[pedal_at];
        for (size_t k = 0; k < performer_part_pedal_count(part); k++) {
            build->pedals[pedal_at++] = performer_part_pedal_at(part, k)->span;
        }
        out->pedal_count = performer_part_pedal_count(part);
    }
    free(voices);
    return MOVEMENT_EXPORT_OK;
}

MovementExportResult movement_export_midi(const Movement *movement, const MovementMidiOptions *options, unsigned char **out_data,
                                          size_t *out_size, char *message, size_t message_size) {
    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    if (movement == NULL || out_data == NULL || out_size == NULL) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NULL, "movement or output is NULL");
    }
    bool names = options == NULL || options->track_names;
    size_t count = 0;
    MovementExportResult result = check_movement(movement, &count, message, message_size);
    if (result != MOVEMENT_EXPORT_OK) {
        return result;
    }
    unsigned division = choose_division(movement);
    if (division == 0) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE, "the movement's times need more ticks per quarter than MIDI holds");
    }
    Built built;
    result = build_events(movement, count, &built);
    if (result != MOVEMENT_EXPORT_OK) {
        return fail(message, message_size, result, "out of memory");
    }
    MidiBuild build;
    memset(&build, 0, sizeof(build));
    size_t key_count = movement_key_signature_count(movement);
    size_t marker_count = movement_marker_count(movement);
    result = build_midi_parts(movement, names, count, &build);
    build.keys = calloc(key_count + 1, sizeof(MidiKeySignatureEvent));
    build.markers = calloc(marker_count + 1, sizeof(MidiMarker));
    if (result != MOVEMENT_EXPORT_OK || build.keys == NULL || build.markers == NULL) {
        midi_build_release(&build);
        built_release(&built);
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_ALLOC, "out of memory");
    }
    for (size_t i = 0; i < key_count; i++) {
        const MovementKeySignature *key = movement_key_signature_at(movement, i);
        build.keys[i].position = key->position;
        build.keys[i].sharps_flats = (int8_t)key->sharps_flats;
        build.keys[i].mode_byte = key->mode == MOVEMENT_KEY_MINOR ? 1 : 0;
        build.keys[i].mode_hint = key->mode == MOVEMENT_KEY_MINOR   ? MIDI_MODE_HINT_MINOR
                                  : key->mode == MOVEMENT_KEY_MAJOR ? MIDI_MODE_HINT_MAJOR
                                                                    : MIDI_MODE_HINT_UNKNOWN;
    }
    for (size_t i = 0; i < marker_count; i++) {
        build.markers[i].position = movement_marker_at(movement, i)->position;
        build.markers[i].text = movement_marker_at(movement, i)->text;
    }
    MidiRenderOptions render;
    memset(&render, 0, sizeof(render));
    if (names) {
        render.conductor_name = movement_title(movement) != NULL ? movement_title(movement) : "piece";
    }
    render.markers = build.markers;
    render.marker_count = marker_count;
    render.parts = build.parts;
    render.part_count = movement_part_count(movement);

    MidiRenderResult rendered = midi_renderer_render_with_options(built.events, built.values, built.count, movement_meter_map(movement),
                                                                  movement_tempo_map(movement), build.keys, key_count, &render,
                                                                  division, out_data, out_size);
    midi_build_release(&build);
    built_release(&built);
    switch (rendered) {
    case MIDI_RENDER_OK:
        return MOVEMENT_EXPORT_OK;
    case MIDI_RENDER_ERR_TOO_MANY_VOICES:
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_TOO_MANY_VOICES,
                    "more than 15 lines in the performer parts; MIDI has 15 channels for them");
    case MIDI_RENDER_ERR_ALLOC:
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_ALLOC, "out of memory");
    default:
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE,
                    "MIDI cannot represent the movement: a pitch outside 0 to 127, or two notes of one pitch overlapping on one channel");
    }
}

/* --- MusicXML --- */

MovementExportResult movement_export_musicxml(const Work *work, const Movement *movement, char **out_xml, char *message,
                                              size_t message_size) {
    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    if (movement == NULL || out_xml == NULL) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_NULL, "movement or output is NULL");
    }
    size_t count = 0;
    MovementExportResult result = check_movement(movement, &count, message, message_size);
    if (result != MOVEMENT_EXPORT_OK) {
        return result;
    }
    Built built;
    result = build_events(movement, count, &built);
    if (result != MOVEMENT_EXPORT_OK) {
        return fail(message, message_size, result, "out of memory");
    }
    size_t part_count = movement_part_count(movement);
    size_t key_count = movement_key_signature_count(movement);
    MusicXmlExportPart *parts = calloc(part_count, sizeof(*parts));
    MusicXmlExportKeySignature *keys = calloc(key_count + 1, sizeof(*keys));
    MusicXmlExportPedal *pedals = calloc(total_pedals(movement) + 1, sizeof(*pedals));
    if (parts == NULL || keys == NULL || pedals == NULL) {
        free(parts);
        free(keys);
        free(pedals);
        built_release(&built);
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_ALLOC, "out of memory");
    }
    size_t pedal_count = 0;
    for (size_t p = 0; p < part_count; p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        parts[p].id = performer_part_id(part);
        parts[p].name = performer_part_name(part);
        parts[p].staff_count = performer_part_staff_count(part);
        parts[p].midi_program = performer_part_midi_program(part);
        for (size_t k = 0; k < performer_part_pedal_count(part); k++) {
            pedals[pedal_count].span = performer_part_pedal_at(part, k)->span;
            pedals[pedal_count].part_index = p;
            pedals[pedal_count].staff = performer_part_staff_count(part); /* under the lowest staff: the bass of a grand staff */
            pedal_count++;
        }
    }
    for (size_t i = 0; i < key_count; i++) {
        const MovementKeySignature *key = movement_key_signature_at(movement, i);
        keys[i].sharps_flats = key->sharps_flats;
        keys[i].mode = key->mode == MOVEMENT_KEY_MINOR   ? MUSICXML_EXPORT_MODE_MINOR
                       : key->mode == MOVEMENT_KEY_MAJOR ? MUSICXML_EXPORT_MODE_MAJOR
                                                         : MUSICXML_EXPORT_MODE_UNKNOWN;
        keys[i].position = key->position;
        keys[i].part_index = 0;
    }
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = built.events;
    score.pitch_values = built.values;
    score.count = built.count;
    score.part_index = built.part_index;
    score.staff = built.staff;
    score.parts = parts;
    score.part_count = part_count;
    score.meter_map = movement_meter_map(movement);
    score.tempo_map = movement_tempo_map(movement);
    score.key_signatures = keys;
    score.key_signature_count = key_count;
    score.pedals = pedals;
    score.pedal_count = pedal_count;
    score.title = work != NULL ? work_title(work) : NULL;
    score.composer = work != NULL ? work_composer(work) : NULL;

    MusicXmlExportResult written = musicxml_export_write_score(&score, out_xml);
    free(parts);
    free(keys);
    free(pedals);
    built_release(&built);
    if (written == MUSICXML_EXPORT_OK) {
        return MOVEMENT_EXPORT_OK;
    }
    if (written == MUSICXML_EXPORT_ERR_ALLOC) {
        return fail(message, message_size, MOVEMENT_EXPORT_ERR_ALLOC, "out of memory");
    }
    char text[96];
    snprintf(text, sizeof(text), "MusicXML export refused the movement (code %d)", (int)written);
    return fail(message, message_size, MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE, text);
}
