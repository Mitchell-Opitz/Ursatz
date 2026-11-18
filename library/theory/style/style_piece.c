#include "theory/style/style_piece.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Room for one more element of an array of *count elements of size bytes; the array and count are kept on failure. */
static bool grow(void **array, size_t *count, size_t size) {
    if (*count >= SIZE_MAX / size - 1) {
        return false;
    }
    void *grown = realloc(*array, (*count + 1) * size);
    if (!grown) {
        return false;
    }
    *array = grown;
    return true;
}

static char *copy_string(const char *text) { return text ? strdup(text) : NULL; }

static bool add_string(char ***array, size_t *count, const char *text) {
    char *copy = copy_string(text);
    if (!copy) {
        return false;
    }
    if (!grow((void **)array, count, sizeof **array)) {
        free(copy);
        return false;
    }
    (*array)[(*count)++] = copy;
    return true;
}

static void free_strings(char **strings, size_t count) {
    for (size_t i = 0; strings && i < count; i++) {
        free(strings[i]);
    }
    free(strings);
}

/* A copy of count strings, an entry NULL when the source's is; NULL with count 0 or on allocation failure (*failed). */
static char **copy_strings(const char *const *source, size_t count, bool *failed) {
    if (count == 0) {
        return NULL;
    }
    char **copy = calloc(count, sizeof *copy);
    if (!copy) {
        *failed = true;
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (source[i] && !(copy[i] = strdup(source[i]))) {
            free_strings(copy, i);
            *failed = true;
            return NULL;
        }
    }
    return copy;
}

static void destroy_motive(StylePieceMotive *motive) {
    free_strings(motive->degrees, motive->degree_count);
    free(motive->intervals);
    free_strings(motive->rhythm, motive->rhythm_count);
    for (size_t i = 0; i < motive->occurrence_count; i++) {
        free(motive->occurrences[i].note_ids);
        free_strings(motive->occurrences[i].degrees, motive->occurrences[i].note_count);
    }
    free(motive->occurrences);
}

void style_piece_destroy(StylePiece *piece) {
    if (!piece) {
        return;
    }
    for (size_t i = 0; i < piece->chord_count; i++) {
        free(piece->chords[i].base_numeral);
    }
    free(piece->chords);
    free_strings(piece->cadence_types, piece->cadence_type_count);
    free(piece->harmonic_rhythm_changes);
    free(piece->time_signatures);
    free(piece->densities);
    free(piece->phrases);
    free(piece->syncopations);
    free(piece->soprano);
    for (size_t i = 0; i < piece->non_chord_tone_count; i++) {
        free(piece->non_chord_tones[i].type);
    }
    free(piece->non_chord_tones);
    for (size_t i = 0; i < piece->motive_count; i++) {
        destroy_motive(&piece->motives[i]);
    }
    free(piece->motives);
    free(piece->melody_ranges);
    free_strings(piece->theme_labels, piece->theme_count);
    free_strings(piece->section_labels, piece->section_count);
    free(piece->home_key_label);
    memset(piece, 0, sizeof *piece);
}

bool style_piece_add_chord(StylePiece *piece, const char *base_numeral, bool has_region, long region) {
    char *copy = copy_string(base_numeral);
    if ((base_numeral && !copy) || !grow((void **)&piece->chords, &piece->chord_count, sizeof *piece->chords)) {
        free(copy);
        return false;
    }
    piece->chords[piece->chord_count++] = (StylePieceChord){copy, has_region, region};
    return true;
}

bool style_piece_add_cadence_type(StylePiece *piece, const char *type) {
    return add_string(&piece->cadence_types, &piece->cadence_type_count, type);
}

bool style_piece_add_harmonic_rhythm(StylePiece *piece, long changes) {
    if (!grow((void **)&piece->harmonic_rhythm_changes, &piece->harmonic_rhythm_count,
              sizeof *piece->harmonic_rhythm_changes)) {
        return false;
    }
    piece->harmonic_rhythm_changes[piece->harmonic_rhythm_count++] = changes;
    return true;
}

bool style_piece_add_time_signature(StylePiece *piece, long numerator, long denominator) {
    if (!grow((void **)&piece->time_signatures, &piece->time_signature_count, sizeof *piece->time_signatures)) {
        return false;
    }
    piece->time_signatures[piece->time_signature_count++] = (StylePieceTimeSignature){numerator, denominator};
    return true;
}

void style_piece_set_tempo(StylePiece *piece, bool declared, double quarters_per_minute) {
    piece->tempo_declared = declared;
    piece->tempo_quarters_per_minute = quarters_per_minute;
}

bool style_piece_add_density(StylePiece *piece, double attacks_per_beat_mean) {
    if (!grow((void **)&piece->densities, &piece->density_count, sizeof *piece->densities)) {
        return false;
    }
    piece->densities[piece->density_count++] = attacks_per_beat_mean;
    return true;
}

bool style_piece_add_phrase(StylePiece *piece, long first_measure, long last_measure, long length) {
    if (!grow((void **)&piece->phrases, &piece->phrase_count, sizeof *piece->phrases)) {
        return false;
    }
    piece->phrases[piece->phrase_count++] = (StylePiecePhrase){first_measure, last_measure, length};
    return true;
}

bool style_piece_add_syncopation(StylePiece *piece, long measure) {
    if (!grow((void **)&piece->syncopations, &piece->syncopation_count, sizeof *piece->syncopations)) {
        return false;
    }
    piece->syncopations[piece->syncopation_count++] = measure;
    return true;
}

bool style_piece_add_soprano_note(StylePiece *piece, const StylePieceSopranoNote *note) {
    if (!grow((void **)&piece->soprano, &piece->soprano_count, sizeof *piece->soprano)) {
        return false;
    }
    piece->soprano[piece->soprano_count++] = *note;
    return true;
}

bool style_piece_add_non_chord_tone(StylePiece *piece, long note_id, const char *type) {
    char *copy = copy_string(type);
    if (!copy || !grow((void **)&piece->non_chord_tones, &piece->non_chord_tone_count, sizeof *piece->non_chord_tones)) {
        free(copy);
        return false;
    }
    piece->non_chord_tones[piece->non_chord_tone_count++] = (StylePieceNonChordTone){note_id, copy};
    return true;
}

bool style_piece_add_melody_range(StylePiece *piece, long semitones) {
    if (!grow((void **)&piece->melody_ranges, &piece->melody_range_count, sizeof *piece->melody_ranges)) {
        return false;
    }
    piece->melody_ranges[piece->melody_range_count++] = semitones;
    return true;
}

bool style_piece_add_theme_label(StylePiece *piece, const char *label) {
    return add_string(&piece->theme_labels, &piece->theme_count, label);
}

bool style_piece_add_section_label(StylePiece *piece, const char *label) {
    return add_string(&piece->section_labels, &piece->section_count, label);
}

bool style_piece_set_home_key_label(StylePiece *piece, const char *label) {
    char *copy = copy_string(label);
    if (label && !copy) {
        return false;
    }
    free(piece->home_key_label);
    piece->home_key_label = copy;
    return true;
}

StylePieceMotive *style_piece_add_motive(StylePiece *piece) {
    if (!grow((void **)&piece->motives, &piece->motive_count, sizeof *piece->motives)) {
        return NULL;
    }
    StylePieceMotive *motive = &piece->motives[piece->motive_count++];
    memset(motive, 0, sizeof *motive);
    return motive;
}

bool style_piece_motive_set_features(StylePieceMotive *motive, const char *const *degrees, size_t degree_count,
                                     const int *intervals, size_t interval_count, const char *const *rhythm,
                                     size_t rhythm_count) {
    bool failed = false;
    char **degrees_copy = copy_strings(degrees, degree_count, &failed);
    char **rhythm_copy = failed ? NULL : copy_strings(rhythm, rhythm_count, &failed);
    int *intervals_copy = NULL;
    if (!failed && interval_count > 0) {
        intervals_copy = malloc(interval_count * sizeof *intervals_copy);
        failed = !intervals_copy;
        if (intervals_copy) {
            memcpy(intervals_copy, intervals, interval_count * sizeof *intervals_copy);
        }
    }
    if (failed) {
        free_strings(degrees_copy, degree_count);
        free_strings(rhythm_copy, rhythm_count);
        free(intervals_copy);
        return false;
    }
    destroy_motive(motive);
    memset(motive, 0, sizeof *motive);
    motive->has_features = true;
    motive->degrees = degrees_copy;
    motive->degree_count = degree_count;
    motive->intervals = intervals_copy;
    motive->interval_count = interval_count;
    motive->rhythm = rhythm_copy;
    motive->rhythm_count = rhythm_count;
    return true;
}

bool style_piece_motive_add_occurrence(StylePieceMotive *motive, const long *note_ids, const char *const *degrees,
                                       size_t note_count) {
    bool failed = false;
    long *ids = NULL;
    if (note_count > 0) {
        ids = malloc(note_count * sizeof *ids);
        failed = !ids;
        if (ids) {
            memcpy(ids, note_ids, note_count * sizeof *ids);
        }
    }
    char **degrees_copy = failed || !degrees ? NULL : copy_strings(degrees, note_count, &failed);
    if (failed || !grow((void **)&motive->occurrences, &motive->occurrence_count, sizeof *motive->occurrences)) {
        free(ids);
        free_strings(degrees_copy, note_count);
        return false;
    }
    motive->occurrences[motive->occurrence_count++] = (StylePieceOccurrence){ids, degrees_copy, note_count};
    return true;
}
