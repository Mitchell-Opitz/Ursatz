#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_profile.h"

#include "ursatz_test.h"

/* Counting a StylePiece into a StyleProfile (R-617), on pieces made by hand: the numerals and progressions of a key
   region, the rhythm, the soprano (a held note once), the motive figures (a note once), the form and the home key's mode. */

static const StyleStatistic *find(const StyleStatistic *list, size_t count, const char *category, const char *label) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i].category, category) == 0 && strcmp(list[i].label, label) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static unsigned long count_of(const StyleStatistic *list, size_t count, const char *category, const char *label) {
    const StyleStatistic *found = find(list, count, category, label);
    return found ? found->count : 0;
}

static unsigned long total_of(const StyleStatistic *list, size_t count, const char *category) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i].category, category) == 0) {
            return list[i].total;
        }
    }
    return 0;
}

static StyleProfile *profile_of(const StylePiece *piece, StyleStatistic **statistics, size_t *count) {
    StyleProfile *profile = style_profile_create();
    CHECKF(profile && style_profile_add_piece(profile, piece) == STYLE_PROFILE_OK, "the piece is counted");
    CHECKF(profile && style_profile_statistics(profile, statistics, count) == STYLE_PROFILE_OK, "statistics");
    return profile;
}

static void test_harmony(void) {
    StylePiece piece = {0};
    /* I I6 V7 I in region 0 (the held I collapses), a chord with no numeral (breaks the run), then ii V I in region 1. */
    const char *numerals[] = {"I", "I6", "V6/5", "I", NULL, "ii6", "V", "I"};
    const long regions[] = {0, 0, 0, 0, 0, 1, 1, 1};
    for (size_t i = 0; i < 8; i++) {
        CHECKF(style_piece_add_chord(&piece, numerals[i], true, regions[i]), "chord");
    }
    CHECKF(style_piece_add_chord(&piece, "IV", false, 0), "a chord in no key region");
    CHECKF(style_piece_add_cadence_type(&piece, "perfect-authentic") && style_piece_add_cadence_type(&piece, "half") &&
               style_piece_add_cadence_type(&piece, "perfect-authentic"),
           "cadences");
    CHECKF(style_piece_add_harmonic_rhythm(&piece, 1) && style_piece_add_harmonic_rhythm(&piece, 2) &&
               style_piece_add_harmonic_rhythm(&piece, 1),
           "harmonic rhythm");
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleProfile *profile = profile_of(&piece, &statistics, &count);
    CHECKF(count_of(statistics, count, "chord-numeral", "I") == 4 && count_of(statistics, count, "chord-numeral", "V7") == 1 &&
               count_of(statistics, count, "chord-numeral", "ii") == 1 && count_of(statistics, count, "chord-numeral", "V") == 1,
           "numerals in root position");
    CHECKF(total_of(statistics, count, "chord-numeral") == 7, "a chord with no numeral or no region is not counted: %lu",
           total_of(statistics, count, "chord-numeral"));
    CHECKF(count_of(statistics, count, "chord-progression-2", "I-V7") == 1 &&
               count_of(statistics, count, "chord-progression-2", "V7-I") == 1 &&
               count_of(statistics, count, "chord-progression-2", "ii-V") == 1 &&
               count_of(statistics, count, "chord-progression-2", "V-I") == 1 && total_of(statistics, count, "chord-progression-2") == 4,
           "pairs: a held chord collapses, a run ends with its key region or a chord with no numeral");
    CHECKF(count_of(statistics, count, "chord-progression-3", "I-V7-I") == 1 &&
               count_of(statistics, count, "chord-progression-3", "ii-V-I") == 1 && total_of(statistics, count, "chord-progression-3") == 2,
           "triples");
    CHECKF(count_of(statistics, count, "cadence-type", "perfect-authentic") == 2 &&
               count_of(statistics, count, "cadence-type", "half") == 1,
           "cadences");
    CHECKF(count_of(statistics, count, "harmonic-rhythm", "1") == 2 && count_of(statistics, count, "harmonic-rhythm", "2") == 1,
           "harmonic rhythm");
    free(statistics);
    style_profile_destroy(profile);
    style_piece_destroy(&piece);
}

static void test_rhythm(void) {
    StylePiece piece = {0};
    CHECKF(style_piece_add_time_signature(&piece, 4, 4) && style_piece_add_time_signature(&piece, 3, 4), "meter");
    style_piece_set_tempo(&piece, true, 132.0);
    CHECKF(style_piece_add_density(&piece, 1.25) && style_piece_add_density(&piece, 4.5), "densities");
    /* Two phrases: measures 1 to 4 hold two syncopations, measures 5 to 8 hold five (counted 4+). */
    CHECKF(style_piece_add_phrase(&piece, 1, 4, 4) && style_piece_add_phrase(&piece, 5, 8, 4), "phrases");
    const long measures[] = {1, 3, 5, 5, 6, 7, 8, 9};
    for (size_t i = 0; i < 8; i++) {
        CHECKF(style_piece_add_syncopation(&piece, measures[i]), "syncopation");
    }
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleProfile *profile = profile_of(&piece, &statistics, &count);
    CHECKF(count_of(statistics, count, "meter", "4/4") == 1 && count_of(statistics, count, "meter", "3/4") == 1, "meter");
    CHECKF(count_of(statistics, count, "tempo", "120-139") == 1, "tempo");
    CHECKF(count_of(statistics, count, "note-density", "3/2") == 1 && count_of(statistics, count, "note-density", "4+") == 1,
           "density");
    CHECKF(count_of(statistics, count, "syncopation-per-phrase", "2") == 1 &&
               count_of(statistics, count, "syncopation-per-phrase", "4+") == 1,
           "syncopations per phrase");
    CHECKF(count_of(statistics, count, "phrase-length", "4") == 2, "phrase length");
    free(statistics);
    style_profile_destroy(profile);
    style_piece_destroy(&piece);

    StylePiece quiet = {0};
    style_piece_set_tempo(&quiet, false, 90.0);
    profile = profile_of(&quiet, &statistics, &count);
    CHECKF(count == 0 && statistics == NULL, "no declared tempo counts no tempo");
    style_profile_destroy(profile);
}

static void test_melody_and_form(void) {
    StylePiece piece = {0};
    /* G4 (note 0, two voice segments), B4, A4 (a duration of 1/4 each): one note counted once. */
    StylePieceSopranoNote notes[] = {{true, 0, 67, true, {1, 4}},
                                     {true, 0, 67, true, {1, 4}},
                                     {false, 0, 0, false, {0, 1}},
                                     {true, 1, 71, true, {1, 4}},
                                     {true, 2, 69, true, {1, 8}}};
    for (size_t i = 0; i < 5; i++) {
        CHECKF(style_piece_add_soprano_note(&piece, &notes[i]), "soprano note");
    }
    CHECKF(style_piece_add_melody_range(&piece, 12) && style_piece_add_melody_range(&piece, 7), "ranges");
    CHECKF(style_piece_add_theme_label(&piece, "sentence") && style_piece_add_theme_label(&piece, "period"), "themes");
    CHECKF(style_piece_add_section_label(&piece, "A") && style_piece_add_section_label(&piece, "B") &&
               style_piece_add_section_label(&piece, "A'"),
           "sections");
    CHECKF(style_piece_set_home_key_label(&piece, "Eb major"), "home key");
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleProfile *profile = profile_of(&piece, &statistics, &count);
    CHECKF(count_of(statistics, count, "duration", "1/4") == 2 && count_of(statistics, count, "duration", "1/8") == 1,
           "a held note counts one duration");
    CHECKF(count_of(statistics, count, "melodic-interval", "+4") == 1 && count_of(statistics, count, "melodic-interval", "-2") == 1 &&
               total_of(statistics, count, "melodic-interval") == 2,
           "intervals between notes");
    CHECKF(count_of(statistics, count, "contour", "up") == 1 && count_of(statistics, count, "contour", "down") == 1, "contour");
    CHECKF(count_of(statistics, count, "phrase-range", "12") == 1 && count_of(statistics, count, "phrase-range", "7") == 1, "ranges");
    CHECKF(count_of(statistics, count, "theme-form", "sentence") == 1 && count_of(statistics, count, "theme-form", "period") == 1,
           "themes");
    CHECKF(count_of(statistics, count, "section-letters", "A B A'") == 1, "section letters");
    CHECKF(count_of(statistics, count, "mode", "major") == 1, "mode of the home key");
    free(statistics);
    style_profile_destroy(profile);
    style_piece_destroy(&piece);
}

/* Two families with the same features count them once each; a note named by two occurrences counts one figure; a note with no
   non-chord-tone type, or in a family without features, counts none. */
static void test_motives(void) {
    StylePiece piece = {0};
    CHECKF(style_piece_add_non_chord_tone(&piece, 3, "appoggiatura") && style_piece_add_non_chord_tone(&piece, 9, "passing") &&
               style_piece_add_non_chord_tone(&piece, 3, "neighbor"),
           "non-chord tones");
    const char *const degrees[] = {"1", "b6", "5"};
    const int intervals[] = {8, -1};
    const char *const rhythm[] = {"1/8", "1/8", "1/4"};
    const long ids_a[] = {2, 3, 4};
    const long ids_b[] = {8, 9, 10};
    const char *const occurrence_degrees_b[] = {"2", "4", NULL};
    StylePieceMotive *first = style_piece_add_motive(&piece);
    CHECKF(first && style_piece_motive_set_features(first, degrees, 3, intervals, 2, rhythm, 3) &&
               style_piece_motive_add_occurrence(first, ids_a, degrees, 3) &&
               style_piece_motive_add_occurrence(first, ids_a, degrees, 3) &&
               style_piece_motive_add_occurrence(first, ids_b, occurrence_degrees_b, 3),
           "first family");
    StylePieceMotive *second = style_piece_add_motive(&piece);
    CHECKF(second && style_piece_motive_set_features(second, degrees, 3, intervals, 2, rhythm, 3), "second family");
    StylePieceMotive *bare = style_piece_add_motive(&piece);
    CHECKF(bare && style_piece_motive_add_occurrence(bare, ids_a, degrees, 3), "a family without features");
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleProfile *profile = profile_of(&piece, &statistics, &count);
    CHECKF(count_of(statistics, count, "motive-degrees", "1 b6 5") == 2 && count_of(statistics, count, "motive-intervals", "+8 -1") == 2 &&
               count_of(statistics, count, "motive-rhythm", "1/8 1/8 1/4") == 2,
           "each family's features");
    CHECKF(count_of(statistics, count, "motive-nct-figure", "1 b6:appoggiatura 5") == 1 &&
               count_of(statistics, count, "motive-nct-figure", "2 4:passing -") == 1 &&
               total_of(statistics, count, "motive-nct-figure") == 2,
           "the first claim of a note wins and a note counts once");
    free(statistics);
    style_profile_destroy(profile);
    style_piece_destroy(&piece);
}

static void test_profile_object(void) {
    StyleProfile *profile = style_profile_create();
    CHECKF(profile && style_profile_piece_count(profile) == 0 && strcmp(style_profile_training_corpus(profile), "") == 0,
           "an empty profile");
    CHECKF(style_profile_add_training_corpus(profile, "corpus-a") == STYLE_PROFILE_OK &&
               style_profile_add_training_corpus(profile, "corpus-a") == STYLE_PROFILE_OK &&
               style_profile_add_training_corpus(profile, "corpus-b") == STYLE_PROFILE_OK &&
               strcmp(style_profile_training_corpus(profile), "corpus-a;corpus-b") == 0,
           "corpora join with ';' and one corpus twice counts once");
    StylePiece empty = {0};
    CHECKF(style_profile_add_piece(profile, &empty) == STYLE_PROFILE_OK && style_profile_add_piece(profile, &empty) == STYLE_PROFILE_OK &&
               style_profile_piece_count(profile) == 2,
           "an empty piece counts as a piece");
    StyleStatistic *statistics = (StyleStatistic *)1;
    size_t count = 5;
    CHECKF(style_profile_statistics(profile, &statistics, &count) == STYLE_PROFILE_OK && statistics == NULL && count == 0,
           "no statistics from empty pieces");
    CHECKF(style_profile_add_piece(NULL, &empty) == STYLE_PROFILE_ERR_ARGUMENT &&
               style_profile_add_piece(profile, NULL) == STYLE_PROFILE_ERR_ARGUMENT &&
               style_profile_statistics(NULL, &statistics, &count) == STYLE_PROFILE_ERR_ARGUMENT &&
               style_profile_add_training_corpus(profile, NULL) == STYLE_PROFILE_ERR_ARGUMENT,
           "refusals");
    style_profile_destroy(profile);
    style_profile_destroy(NULL);
    style_piece_destroy(NULL);
}

int main(void) {
    test_harmony();
    test_rhythm();
    test_melody_and_form();
    test_motives();
    test_profile_object();
    return test_finish("theory_style_profile");
}
