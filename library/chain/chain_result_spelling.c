#include "chain/chain_result_spelling.h"

#include "theory/tonality/tonal_key.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chain/chain_result_spelling.h"

#include "theory/tonality/tonal_key.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/midi_pitch.h"

bool spelled_note_name(const NoteEvent *note, long sounding_pitch, bool with_octave, char out[16]) {
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
    if (!spelling) {
        return false;
    }
    PitchLetter letter = pitch_spelling_letter(spelling);
    int accidental = pitch_spelling_accidental(spelling);
    char name[8];
    if (!pitch_spelling_name(letter, accidental, name, sizeof(name)) ||
        pitch_class_of_letter(letter, accidental) != pitch_class_of_midi((int)sounding_pitch)) {
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
    pitch_name_of_midi((int)midi, prefer_flats, out, 16);
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

/* The pitch class of a note name without an octave ("C#", "Db"); -1 for text that is not one. */
static int pitch_class_of_name(const char *name) {
    PitchLetter letter;
    int accidental;
    return pitch_spelling_parse_name(name, &letter, &accidental, NULL) ? pitch_class_of_letter(letter, accidental) : -1;
}

/* The spelling of the lowest note of the segment that has pitch_class; false when none is spelled. */
static bool lowest_spelling(const ImportedScore *notes, const Segment *segment, int pitch_class, char out[16]) {
    size_t found = segment->member_count;
    for (size_t i = 0; i < segment->member_count; i++) {
        if (pitch_class_of_midi((int)segment->members[i].pitch) == pitch_class &&
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

bool respell_chord_reading(AnalysisChordReading *reading, const ImportedScore *notes, const Segment *segment) {
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

bool chain_tonic_prefers_flats(int tonic_pitch_class, const PitchMaterial *material) {
    PitchLetter letter;
    int accidental;
    int fifths;
    return material != NULL &&
           standard_tonality_key_tonic_of_pitch_class_material(tonic_pitch_class, material, &letter, &accidental) &&
           standard_tonality_key_fifths_of(letter, accidental, material, &fifths) && fifths < 0;
}

char *chain_key_label(int tonic_pitch_class, const PitchMaterial *material) {
    if (material == NULL || material->key_word == NULL) {
        return NULL;
    }
    const char *name = pitch_class_name(tonic_pitch_class, chain_tonic_prefers_flats(tonic_pitch_class, material));
    const char *mode = material->key_word;
    size_t size = (size_t)snprintf(NULL, 0, "%s %s", name, mode) + 1;
    char *label = malloc(size);
    if (label) {
        snprintf(label, size, "%s %s", name, mode);
    }
    return label;
}

int chain_key_signature_major_tonic(int sharps_flats) {
    PitchLetter letter;
    int accidental;
    if (sharps_flats < -7 || sharps_flats > 7) {
        return -1;
    }
    /* The major tonic of a signature: `sharps_flats` steps along the line of fifths from C. */
    pitch_spelling_from_fifths(sharps_flats, &letter, &accidental);
    return pitch_class_of_letter(letter, accidental);
}
