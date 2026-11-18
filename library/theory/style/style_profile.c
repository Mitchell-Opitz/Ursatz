#include "theory/style/style_profile.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYNCOPATION_CAP 4

typedef struct {
    char *category;
    char *label;
    unsigned long count;
} ProfileEntry;

struct StyleProfile {
    ProfileEntry *entries; /* sorted by category, then label */
    size_t count;
    size_t capacity;
    size_t pieces;
    char *training_corpus;
};

StyleProfile *style_profile_create(void) {
    StyleProfile *profile = calloc(1, sizeof *profile);
    if (!profile) {
        return NULL;
    }
    profile->training_corpus = calloc(1, 1);
    if (!profile->training_corpus) {
        free(profile);
        return NULL;
    }
    return profile;
}

void style_profile_destroy(StyleProfile *profile) {
    if (!profile) {
        return;
    }
    for (size_t i = 0; i < profile->count; i++) {
        free(profile->entries[i].category);
        free(profile->entries[i].label);
    }
    free(profile->entries);
    free(profile->training_corpus);
    free(profile);
}

size_t style_profile_piece_count(const StyleProfile *profile) {
    return profile ? profile->pieces : 0;
}

const char *style_profile_training_corpus(const StyleProfile *profile) {
    return profile ? profile->training_corpus : "";
}

StyleProfileResult style_profile_add_training_corpus(StyleProfile *profile, const char *input_dir) {
    if (!profile || !input_dir) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    if (strcmp(profile->training_corpus, input_dir) == 0) {
        return STYLE_PROFILE_OK;
    }
    size_t old = strlen(profile->training_corpus);
    char *grown = realloc(profile->training_corpus, old + strlen(input_dir) + 2);
    if (!grown) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    profile->training_corpus = grown;
    if (old > 0) {
        grown[old++] = ';';
    }
    strcpy(grown + old, input_dir);
    return STYLE_PROFILE_OK;
}

static int compare_key(const char *category, const char *label, const ProfileEntry *entry) {
    int c = strcmp(category, entry->category);
    return c != 0 ? c : strcmp(label, entry->label);
}

/* Adds one to (category, label). false on allocation failure. */
static bool count_label(StyleProfile *profile, const char *category, const char *label) {
    size_t low = 0, high = profile->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int c = compare_key(category, label, &profile->entries[middle]);
        if (c == 0) {
            profile->entries[middle].count++;
            return true;
        }
        if (c < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    if (profile->count == profile->capacity) {
        size_t capacity = profile->capacity ? profile->capacity * 2 : 64;
        ProfileEntry *grown = realloc(profile->entries, capacity * sizeof *grown);
        if (!grown) {
            return false;
        }
        profile->entries = grown;
        profile->capacity = capacity;
    }
    char *category_copy = strdup(category);
    char *label_copy = strdup(label);
    if (!category_copy || !label_copy) {
        free(category_copy);
        free(label_copy);
        return false;
    }
    memmove(&profile->entries[low + 1], &profile->entries[low], (profile->count - low) * sizeof *profile->entries);
    profile->entries[low] = (ProfileEntry){category_copy, label_copy, 1};
    profile->count++;
    return true;
}

static bool count_number(StyleProfile *profile, const char *category, long value) {
    char label[32];
    snprintf(label, sizeof label, "%ld", value);
    return count_label(profile, category, label);
}

/* ---- counting a piece ---- */

/* Counts the chords: numerals and progressions within one key region. */
static bool count_harmony(StyleProfile *profile, const StylePiece *piece) {
    char *previous = NULL, *before_previous = NULL; /* the last two numerals of the current run, collapsed */
    long run_region = -1;
    bool ok = true;
    for (size_t i = 0; i < piece->chord_count; i++) {
        const StylePieceChord *chord = &piece->chords[i];
        if (!chord->base_numeral || !chord->has_region) {
            free(previous);
            free(before_previous);
            previous = before_previous = NULL;
            run_region = -1;
            continue;
        }
        char *numeral = style_label_numeral(chord->base_numeral);
        if (!numeral) {
            continue;
        }
        ok = count_label(profile, STYLE_CATEGORY_CHORD_NUMERAL, numeral);
        if (chord->region != run_region) {
            free(previous);
            free(before_previous);
            previous = before_previous = NULL;
            run_region = chord->region;
        }
        if (ok && !(previous && strcmp(previous, numeral) == 0)) {
            if (previous) {
                const char *pair[2] = {previous, numeral};
                char *label = style_label_progression(pair, 2);
                ok = label && count_label(profile, STYLE_CATEGORY_CHORD_PROGRESSION_2, label);
                free(label);
            }
            if (ok && previous && before_previous) {
                const char *triple[3] = {before_previous, previous, numeral};
                char *label = style_label_progression(triple, 3);
                ok = label && count_label(profile, STYLE_CATEGORY_CHORD_PROGRESSION_3, label);
                free(label);
            }
            free(before_previous);
            before_previous = previous;
            previous = numeral;
        } else {
            free(numeral);
        }
        if (!ok) {
            break;
        }
    }
    free(previous);
    free(before_previous);
    if (!ok) {
        return false;
    }
    for (size_t i = 0; i < piece->cadence_type_count; i++) {
        if (!count_label(profile, STYLE_CATEGORY_CADENCE_TYPE, piece->cadence_types[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < piece->harmonic_rhythm_count; i++) {
        if (!count_number(profile, STYLE_CATEGORY_HARMONIC_RHYTHM, piece->harmonic_rhythm_changes[i])) {
            return false;
        }
    }
    return true;
}

static bool count_rhythm(StyleProfile *profile, const StylePiece *piece) {
    for (size_t i = 0; i < piece->time_signature_count; i++) {
        char label[32];
        snprintf(label, sizeof label, "%ld/%ld", piece->time_signatures[i].numerator,
                 piece->time_signatures[i].denominator);
        if (!count_label(profile, STYLE_CATEGORY_METER, label)) {
            return false;
        }
    }
    if (piece->tempo_declared) {
        char label[32];
        style_label_tempo(piece->tempo_quarters_per_minute, label, sizeof label);
        if (!count_label(profile, STYLE_CATEGORY_TEMPO, label)) {
            return false;
        }
    }
    for (size_t i = 0; i < piece->density_count; i++) {
        char label[32];
        style_label_density(piece->densities[i], label, sizeof label);
        if (!count_label(profile, STYLE_CATEGORY_NOTE_DENSITY, label)) {
            return false;
        }
    }
    for (size_t p = 0; p < piece->phrase_count; p++) {
        const StylePiecePhrase *phrase = &piece->phrases[p];
        long syncopations = 0;
        for (size_t s = 0; s < piece->syncopation_count; s++) {
            if (piece->syncopations[s] >= phrase->first_measure && piece->syncopations[s] <= phrase->last_measure) {
                syncopations++;
            }
        }
        char label[32];
        if (syncopations >= SYNCOPATION_CAP) {
            snprintf(label, sizeof label, "%d+", SYNCOPATION_CAP);
        } else {
            snprintf(label, sizeof label, "%ld", syncopations);
        }
        if (!count_label(profile, STYLE_CATEGORY_SYNCOPATION, label)) {
            return false;
        }
    }
    return true;
}

/* Soprano durations, melodic intervals and contour. A note held across several voice segments (one entry per melody note) is one note. */
static bool count_soprano(StyleProfile *profile, const StylePiece *piece) {
    bool have_previous = false;
    long previous_id = 0;
    long previous_pitch = 0;
    for (size_t i = 0; i < piece->soprano_count; i++) {
        const StylePieceSopranoNote *note = &piece->soprano[i];
        if (!note->present) {
            continue;
        }
        if (have_previous && note->note_id == previous_id) {
            continue;
        }
        if (note->has_duration && note->duration.den > 0 && note->duration.num >= 0) {
            char label[48];
            style_label_duration((unsigned long)note->duration.num, (unsigned long)note->duration.den, label, sizeof label);
            if (!count_label(profile, STYLE_CATEGORY_DURATION, label)) {
                return false;
            }
        }
        if (have_previous) {
            int step = (int)(note->midi - previous_pitch);
            char label[16];
            style_label_interval(step, label, sizeof label);
            if (!count_label(profile, STYLE_CATEGORY_MELODIC_INTERVAL, label) ||
                !count_label(profile, STYLE_CATEGORY_CONTOUR, style_label_contour(step))) {
                return false;
            }
        }
        have_previous = true;
        previous_id = note->note_id;
        previous_pitch = note->midi;
    }
    return true;
}

/* The strings joined by single spaces into a new string, or NULL when there is none, an entry is NULL, or on allocation
   failure (*failed). */
static char *join_strings(char *const *strings, size_t count, bool *failed) {
    if (count == 0) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (!strings[i]) {
            return NULL;
        }
    }
    char *joined = style_label_join((const char *const *)strings, count);
    *failed = *failed || !joined;
    return joined;
}

/* The non-chord-tone type the piece names for a note, or NULL. The first claim of a note wins. */
static const char *note_nct_type(const StylePiece *piece, long note_id) {
    for (size_t i = 0; i < piece->non_chord_tone_count; i++) {
        if (piece->non_chord_tones[i].note_id == note_id) {
            return piece->non_chord_tones[i].type;
        }
    }
    return NULL;
}

typedef struct {
    long *ids;
    size_t count;
    size_t capacity;
} SeenNotes;

/* Records the note; false when it was already there or on allocation failure (*failed). */
static bool see_note(SeenNotes *seen, long id, bool *failed) {
    for (size_t i = 0; i < seen->count; i++) {
        if (seen->ids[i] == id) {
            return false;
        }
    }
    if (seen->count == seen->capacity) {
        size_t capacity = seen->capacity ? seen->capacity * 2 : 64;
        long *grown = realloc(seen->ids, capacity * sizeof *grown);
        if (!grown) {
            *failed = true;
            return false;
        }
        seen->ids = grown;
        seen->capacity = capacity;
    }
    seen->ids[seen->count++] = id;
    return true;
}

static const char *degree_at(char *const *degrees, size_t count, long index) {
    return index >= 0 && (size_t)index < count && degrees[index] ? degrees[index] : "-";
}

/* One label per non-chord tone on a note of an occurrence of a family with features: the previous note's degree, the
   note's degree and type joined by ":", the next note's degree ("1 b6:appoggiatura 5"), a dash for a neighbour the
   occurrence does not have. A note counts once for the piece, whichever occurrence names it first. The family's
   degrees, intervals and rhythm are one label each. */
static bool count_motives(StyleProfile *profile, const StylePiece *piece) {
    SeenNotes seen = {NULL, 0, 0};
    bool ok = true;
    bool failed = false;
    for (size_t f = 0; f < piece->motive_count; f++) {
        const StylePieceMotive *family = &piece->motives[f];
        if (!ok || !family->has_features) {
            continue;
        }
        char *degrees = join_strings(family->degrees, family->degree_count, &failed);
        char *intervals = NULL;
        if (!failed && family->interval_count > 0) {
            intervals = style_label_motive_intervals(family->intervals, family->interval_count);
            failed = !intervals;
        }
        char *rhythm = failed ? NULL : join_strings(family->rhythm, family->rhythm_count, &failed);
        ok = !failed && (!degrees || count_label(profile, STYLE_CATEGORY_MOTIVE_DEGREES, degrees)) &&
             (!intervals || count_label(profile, STYLE_CATEGORY_MOTIVE_INTERVALS, intervals)) &&
             (!rhythm || count_label(profile, STYLE_CATEGORY_MOTIVE_RHYTHM, rhythm));
        free(degrees);
        free(intervals);
        free(rhythm);
        for (size_t o = 0; o < family->occurrence_count; o++) {
            const StylePieceOccurrence *occurrence = &family->occurrences[o];
            if (!ok || !occurrence->degrees) {
                continue;
            }
            size_t count = occurrence->note_count;
            for (size_t k = 0; ok && k < count; k++) {
                long id = occurrence->note_ids[k];
                const char *type = note_nct_type(piece, id);
                if (!type || !see_note(&seen, id, &failed)) {
                    ok = !failed;
                    continue;
                }
                const char *current = degree_at(occurrence->degrees, count, (long)k);
                size_t length = strlen(current) + strlen(type) + 2 * 16 + 4;
                char *label = malloc(length);
                if (!label) {
                    ok = false;
                    break;
                }
                style_label_nct_figure(degree_at(occurrence->degrees, count, (long)k - 1), current, type,
                                       k + 1 < count ? degree_at(occurrence->degrees, count, (long)k + 1) : NULL, label, length);
                ok = count_label(profile, STYLE_CATEGORY_MOTIVE_NCT_FIGURE, label);
                free(label);
            }
        }
    }
    free(seen.ids);
    return ok && !failed;
}

static bool count_melody(StyleProfile *profile, const StylePiece *piece) {
    if (!count_soprano(profile, piece) || !count_motives(profile, piece)) {
        return false;
    }
    for (size_t i = 0; i < piece->melody_range_count; i++) {
        if (!count_number(profile, STYLE_CATEGORY_PHRASE_RANGE, piece->melody_ranges[i])) {
            return false;
        }
    }
    return true;
}

static bool count_form(StyleProfile *profile, const StylePiece *piece) {
    for (size_t i = 0; i < piece->theme_count; i++) {
        if (!count_label(profile, STYLE_CATEGORY_THEME_FORM, piece->theme_labels[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < piece->phrase_count; i++) {
        if (!count_number(profile, STYLE_CATEGORY_PHRASE_LENGTH, piece->phrases[i].length)) {
            return false;
        }
    }
    size_t length = 1;
    for (size_t i = 0; i < piece->section_count; i++) {
        length += strlen(piece->section_labels[i]) + 1;
    }
    if (length > 1) {
        char *letters = malloc(length);
        if (!letters) {
            return false;
        }
        letters[0] = '\0';
        for (size_t i = 0; i < piece->section_count; i++) {
            if (letters[0]) {
                strcat(letters, " ");
            }
            strcat(letters, piece->section_labels[i]);
        }
        bool ok = count_label(profile, STYLE_CATEGORY_SECTION_LETTERS, letters);
        free(letters);
        if (!ok) {
            return false;
        }
    }
    const char *home = piece->home_key_label;
    const char *mode = home ? strrchr(home, ' ') : NULL;
    if (mode && mode[1] && !count_label(profile, STYLE_CATEGORY_MODE, mode + 1)) {
        return false;
    }
    return true;
}

StyleProfileResult style_profile_add_piece(StyleProfile *profile, const StylePiece *piece) {
    if (!profile || !piece) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    bool ok = count_harmony(profile, piece) && count_rhythm(profile, piece) && count_melody(profile, piece) &&
              count_form(profile, piece);
    if (!ok) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    profile->pieces++;
    return STYLE_PROFILE_OK;
}

/* ---- statistics ---- */

StyleProfileResult style_profile_statistics(const StyleProfile *profile, StyleStatistic **out, size_t *count) {
    if (!profile || !out || !count) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = profile->count;
    if (profile->count == 0) {
        return STYLE_PROFILE_OK;
    }
    StyleStatistic *statistics = malloc(profile->count * sizeof *statistics);
    if (!statistics) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    size_t start = 0;
    for (size_t i = 0; i <= profile->count; i++) {
        if (i == profile->count || strcmp(profile->entries[i].category, profile->entries[start].category) != 0) {
            unsigned long total = 0;
            for (size_t j = start; j < i; j++) {
                total += profile->entries[j].count;
            }
            for (size_t j = start; j < i; j++) {
                statistics[j] = (StyleStatistic){profile->entries[j].category, profile->entries[j].label,
                                                 profile->entries[j].count, total};
            }
            start = i;
        }
    }
    *out = statistics;
    return STYLE_PROFILE_OK;
}
