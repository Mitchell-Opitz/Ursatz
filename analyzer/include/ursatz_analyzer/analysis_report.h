#ifndef URSATZ_ANALYZER_ANALYSIS_REPORT_H
#define URSATZ_ANALYZER_ANALYSIS_REPORT_H

#include <stdbool.h>
#include <stddef.h>

/* A library Interpretation's scalar confidence (numerator/denominator in
   [0, 1]); present is false when the library supplies none. */
typedef struct {
    bool present;
    unsigned long numerator;
    unsigned long denominator;
} AnalysisConfidence;

/* One chord reading from the library's weighted identification: the best
   chord of a segment, or its runner-up. label is NULL when there is no
   such reading (no match, or no runner-up). All strings are owned. */
typedef struct {
    char *label;   /* the library's label for the chord, e.g. "V" or "dominant7" */
    char *root;    /* root pitch class name, e.g. "G" */
    char *quality; /* e.g. "major", "minor", "diminished", "dominant7" */
    char *bass;    /* bass pitch class name; the segment's, so the same for both readings */
    int inversion; /* 0 root position, 1 first, 2 second, 3 third; -1 when the bass is not a chord tone */
    double score;  /* the library's candidate score */
    AnalysisConfidence confidence;
} AnalysisChordReading;

/* What one outer voice (the soprano or the bass) is in one chord segment, from the library's outer-voice
   extraction (analysis/outer_voice.h). The extraction is theory-neutral pitch height; the degree is the
   library's label for that pitch in the local key of the segment's key region. */
typedef enum {
    ANALYSIS_OUTER_ABSENT = 0,  /* the line is not sounding: only other voices are */
    ANALYSIS_OUTER_PRESENT,     /* a note sounds as this line */
    ANALYSIS_OUTER_BRIDGED,     /* a rest inside the line: a short gap between two of its notes */
    ANALYSIS_OUTER_GUARDED      /* dropped by the register guard rather than adopt an accompaniment note */
} AnalysisOuterStatus;

typedef struct {
    AnalysisOuterStatus status;
    size_t note_id;     /* ImportedNotes index; valid only when status is PRESENT */
    int pitch;          /* MIDI note number; valid only when PRESENT */
    bool held;          /* the note was attacked before the segment began; valid only when PRESENT */
    char *pitch_name;   /* owned; e.g. "E5", spelled in the key of the segment's region; NULL unless PRESENT */
    char *degree;       /* owned; the library's scale degree label, e.g. "3" or "#4"; NULL unless PRESENT and the
                           segment lies in a key region */
} AnalysisOuterNote;

/* The Roman numeral the library's standard-tonality adapter gives a chord in the local key of the region it
   lies in (analysis/roman_numeral_adapter.h), or why it gives none. The numeral is relative to the entry's
   key region (entry.region), not the home key. */
typedef struct {
    bool labelled;      /* the library produced a numeral for the segment */
    char *numeral;      /* owned; the library's label with its figure, e.g. "V6/5"; NULL unless labelled */
    char *reason;       /* owned; why the segment has no numeral; NULL when labelled */
    char *base_numeral; /* owned; the same numeral in root position, the library's own formatting of it with
                           no inversion figure: "V7" for "V6/5", "I" for "I6/4"; NULL unless labelled */
    int degree;         /* 1..7: the chord's root as a degree of the local key; valid only when labelled */
    double uncertainty; /* [0, 1], higher is LESS certain: the chord's and the key region's combined; valid only
                           when labelled */
    /* An applied (secondary) chord, as the library's applied-chord re-check reads it (analysis/applied_chords.h).
       numeral is then the whole label, e.g. "V6/5/V"; the parts are below. */
    bool applied;
    char *applied_numeral;      /* owned; the chord as it reads in the tonicized key, e.g. "V6/5"; NULL unless applied */
    char *target_numeral;       /* owned; the chord tonicized, e.g. "V"; NULL unless applied */
    int target_degree;          /* 1..7: the degree of the local key tonicized; valid only when applied */
    const char *applied_kind;   /* the library's name for the kind: "secondary-dominant" or "secondary-leading-tone" */
    size_t resolution_entry;    /* index into entries of the segment the chord resolves to; valid only when applied */
    bool deceptive;             /* resolves to the target's submediant, not the target; valid only when applied */
} AnalysisRomanNumeral;

/* Where the library's weighted identification took a chord reading from (CommonPracticeContextReading): the
   segment itself, the previous or the next segment's chord, or an arpeggio read across one beat. */
typedef enum {
    ANALYSIS_CHORD_CONTEXT_NONE = 0,
    ANALYSIS_CHORD_CONTEXT_PREVIOUS,
    ANALYSIS_CHORD_CONTEXT_NEXT,
    ANALYSIS_CHORD_CONTEXT_ARPEGGIO
} AnalysisChordContext;

typedef struct {
    long time_numerator; /* segment start, whole notes */
    unsigned long time_denominator;
    long end_numerator; /* segment end (exclusive), whole notes */
    unsigned long end_denominator;
    char *chord_label; /* owned; "no match" when nothing was identified */
    char *key_label;   /* owned; "no match" when nothing was identified */
    size_t *note_ids;  /* owned; ImportedNotes indices (Ursatz "midi-note-N") sounding in this segment, attacked or held */
    size_t note_id_count;
    AnalysisConfidence chord_confidence;
    AnalysisChordReading chord;     /* best chord; label NULL when "no match" */
    AnalysisChordReading runner_up; /* label NULL when the library gave none */
    AnalysisChordContext chord_context; /* where the best reading came from; NONE when "no match" */
    AnalysisOuterNote soprano;      /* the highest sounding line in this segment */
    AnalysisOuterNote bass;         /* the lowest sounding line in this segment */
    bool in_region;                 /* the segment lies in a key region */
    size_t region;                  /* index of that region in key_regions; valid only when in_region */
    AnalysisRomanNumeral roman;     /* the chord's Roman numeral in that region's key */
} AnalysisReportEntry;

/* Frees an outer note's strings and clears it. */
void analysis_outer_note_destroy(AnalysisOuterNote *note);

/* Frees a Roman numeral's strings and clears it. */
void analysis_roman_numeral_destroy(AnalysisRomanNumeral *roman);

/* Frees a reading's strings and clears it. */
void analysis_chord_reading_destroy(AnalysisChordReading *reading);

/* A note sounding in a chord's segment that is not one of the chord's tones
   (a "leftover tone" in the library's weighted identification), against
   the chord of entries[chord_index]. type is the library's name for the
   figure (non_chord_tone_name, e.g. "passing", "escape-tone"), or NULL
   when the library left the tone unclassified. */
typedef struct {
    size_t note_id; /* ImportedNotes index (Ursatz "midi-note-N") */
    size_t chord_index;
    char *type; /* owned; NULL when unclassified */
} AnalysisNonChordTone;

/* What set a key region's boundary: the library's profile correlation, or a notated key signature
   restricting the region to that signature's major key and its relative minor. */
typedef enum {
    ANALYSIS_REGION_SOURCE_PROFILE = 0,
    ANALYSIS_REGION_SOURCE_KEY_SIGNATURE
} AnalysisRegionSource;

/* One local key from the library's key-region detection. The regions of a report are ordered and gap-free;
   each runs from a barline to a barline. */
typedef struct {
    char *label;       /* owned; e.g. "Eb major", minimal-accidental spelling */
    int tonic_pitch_class; /* 0-11 */
    bool is_major;
    bool prefer_flats; /* the key reads in flats, for naming pitch classes */
    long start_numerator; /* region start, whole notes */
    unsigned long start_denominator;
    long end_numerator; /* region end (exclusive), whole notes */
    unsigned long end_denominator;
    int first_measure; /* measure numbers as the library numbers them (a pickup is 0), inclusive */
    int last_measure;
    double uncertainty; /* the library's, in [0, 1]; higher is LESS certain */
    AnalysisRegionSource source;
} AnalysisKeyRegion;

/* How far a change goes toward the new key: the library's KeyChangeExtent. */
typedef enum {
    ANALYSIS_KEY_CHANGE_TONICIZATION = 0,
    ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION,
    ANALYSIS_KEY_CHANGE_WEAK_MODULATION,
    ANALYSIS_KEY_CHANGE_MODULATION
} AnalysisKeyChangeExtent;

/* The change between two neighbouring key regions (to_region == from_region + 1). */
typedef struct {
    size_t from_region;
    size_t to_region;
    AnalysisKeyChangeExtent extent;
    bool is_return; /* goes back to the key before an excursion; extent is that excursion's */
    bool has_pivot; /* the library found a pivot chord; false for a direct change */
    size_t pivot_entry; /* index into entries of the pivot chord's segment; valid only when has_pivot */
    AnalysisChordReading pivot; /* the pivot chord read in the old key; label NULL unless has_pivot */
} AnalysisKeyChange;

/* One cadence from the library's standard-harmony analysis (analysis/analysis_cadence.h), in time order. Entry
   ranges are indices into entries, inclusive. */
typedef struct {
    char *type;                       /* owned; the library's CadenceType name, e.g. "perfect-authentic" */
    size_t arrival_first;             /* the chord segments of the arrival */
    size_t arrival_last;
    size_t dominant_first;            /* the dominant (with its cadential 6/4, when it has one) */
    size_t dominant_last;
    char *soprano_degree;             /* owned; the outer voice's label at the arrival's first entry; NULL when the
                                         soprano is absent or lies in no key region */
    const char *arrival_beat_strength; /* the library's BeatStrength name at the arrival: "strong", "medium", "weak" */
    bool boundary;                    /* a half cadence the boundary step added before a section start */
    bool in_region;                   /* the arrival lies in a key region */
    size_t region;                    /* index of that region in key_regions; valid only when in_region */
    double uncertainty;               /* [0, 1], higher is LESS certain */
} AnalysisCadence;

typedef struct {
    AnalysisReportEntry *entries;
    size_t entry_count;
    AnalysisNonChordTone *non_chord_tones; /* owned; in time order, then note order */
    size_t non_chord_tone_count;
    AnalysisKeyRegion *key_regions; /* owned; empty when no key was found */
    size_t key_region_count;
    AnalysisKeyChange *key_changes; /* owned; key_region_count - 1 of them */
    size_t key_change_count;
    AnalysisCadence *cadences; /* owned; in time order */
    size_t cadence_count;
    size_t home_region; /* first region in the home key; valid only when key_region_count > 0 */
    long home_duration_numerator; /* total time in the home key across its regions, whole notes */
    unsigned long home_duration_denominator;
} AnalysisReport;

/* Frees every entry's labels and note ids, the entries array, the key regions and changes, and the cadences. */
void analysis_report_destroy(AnalysisReport *report);

#endif
