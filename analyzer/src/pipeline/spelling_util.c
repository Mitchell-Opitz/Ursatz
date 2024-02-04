#include "pipeline/spelling_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/pitch_class_util.h"
#include "pitch/pitch_spelling.h"

static const char LETTER_NAME[7] = {'A', 'B', 'C', 'D', 'E', 'F', 'G'};
static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};

bool spelled_note_name(const NoteEvent *note, long sounding_pitch, bool with_octave, char out[16]) {
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
    if (!spelling) {
        return false;
    }
    int letter = (int)pitch_spelling_letter(spelling);
    int accidental = pitch_spelling_accidental(spelling);
    if (letter < 0 || letter > 6 || accidental < -2 || accidental > 2 ||
        pitch_class_of(LETTER_SEMITONE[letter] + accidental) != pitch_class_of(sounding_pitch)) {
        return false;
    }
    static const char *const ACCIDENTAL[5] = {"bb", "b", "", "#", "##"};
    if (with_octave) {
        snprintf(out, 16, "%c%s%d", LETTER_NAME[letter], ACCIDENTAL[accidental + 2], pitch_spelling_octave(spelling));
    } else {
        snprintf(out, 16, "%c%s", LETTER_NAME[letter], ACCIDENTAL[accidental + 2]);
    }
    return true;
}

static int pitch_class_of_name(const char *name) {
    for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
        if (strcmp(name, PITCH_CLASS_NAMES[pitch_class]) == 0 || strcmp(name, PITCH_CLASS_NAMES_FLAT[pitch_class]) == 0) {
            return pitch_class;
        }
    }
    return -1;
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
