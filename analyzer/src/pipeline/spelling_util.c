#include "pipeline/spelling_util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/pitch_class_util.h"
#include "pitch/pitch_arithmetic.h"
#include "pitch/pitch_spelling.h"

bool spelled_note_name(const NoteEvent *note, long sounding_pitch, bool with_octave, char out[16]) {
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
    if (!spelling) {
        return false;
    }
    PitchLetter letter = pitch_spelling_letter(spelling);
    int accidental = pitch_spelling_accidental(spelling);
    char name[8];
    if (!pitch_spelling_name(letter, accidental, name, sizeof(name)) ||
        pitch_class_of_letter(letter, accidental) != pitch_class_of(sounding_pitch)) {
        return false;
    }
    if (with_octave) {
        snprintf(out, 16, "%s%d", name, pitch_spelling_octave(spelling));
    } else {
        snprintf(out, 16, "%s", name);
    }
    return true;
}

void note_name(long midi, bool prefer_flats, char out[16]) {
    snprintf(out, 16, "%s%d", pitch_class_name(pitch_class_of(midi), prefer_flats), (int)(midi / 12) - 1);
}

int spelling_from_name(const char *name, PitchSpelling **out) {
    *out = NULL;
    PitchLetter letter;
    int accidental;
    int octave = INT_MIN;
    if (!pitch_spelling_parse_name(name, &letter, &accidental, &octave) || octave == INT_MIN) {
        return 0;
    }
    PitchSpellingResult result = pitch_spelling_create(letter, accidental, octave, out);
    return result == PITCH_SPELLING_OK ? 1 : (result == PITCH_SPELLING_ERR_ALLOC ? -1 : 0);
}

int note_pitch_spelling(const ImportedNotes *notes, size_t note, bool prefer_flats, PitchSpelling **out) {
    long pitch = pitch_numeric_value_position_numerator(notes->pitch_values[note]);
    char name[16];
    if (!(notes->source.spelling_notated && spelled_note_name(notes->events[note], pitch, true, name))) {
        note_name(pitch, prefer_flats, name);
    }
    return spelling_from_name(name, out);
}

/* The pitch class of a note name without an octave ("C#", "Db"); -1 for text that is not one. */
static int pitch_class_of_name(const char *name) {
    PitchLetter letter;
    int accidental;
    return pitch_spelling_parse_name(name, &letter, &accidental, NULL) ? pitch_class_of_letter(letter, accidental) : -1;
}

/* The spelling of the lowest note of the segment that has pitch_class; false when none is spelled. */
static bool lowest_spelling(const ImportedNotes *notes, const Segment *segment, int pitch_class, char out[16]) {
    size_t found = segment->member_count;
    for (size_t i = 0; i < segment->member_count; i++) {
        if (pitch_class_of(segment->members[i].pitch) == pitch_class &&
            (found == segment->member_count || segment->members[i].pitch < segment->members[found].pitch)) {
            found = i;
        }
    }
    return found < segment->member_count &&
           spelled_note_name(notes->events[segment->note_indices[found]], segment->members[found].pitch, false, out);
}

static bool rename_to(char **name, const char *spelled) {
    if (strcmp(*name, spelled) == 0) {
        return true;
    }
    char *copy = strdup(spelled);
    if (!copy) {
        return false;
    }
    free(*name);
    *name = copy;
    return true;
}

bool respell_chord_reading(AnalysisChordReading *reading, const ImportedNotes *notes, const Segment *segment) {
    if (!reading->label || !reading->root || !reading->bass || segment->member_count == 0) {
        return true;
    }
    char spelled[16];
    int root = pitch_class_of_name(reading->root);
    if (root >= 0 && lowest_spelling(notes, segment, root, spelled) && !rename_to(&reading->root, spelled)) {
        return false;
    }
    int bass = segment_bass_pitch_class(segment);
    return !lowest_spelling(notes, segment, bass, spelled) || rename_to(&reading->bass, spelled);
}
