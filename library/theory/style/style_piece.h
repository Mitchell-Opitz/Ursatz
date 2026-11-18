#ifndef URSATZ_THEORY_STYLE_PIECE_H
#define URSATZ_THEORY_STYLE_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"

/*
 * The facts about one piece that a style profile counts (R-617): the chords' base numerals and key regions, the cadence
 * types, the harmonic rhythm, the meter, the tempo and densities, the phrases and their syncopations, the soprano notes,
 * the non-chord tones, the motive families, the melody ranges, the theme and section labels and the home key. Two
 * sources fill it, a document's JSON (the Analyzer) and an analysis chain's result (chain/chain_style.h), and a profile
 * counts it (theory/style/style_profile.h). A piece owns its strings and arrays; start from `StylePiece piece = {0}` and
 * release with style_piece_destroy. Every add function returns false on allocation failure and leaves the piece valid.
 */

typedef struct {
    char *base_numeral; /* the chord's Roman numeral in root position, NULL when the chord has none */
    bool has_region;    /* the chord lies in a key region */
    long region;        /* that region's index; chords of one run share it */
} StylePieceChord;

typedef struct {
    long numerator;
    long denominator;
} StylePieceTimeSignature;

typedef struct {
    long first_measure; /* the phrase proper, as the document's "measures" give it */
    long last_measure;
    long length;
} StylePiecePhrase;

typedef struct {
    bool present;       /* a note sounds as the soprano in this voice segment; the other fields are valid only then */
    long note_id;       /* one entry per voice segment: a note held over several is repeated */
    long midi;
    bool has_duration;  /* the note's length in whole notes is known and not negative */
    Rational duration;
} StylePieceSopranoNote;

typedef struct {
    long note_id;
    char *type; /* the library's non-chord-tone name */
} StylePieceNonChordTone;

typedef struct {
    long *note_ids;
    char **degrees; /* one per note, an entry NULL for a note with no degree; the array is NULL when the occurrence has none */
    size_t note_count;
} StylePieceOccurrence;

typedef struct {
    bool has_features;
    char **degrees;   /* the prototype's: one label per note, an entry NULL for none */
    size_t degree_count;
    int *intervals;   /* signed semitones between neighbours */
    size_t interval_count;
    char **rhythm;    /* the durations, each "n/d" in whole notes */
    size_t rhythm_count;
    StylePieceOccurrence *occurrences;
    size_t occurrence_count;
} StylePieceMotive;

typedef struct {
    StylePieceChord *chords;
    size_t chord_count;
    char **cadence_types;
    size_t cadence_type_count;
    long *harmonic_rhythm_changes; /* one count per measure */
    size_t harmonic_rhythm_count;
    StylePieceTimeSignature *time_signatures; /* the first measure's, then each change's */
    size_t time_signature_count;
    bool tempo_declared;
    double tempo_quarters_per_minute;
    double *densities; /* the mean attacks per beat of each phrase's density over all notes */
    size_t density_count;
    StylePiecePhrase *phrases;
    size_t phrase_count;
    long *syncopations; /* the measure of each syncopation of the soprano */
    size_t syncopation_count;
    StylePieceSopranoNote *soprano;
    size_t soprano_count;
    StylePieceNonChordTone *non_chord_tones;
    size_t non_chord_tone_count;
    StylePieceMotive *motives;
    size_t motive_count;
    long *melody_ranges; /* the range in semitones of each phrase's melody */
    size_t melody_range_count;
    char **theme_labels;
    size_t theme_count;
    char **section_labels;
    size_t section_count;
    char *home_key_label; /* NULL when the piece has no key region */
} StylePiece;

/* Frees what the piece owns and clears it. NULL is a no-op. */
void style_piece_destroy(StylePiece *piece);

bool style_piece_add_chord(StylePiece *piece, const char *base_numeral, bool has_region, long region);
bool style_piece_add_cadence_type(StylePiece *piece, const char *type);
bool style_piece_add_harmonic_rhythm(StylePiece *piece, long changes);
bool style_piece_add_time_signature(StylePiece *piece, long numerator, long denominator);
void style_piece_set_tempo(StylePiece *piece, bool declared, double quarters_per_minute);
bool style_piece_add_density(StylePiece *piece, double attacks_per_beat_mean);
bool style_piece_add_phrase(StylePiece *piece, long first_measure, long last_measure, long length);
bool style_piece_add_syncopation(StylePiece *piece, long measure);
bool style_piece_add_soprano_note(StylePiece *piece, const StylePieceSopranoNote *note);
bool style_piece_add_non_chord_tone(StylePiece *piece, long note_id, const char *type);
bool style_piece_add_melody_range(StylePiece *piece, long semitones);
bool style_piece_add_theme_label(StylePiece *piece, const char *label);
bool style_piece_add_section_label(StylePiece *piece, const char *label);
bool style_piece_set_home_key_label(StylePiece *piece, const char *label);

/* A new motive family with no features and no occurrences, or NULL. The pointer is valid until the next motive is added. */
StylePieceMotive *style_piece_add_motive(StylePiece *piece);
/* Gives the family its prototype's features (copied; a NULL string in degrees is kept as none). */
bool style_piece_motive_set_features(StylePieceMotive *motive, const char *const *degrees, size_t degree_count,
                                     const int *intervals, size_t interval_count, const char *const *rhythm,
                                     size_t rhythm_count);
/* Adds an occurrence (copied). degrees may be NULL (the occurrence has none), else it holds note_count entries. */
bool style_piece_motive_add_occurrence(StylePieceMotive *motive, const long *note_ids, const char *const *degrees,
                                       size_t note_count);

#endif
