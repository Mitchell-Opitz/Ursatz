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
    /* The library's harmonic function of the chord (analysis/analysis_function.h). NULL when the library gives
       none (a segment that is not a chord it reads, or no prepared segments). */
    const char *function;           /* the library's HarmonicFunction name: "tonic", "predominant" or "dominant" */
    bool function_applied;          /* an applied chord: a dominant toward function_applied_to */
    int function_applied_to;        /* the target's scale degree 1..7; 0 unless function_applied */
    double function_uncertainty;    /* [0, 1], higher is LESS certain; valid only when function is set */
    const char *predominant_strength; /* "strong" (IV, ii) or "weak" (iii, vi) for a diatonic predominant, from the
                                         library's harmony_predominant_strength; NULL for any other chord */
    const char *six_four;           /* the library's SixFourType name ("cadential", "passing", "neighbor",
                                       "arpeggiating") of a classified six-four chord; NULL otherwise */
    const char *phrase_model;       /* the library's phrase model label of the chord, as it abbreviates it: "Tb", "PD", "D"
                                       or "Te"; NULL for a chord with none (outside a labelled phrase, or not a chord
                                       of the model) */
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

/* How far a change goes toward the new key: the library's KeyChangeExtent, as confirmed by the cadences found
   (a modulation needs a cadence arriving in the new key; without one it is a tonicization however long). */
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
    bool cadence_confirmed; /* a cadence arriving in the new key confirmed the modulation (the library's
                               harmony_key_change_confirm); false for a tonicization, and for a modulation the
                               library kept on region length alone because it had no cadences to consult */
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

/* One prolongation from the library's standard-harmony analysis (analysis/analysis_prolongation.h): the harmony of
   entries[first_entry] lasts through the interior chords to the closing chord at entries[last_entry]. Entry
   indices are inclusive. */
typedef struct {
    const char *function;        /* the prolonged harmony's HarmonicFunction name: "tonic" or "dominant" */
    size_t first_entry;          /* the first chord of the prolonged harmony */
    size_t last_entry;           /* the first chord of the closing harmony */
    size_t interior_count;       /* the chords between them */
    const char **reasons;        /* owned array of interior_count library reason names (not owned strings), one per
                                    interior chord: "passing-six-four", "tonic-line", ... */
    bool in_region;              /* the first chord lies in a key region */
    size_t region;               /* index of that region in key_regions; valid only when in_region */
    double uncertainty;          /* [0, 1], higher is LESS certain: the least certain function of the chords in the
                                    span */
} AnalysisProlongation;

/* One place a recurring progression occurs: the chords of entries[first_entry..last_entry], inclusive. The run
   covers every chord of the pattern, a held chord (consecutive chords with the same base numeral) included; all of
   them lie in the key region of entries[first_entry]. */
typedef struct {
    size_t first_entry;
    size_t last_entry;
} AnalysisProgressionOccurrence;

/* A run of two to six labelled chords, by the base numerals of the library's Roman numerals, that occurs at least
   twice. Counting is the analyzer's; the numerals are the library's. */
typedef struct {
    char **pattern;                          /* owned array of owned base numerals, e.g. "I", "IV", "V" */
    size_t length;                           /* 2..6 */
    AnalysisProgressionOccurrence *occurrences; /* owned; in time order */
    size_t occurrence_count;                 /* how often the pattern occurs: at least 2 */
} AnalysisProgression;

/* How many times the harmony changes in one measure: the chord segments that start in it with a base numeral or a
   key region different from the segment before. */
typedef struct {
    int measure; /* as the library numbers measures (a pickup is 0) */
    size_t changes;
} AnalysisMeasureChanges;

/* How the harmonic rhythm moves into one cadence: the changes in the two measures before the arrival's measure (the
   approach) against the changes in the two measures before those. A window clipped by the start of the piece holds
   fewer measures, or none (then its mean is undefined). */
typedef struct {
    size_t cadence;          /* index into cadences */
    int arrival_measure;     /* the measure the arrival begins in */
    size_t before_changes;   /* in the two measures before the approach */
    size_t before_measures;  /* how many of them the piece has: 0..2 */
    size_t approach_changes; /* in the two measures before the arrival */
    size_t approach_measures;
} AnalysisCadenceApproach;

/* One phrase from the library's standard-form analysis (analysis/analysis_phrase.h), in time order. The phrases
   tile the analysed range: each begins where the one before ended. The span includes any extension after the
   phrase's cadence (a post-cadential extension or a codetta), which the measures and length do not. Measure
   numbers are the library's (a pickup is 0, and is not counted in the length). */
typedef struct {
    long start_numerator; /* the phrase's span, whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    size_t first_entry;   /* the chord segments that start inside the span: entries[first_entry .. + entry_count) */
    size_t entry_count;
    int first_measure;
    int end_measure;      /* the measure the phrase proper ends in: its cadence's arrival chord ends there */
    int last_measure;     /* end_measure, or the last measure of the extension */
    int length_measures;  /* the phrase proper, a pickup not counted */
    int extension_measures; /* last_measure - end_measure */
    bool has_cadence;
    size_t cadence;       /* index into cadences; valid only when has_cadence */
    const char *signal;   /* what ended the phrase: "cadence", "discontinuity" or "none" (the end of the input) */
    double uncertainty;   /* the library's, in [0, 1]; higher is LESS certain */
    const char *role;     /* where the phrase stands in the piece: "beginning" (the first), "ending" (the last) or
                             "middle". The library names no such role for a phrase, so this is position alone. */
    bool phrase_model_labeled; /* the library labelled the phrase's chords for the phrase model */
    size_t first_idea;    /* the phrase's ideas: ideas[first_idea .. + idea_count) */
    size_t idea_count;
    size_t first_subphrase; /* its subphrases: subphrases[first_subphrase .. + subphrase_count) */
    size_t subphrase_count;
} AnalysisPhrase;

/* One idea of a phrase from the library's standard-form analysis (analysis/analysis_ideas.h), in time order. The span
   runs from the first measure to the end of the last (a pickup belongs to the first idea). */
typedef struct {
    size_t phrase;          /* index into phrases */
    int index;              /* within the phrase */
    long start_numerator;   /* whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    int first_measure;
    int last_measure;
    int length_measures;    /* counted measures */
    const char *label;      /* the library's IdeaLabel name: "basic-idea", "contrasting-idea", "fragment", "unit" */
    bool compared;          /* the idea was compared with the phrase's first idea (false for the first) */
    double similarity;      /* the share of matching intervals; valid only when compared */
    size_t *note_ids;       /* owned; ImportedNotes indices of the soprano notes of the idea */
    size_t note_id_count;
    double uncertainty;     /* the library's, in [0, 1] */
} AnalysisIdea;

/* One subphrase of a phrase of three or more ideas: its first two ideas, or the rest. */
typedef struct {
    size_t phrase;
    int index;              /* 0 or 1 */
    size_t first_idea;      /* index into ideas */
    size_t idea_count;
    long start_numerator;   /* whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    int first_measure;
    int last_measure;
    int length_measures;
    size_t *note_ids;       /* owned; ImportedNotes indices of the soprano notes of the subphrase */
    size_t note_id_count;
    double uncertainty;
} AnalysisSubphrase;

/* One part of a theme: a subphrase of a one-phrase theme (presentation and continuation) or a phrase of a two-phrase
   one (antecedent and consequent). Measures are the library's. */
typedef struct {
    const char *name;      /* the library's PhraseBeginning or PhraseEnding name; NULL for the parts of a repeated
                              phrase or a compound period, which the library does not name */
    int first_measure;
    int last_measure;
    int length_measures;
    bool has_cadence;      /* the part ends with a cadence: its phrase's (a presentation, a compound basic idea and
                              the first half of a two-phrase theme can have none) */
    size_t cadence;        /* index into cadences; valid only when has_cadence */
} AnalysisThemePart;

#define ANALYSIS_THEME_MAX_TRAITS 4

/* One theme from the library's standard-form analysis (analysis/analysis_theme.h), in the library's order (a compound
   period after its two sentences). The span runs from the first measure to the end of the last, an extension after
   the last phrase's cadence not included. */
typedef struct {
    int index;               /* the library's, within the piece */
    const char *form;        /* the library's PhraseLevelForm name: "sentence", "period", "hybrid", "compound-period",
                                "repeated-phrase" */
    const char *beginning;   /* the library's PhraseBeginning name; NULL for a repeated phrase or a compound period */
    const char *ending;      /* the library's PhraseEnding name; NULL likewise */
    size_t first_phrase;     /* the phrases it spans: phrases[first_phrase .. + phrase_count) */
    size_t phrase_count;     /* 1 or 2 */
    long start_numerator;    /* whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    int first_measure;
    int last_measure;
    int length_measures;
    bool parts_are_phrases;  /* the parts are phrases, not subphrases */
    const char *commonality; /* the library's StandardFormCommonality name of the pairing of the two parts ("very-common",
                                "slightly-less-common", "occasional", "rare"); NULL when the pairing has none */
    bool part_of_compound;   /* a sentence that is one half of a compound period */
    const char *traits[ANALYSIS_THEME_MAX_TRAITS]; /* the library's ContinuationTrait names the theme's continuation
                                shows: "fragmentation" is all its claim gives (acceleration, sequence and harmonic
                                rhythm are not in the claim) */
    size_t trait_count;
    bool archetypal;         /* the library's claim: a sentence of 4 or 8 measures whose continuation balances the
                                presentation and has fragmentation */
    bool cadences_usual;     /* the library's claim: a period's antecedent half cadence and consequent perfect authentic
                                cadence; a sentence's cadence is a goal */
    bool lengths_usual;      /* the library's claim: a four-measure presentation or antecedent, and an ending at least as
                                long */
    bool has_halves;         /* a compound period: its two halves are themes */
    size_t halves[2];        /* indices into themes of the two sentences; valid only when has_halves */
    AnalysisThemePart parts[2];
    size_t *note_ids;        /* owned; ImportedNotes indices of the soprano notes in the span */
    size_t note_id_count;
    double uncertainty;      /* the library's, in [0, 1]; higher is LESS certain */
} AnalysisTheme;

#define ANALYSIS_EXPANSION_MAX_TECHNIQUES 4

/* One expansion or contraction from the library's standard-form analysis (analysis/analysis_expansion.h), in the
   library's order (by theme, then phrase). An internal expansion or a contraction concerns the ending of a theme
   against its beginning; an external one concerns the extension after a phrase's cadence. */
typedef struct {
    int index;                 /* the library's, within the piece */
    const char *kind;          /* "internal", "external" (the library's ExpansionPlacement names) or "contraction"
                                  (its PhraseLengthChange name): the library's StandardFormExpansionKind has no
                                  name of its own */
    const char *change;        /* the library's PhraseLengthChange name: "expansion" or "contraction" */
    bool has_theme;            /* false for an external expansion of a phrase that is in no theme */
    size_t theme;              /* index into themes; valid only when has_theme */
    size_t phrase;             /* index into phrases: the phrase of the ending, or the extended phrase */
    int expected_length;       /* measures: the beginning's, or the phrase's own length when external */
    int actual_length;         /* measures: the ending's, or the phrase's with its extension */
    int measures_changed;      /* added (expansion) or removed (contraction), at least 1 */
    int first_measure;
    int last_measure;
    long start_numerator;      /* whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    const char *techniques[ANALYSIS_EXPANSION_MAX_TECHNIQUES]; /* the library's InternalExpansionTechnique names it
                                  finds evidence for; none when unspecified or not internal */
    size_t technique_count;
    const char *external_kind; /* the library's ExternalExpansionKind name; NULL unless external */
    bool suffix_length_typical;       /* external only: the extension's length is typical for its kind */
    bool suffix_agrees_with_location; /* external only: its length agrees with its location */
    size_t *note_ids;          /* owned; ImportedNotes indices of the soprano notes in the span */
    size_t note_id_count;
    double uncertainty;        /* the library's, in [0, 1]; higher is LESS certain */
} AnalysisExpansion;

/* One section of the form plan: consecutive phrases, grouped by the kernel's form structure detection
   (analysis/form_structure_detection.h) at the section-closing cadences (perfect and imperfect authentic) of the
   phrases. The sections tile the phrases: the phrases after the last closing cadence, which the kernel leaves out,
   form a last section with closed false. The label is the analyzer's bookkeeping over the thematic returns. */
#define ANALYSIS_SECTION_LABEL_SIZE 8

typedef struct {
    char label[ANALYSIS_SECTION_LABEL_SIZE]; /* letters by first appearance; a section whose opening theme returns
                                  an earlier section's takes that section's letter, with one more prime than the
                                  sections of the letter before it ("A", "A'", "A''") */
    size_t first_phrase;     /* the phrases it holds: phrases[first_phrase .. + phrase_count) */
    size_t phrase_count;
    long start_numerator;    /* whole notes: the first phrase's start to the last phrase's end */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    size_t first_entry;      /* the chord segments that start inside the span: entries[first_entry .. + entry_count) */
    size_t entry_count;
    int first_measure;       /* the library's measure numbers (a pickup is 0) */
    int last_measure;
    bool closed;             /* a section-closing cadence of the library's ended it */
    bool has_cadence;        /* the last phrase ends with a cadence */
    size_t cadence;          /* index into cadences; valid only when has_cadence */
    bool in_region;          /* the section's first chord lies in a key region */
    size_t region;           /* index of that region in key_regions; valid only when in_region */
    bool has_return;         /* the section's opening theme returns an earlier section's material */
    size_t returns_from;     /* index into sections of that earlier section; valid only when has_return */
    size_t return_index;     /* index into thematic_returns of the return that set the letter; valid only when
                                has_return */
    double uncertainty;      /* the greatest uncertainty of its phrases, in [0, 1]; higher is LESS certain */
} AnalysisSection;

/* A later theme recalling an earlier one: one "resembles" relationship of the kernel's thematic return detection
   (analysis/thematic_return_detection.h) over the themes' soprano notes, default config. Themes that share a phrase
   (a compound period and its sentences) are not returns of one another. */
typedef struct {
    size_t earlier_theme;      /* indices into themes */
    size_t later_theme;
    size_t earlier_section;    /* indices into sections: the section of each theme's first phrase */
    size_t later_section;
    double similarity;         /* the library's: the lower of the two scores, in [0, 1] */
    double contour_score;
    double rhythm_score;
    size_t earlier_offset;     /* the best stretch pair, as note offsets inside each theme */
    size_t later_offset;
    size_t window_length;      /* notes compared */
} AnalysisThematicReturn;

/* One occurrence of a motive family from the library's motive analysis (analysis/analysis_motive.h) over the
   note-level soprano: the highest sounding note at each attack. The first occurrence of a family is its prototype. */
typedef struct {
    size_t index;            /* within the family; 0 is the prototype */
    long start_numerator;    /* whole notes: the first note's start to the last note's end */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    int first_measure;       /* the library's measure numbers (a pickup is 0) */
    int last_measure;        /* the measure the last note starts in */
    double first_beat;       /* 1-based, in beats of the first measure */
    size_t *note_ids;        /* owned; ImportedNotes indices of the occurrence's soprano notes */
    size_t note_id_count;
    const char *kind;        /* the library's StandardFormMotiveKind name, relative to the family's prototype: "exact",
                                "real-transposed", "tonal-transposed", "inverted", "retrograde", "augmented",
                                "diminished", "fragmented" or "varied"; the prototype is "exact" */
    double score;            /* the library's, in [0, 1]: 1 minus mismatches over intervals, times its rhythm factor */
    int mismatches;          /* interval mismatches of a varied occurrence, else 0 */
    int intervals;           /* the intervals compared */
    const char *fragment;    /* a fragmented occurrence: "prefix" or "suffix" of the prototype; else NULL */
    bool in_continuation;    /* a fragment found inside a continuation span of a theme */
    bool has_phrase;         /* the whole occurrence lies in one of phrases; false when it crosses a phrase boundary */
    size_t phrase;           /* index into phrases; valid only when has_phrase */
    bool has_idea;           /* it lies in one idea of that phrase */
    size_t idea;             /* index into ideas; valid only when has_idea */
    double uncertainty;      /* the library's, 1 minus its score, in [0, 1]; higher is LESS certain */
} AnalysisMotiveOccurrence;

/* One motive family: a short figure that recurs. Its occurrences are the prototype first and then the library's order,
   which is not always time order (it searches continuations for fragments first); development is the time order. */
typedef struct {
    size_t index;            /* within the piece, in the library's order: by prototype, longer first */
    int note_count;          /* the prototype's */
    bool generic;            /* the pattern is the scale-step interval sequence (else the semitones) */
    AnalysisMotiveOccurrence *occurrences; /* owned */
    size_t occurrence_count;
    size_t *development;     /* owned; occurrence_count indices into occurrences in time order, earliest first: the
                                family's development through the piece. Each step's transformation is the occurrence's
                                kind (the library's, against the prototype); the analyzer only orders */
} AnalysisMotive;

/* One leap of a melody (an interval of a third or larger) and the library's verdict on its recovery
   (analysis_melody_leaps.h, the cantus firmus rule LEAP_RECOVERY): a leap of a fourth or larger should be followed by a
   step in the opposite direction. */
typedef struct {
    size_t from_note;          /* ImportedNotes indices of the two notes */
    size_t to_note;
    long semitones;            /* signed */
    const char *direction;     /* "up" or "down" */
    int generic_size;          /* the size in letter names (a third is 3); 0 when the library has none */
    bool approximate;          /* the library read it from the semitones, with no letter names or scale degrees */
    const char *status;        /* the library's name: "recovered", "unrecovered", "not-required" (a third) or
                                  "not-judged" (a fourth or larger with nothing after it) */
    double uncertainty;        /* the library's, in [0, 1]; higher is LESS certain */
} AnalysisMelodyLeap;

/* Two leaps in a row that the library's rule CONSECUTIVE_LEAPS finds fault with: the second starts where the first
   ends. */
typedef struct {
    size_t first_note;         /* ImportedNotes indices: the first leap is first to middle, the second middle to last */
    size_t middle_note;
    size_t last_note;
    bool same_direction;       /* the two go the same way and are not two thirds outlining a perfect fifth */
    bool three_in_a_row;       /* a third leap follows */
    double uncertainty;
} AnalysisMelodyConsecutiveLeaps;

/* The non-chord-tone types the melody counts, one per type of the library's NonChordTone. */
#define ANALYSIS_MELODY_NCT_TYPES 10

/* A melody note the library's chord identification found not to be a tone of its chord, with the type it gave
   (analysis_melody_nct.h attaches that result to the note; the melody adapter labels nothing itself). */
typedef struct {
    size_t note;               /* ImportedNotes index */
    const char *type;          /* the library's NonChordTone name, e.g. "passing" or "escape-tone" */
    double uncertainty;        /* the library's: 1 minus the note's confidence */
} AnalysisMelodyNct;

/* The library's melodic tendencies of a line (tendency_rules.h, after Huron): pitch proximity, step declination, step
   inertia, melodic regression and melodic arch. Soft preferences a well-formed line usually shows. */
#define ANALYSIS_MELODY_TENDENCIES 5

/* A seventh degree the library's rule LEADING_TONE finds the melody not to follow: the note, and the note after it. */
typedef struct {
    size_t note;               /* ImportedNotes indices */
    size_t next_note;
    double uncertainty;        /* the key region's, in [0, 1]: the degree is read in its key */
} AnalysisMelodyBreak;

/* The melody of one phrase: the soprano notes (the outer voice line, one entry per melody note) that start inside the
   phrase's span, read by the melody adapter of standard-counterpoint (analysis/analysis_melody.h). A note held in from
   the phrase before is not one of them, and a rest in the line breaks the chain: no interval is formed across it. The
   Analyzer counts and groups; the pitches, positions, directions and every verdict are the library's. */
typedef struct {
    size_t phrase;           /* index into phrases */
    const char *framework;   /* the library's framework name for the claims */
    long start_numerator;    /* the phrase's span, whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    int first_measure;       /* the phrase's, as the library numbers them (a pickup is 0) */
    int last_measure;
    size_t *note_ids;        /* owned; ImportedNotes indices of the melody notes, in time order */
    size_t note_id_count;
    long lowest_pitch;       /* MIDI note numbers: the first note at the lowest and at the highest pitch */
    size_t lowest_note;      /* ImportedNotes indices */
    char lowest_name[16];    /* e.g. "E5", spelled as the outer voice lines spell it */
    long highest_pitch;
    size_t highest_note;
    char highest_name[16];
    long range_semitones;    /* highest minus lowest */
    /* The climax (analysis_melody_climax.h): the highest pitch, and the first note at it. */
    size_t climax_note;      /* ImportedNotes index of the first note at the highest pitch */
    int climax_occurrences;  /* notes at the highest pitch */
    bool climax_unique;
    double climax_relative_position; /* the first occurrence's rank among the notes over (notes - 1), by note order */
    bool climax_has_strength;
    const char *climax_strength;     /* the library's BeatStrength name; valid only when climax_has_strength */
    bool climax_by_leap;     /* the first occurrence is approached by a leap */
    bool climax_final;       /* the first occurrence is the phrase's last note */
    /* The contour (analysis_melody.h): the library's direction of each interval between consecutive notes. */
    int up;
    int down;
    int same;
    int step_count;
    int leap_count;
    const char *net_direction; /* "up", "down" or "same": the last note against the first */
    long net_semitones;
    const char **directions;   /* owned array of direction_count names ("up", "down", "same"), one per interval, in
                                  time order; the names are static */
    size_t direction_count;
    AnalysisMelodyLeap *leaps; /* owned; every leap in time order */
    size_t leap_total;
    AnalysisMelodyConsecutiveLeaps *consecutive_leaps; /* owned; one per offending pair, in time order */
    size_t consecutive_leap_count;
    /* Non-chord tones: each melody note against the chord of the segment it starts in (the first of its segments). A
       note in a segment with no chord, or one the library left unclassified, is counted as unclassified. */
    int chord_tone_count;
    int unclassified_count;
    int non_chord_tone_count;
    const char *nct_type_names[ANALYSIS_MELODY_NCT_TYPES]; /* the library's NonChordTone names, in its order */
    int nct_by_type[ANALYSIS_MELODY_NCT_TYPES];            /* non-chord tones by type, parallel to the names */
    AnalysisMelodyNct *ncts;   /* owned; the non-chord tones in time order */
    size_t nct_total;
    /* Tendencies. The library's line tendencies need every note spelled (the line's letter names); the Analyzer spells a
       note as the outer voice lines do, so they are read for every phrase. */
    const char *tendency_names[ANALYSIS_MELODY_TENDENCIES]; /* the library's names, in its order */
    bool tendency_holds[ANALYSIS_MELODY_TENDENCIES];
    /* The seventh degree: each note the library labels degree 7 that another note follows is judged by the cantus
       firmus rule LEADING_TONE on the pair, in the key of the note's region: a leading tone goes up to the tonic by
       step, and a seventh a whole step below the tonic does not. followed + broken is the number judged. */
    const char *seventh_rule;  /* the library's rule name: "leading-tone" */
    int seventh_followed;
    int seventh_broken;
    AnalysisMelodyBreak *seventh_breaks; /* owned; seventh_broken of them, in time order */
    double uncertainty;      /* the library's, in [0, 1]; higher is LESS certain */
} AnalysisMelody;

/* One duration of a density profile's histogram: how many notes of the phrase have it, as an exact rational of a
   whole note. */
typedef struct {
    long numerator;
    unsigned long denominator;
    unsigned count;
} AnalysisDurationBin;

#define ANALYSIS_DENSITY_BINS 16

/* The rhythmic density of one phrase, for all its notes or for one line (the library's standard-meter/density claim,
   analysis/meter_analysis_density.h). The Analyzer only builds the library's input (every imported note, the line it
   belongs to, the chord changes and the phrases) and copies the profile; the counts and shares are the library's. A note
   belongs to a phrase when its onset, snapped to the library's grid, is in the phrase's span. The recurring rhythm
   patterns are the motive families' rhythms (motives), not a second detector. */
typedef struct {
    size_t phrase;           /* index into phrases */
    int line;                /* -1 all notes, 0 the outer-voice soprano, 1 the bass, 2 any other note */
    const char *framework;   /* the library's framework name for the claim */
    size_t *note_ids;        /* owned; ImportedNotes indices of the notes counted, in time order */
    size_t note_id_count;
    unsigned notes;          /* the library's counts: notes, and the distinct onsets among them */
    unsigned attacks;
    int measures;            /* the measures the phrase touches */
    double attacks_per_beat_mean; /* per measure, the attacks over the beats the phrase's part of it spans; the mean
                                     over the measures and the largest */
    double attacks_per_beat_max;
    double mean_inter_onset; /* whole notes; 0 with fewer than two attacks */
    double on_beat_share;    /* of the attacks, the share exactly on a beat and the rest */
    double off_beat_share;
    AnalysisDurationBin bins[ANALYSIS_DENSITY_BINS]; /* the notes' durations, ascending */
    unsigned bin_count;
    bool bins_truncated;     /* longer durations were left out of the bins */
    double uncertainty;      /* the library's: 0, the profile describes the notes as given */
} AnalysisDensity;

/* One passage of the texture: a run of measures the library classifies as the same texture (the standard-texture
   passage claim, analysis/texture_analysis_classify.h). The Analyzer only builds the library's input (every imported
   note, the voice kernel's separation per stream) and copies the passages and changes; the texture, the category and
   the measured features are the library's. A measure whose features fit no texture takes its neighbour's (counted in
   unclassified_windows). The features are the means over the passage's measures, the voice count the largest. */
typedef struct {
    const char *framework;   /* the library's framework name for the claim */
    int first_measure;       /* as the library numbers them (a pickup is 0) */
    int last_measure;
    long start_numerator;    /* the passage's span in whole notes: its first measure's start to its last measure's end */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    const char *texture;     /* the library's name: "monophony", "heterophony", "homophony", "homorhythm",
                                "melody and accompaniment" or "polyphony" */
    const char *category;    /* the texture's category: homorhythm and melody and accompaniment are homophony */
    int windows;             /* measures in the passage, and those that took their neighbour's texture */
    int unclassified_windows;
    size_t *note_ids;        /* owned; ImportedNotes indices of the notes attacked in the passage, in time order */
    size_t note_id_count;
    double confidence;       /* the library's: of the texture, of the voice separation, and both together */
    double texture_confidence;
    double voice_confidence;
    int voices_max;
    double voices_mean;
    double synchrony;
    double independence;
    double prominence;
    double top_gap;
    double register_span;
    double spacing;
    double density;          /* attack times per beat */
    double pattern;          /* the repeated-pattern score, 0 to 1: evidence for an accompaniment figure, not a name */
    double uncertainty;      /* the library's, in [0, 1]; higher is LESS certain */
} AnalysisTexturePassage;

/* A change of texture: the boundary between two consecutive passages (the library's standard-texture change claim). */
typedef struct {
    const char *framework;
    size_t from;             /* index into texture_passages; the passage changed to is from + 1 */
    size_t to;
    int boundary_measure;    /* the first measure of the passage changed to */
    const char *from_texture;
    const char *to_texture;
    bool category_changed;   /* false for homorhythm to melody and accompaniment, both homophony */
    double uncertainty;
} AnalysisTextureChange;

/* The motion of the voices, from the library's standard-counterpoint adapter (analysis/analysis_motion.h). The Analyzer
   separates the notes of the whole piece into voices with the kernel's voice separation (voice 0 is the top, all tracks
   and parts together), puts each voice's sounding note at the start of every chord segment into the library's voice-pitch
   grid (one row per chord segment, one column per voice, at most ANALYSIS_VOICE_LEADING_MAX_VOICES; lower voices are left
   out), and counts what the library classifies. The motion types are the library's: contrary, similar, parallel,
   oblique and static, between consecutive chord segments for every pair of voices that sound in both. */
#define ANALYSIS_VOICE_LEADING_MAX_VOICES 6
#define ANALYSIS_MOTION_TYPES 5

typedef struct {
    size_t upper_voice;                       /* the voice nearer the top; voice 0 is the top */
    size_t lower_voice;
    size_t counts[ANALYSIS_MOTION_TYPES];     /* contrary, similar, parallel, oblique, static (the library's order) */
    size_t total;
} AnalysisVoicePairMotion;

/* A fault the library finds in the voices' leading (standard-counterpoint's check_perfects): parallel fifths or
   octaves between a pair of voices (any pair), or direct fifths and octaves ("direct-perfects": arrival by similar
   motion with the top voice leaping, in the outer pair only). It is the move from chord segment entry_from to entry_to
   (entries of the report, consecutive). The notes are the upper voice's at each segment, then the lower voice's. */
typedef struct {
    const char *rule;         /* the library's: "parallel-fifths", "parallel-octaves" or "direct-perfects" */
    size_t upper_voice;
    size_t lower_voice;
    size_t entry_from;
    size_t entry_to;
    int arrival_interval;     /* the simple size arrived at: 5, 8 or 1 */
    size_t note_ids[4];       /* ImportedNotes indices: upper at from, upper at to, lower at from, lower at to */
    double uncertainty;       /* the library's: 1 less the least confidence of the voice separation, in [0, 1] */
} AnalysisVoiceViolation;

/* A tendency tone the library finds left unresolved when a chord changes (standard-harmony's check_resolutions): a
   seventh that does not fall, or a leading tone that does not rise to the tonic. The voice moves from the last segment
   of its chord (entry_from) to the first of the next (entry_to). */
typedef struct {
    const char *rule;         /* the library's: "seventh-resolves-down" or "leading-tone-resolves" */
    size_t voice;
    size_t entry_from;
    size_t entry_to;
    size_t note_ids[2];       /* ImportedNotes indices: the tone and the note it moved to */
    double uncertainty;       /* the library's: the voice's and the chord's confidence together, in [0, 1] */
} AnalysisVoiceResolution;

/* The doubling and spacing of one chord segment (a report entry) that three or more voices sound and the harmony stage
   read as a chord. Doubling is counted, with no verdict: the voices' pitch classes are placed in the chord's interval set
   (the library's, root first) by the chord's root, which is the bass's pitch class less the interval of the chord member
   in the bass. Spacing is the semitone distance between adjacent sounding voices, top to bottom; when the grid has four
   voices and all four sound, the library's standard-voicing spacing rule gives a verdict. */
#define ANALYSIS_CHORD_MEMBERS 4

typedef struct {
    size_t entry;                         /* the chord segment, an index into entries */
    size_t voices_sounding;
    size_t member_voices[ANALYSIS_CHORD_MEMBERS]; /* voices on each member of the chord's interval set: root, third, fifth,
                                                     seventh (positions in the set; 0 where the chord has fewer) */
    size_t non_chord_voices;              /* voices on a pitch class the chord does not have */
    int doubled_member;                   /* the member with the most voices when more than one; -1 when none */
    long gaps[ANALYSIS_VOICE_LEADING_MAX_VOICES]; /* semitones from each sounding voice down to the next, top first */
    size_t gap_count;
    bool spacing_judged;                  /* the library's spacing rule was applied (four voices, all sounding) */
    int wide_voice;                       /* when judged: -1 within the limits, else the upper voice (0, 1 or 2) of the
                                             first pair that is too wide */
    size_t note_ids[ANALYSIS_VOICE_LEADING_MAX_VOICES]; /* ImportedNotes indices, top voice first */
    size_t note_id_count;
    double uncertainty;                   /* 1 less the least confidence of the voice separation, in [0, 1] */
} AnalysisVoiceChord;

typedef struct {
    bool present;                             /* the library took the voice grid; false for a piece it declined */
    const char *framework;                    /* the library's framework name for the claims */
    size_t voice_count;                       /* columns of the grid */
    size_t row_count;                         /* chord segments, one row each */
    size_t motion_count;                      /* motions classified */
    size_t motion_totals[ANALYSIS_MOTION_TYPES];
    AnalysisVoicePairMotion *pairs;           /* owned; every pair, the upper voice first */
    size_t pair_count;
    double motion_uncertainty;                /* the mean of the library's, in [0, 1]; higher is LESS certain */
    AnalysisVoiceViolation *violations;       /* owned; in segment order, then voice pair */
    size_t violation_count;
    AnalysisVoiceResolution *resolutions;     /* owned; unresolved tones only, in order of chord change, then voice */
    size_t resolution_count;
    bool resolutions_checked;                 /* the library had prepared chords to check against */
    AnalysisVoiceChord *chords;               /* owned; in time order */
    size_t chord_count;
} AnalysisVoiceLeading;

/* A note held through a stronger metric position than it was attacked at, with no note attacked there (the library's
   THROUGH_STRONGER rule, analysis/meter_analysis_syncopation.h). Each line is read on its own. The levels are the
   library's StandardMeterLevel: "downbeat", "secondary-beat", "beat", "offbeat" or "finer"; the strength is the
   difference of the two (1 to 4). */
typedef struct {
    const char *framework;   /* the library's framework name for the claim */
    size_t note;             /* ImportedNotes index of the syncopated note */
    int line;                /* 0 the outer-voice soprano, 1 the bass, 2 any other note */
    int measure;             /* where it is attacked, as the library numbers them (a pickup is 0) */
    double beat;             /* 1-based, fractional */
    const char *attack_level;
    const char *through_level;
    unsigned strength;
    int through_measure;     /* the position it is held through */
    double through_beat;
    double uncertainty;      /* the library's: 0.1 when the snap moved the onset, otherwise 0 */
} AnalysisSyncopation;

/* A hemiola: six pulses read as three duple units (analysis/meter_analysis_hemiola.h). It is one when at least two of
   the evidence types that are available agree. An evidence value is 1 when it agrees, 0 when it does not and -1 when it
   is unavailable (the chord evidence, with no chord changes). */
typedef struct {
    const char *framework;
    int first_measure;       /* the window: two measures of a simple triple meter, or one of 6/8 */
    int last_measure;
    int duration_evidence;
    int chord_evidence;
    int accent_evidence;
    unsigned agreeing;
    unsigned available;
    size_t *note_ids;        /* owned; ImportedNotes indices of the notes attacked on the window's pulses */
    size_t note_id_count;
    double uncertainty;      /* the library's: 1 - agreeing / 3 */
} AnalysisHemiola;

/* One hypermeasure of a phrase (analysis/meter_analysis_hypermeter.h). The kind is the library's
   StandardMeterHypermeasureKind: "regular", "short", "expanded", "extension" or "contracted". */
typedef struct {
    int index;               /* in the phrase, from 0 */
    int first_measure;
    int last_measure;
    int measures;
    const char *kind;
    int extra_hyperbeats;    /* measures - the phrase's nominal length: positive when expanded, negative when short or
                                contracted */
    bool contains_cadence;   /* the cadence arrival is in this hypermeasure */
    double score;
    double uncertainty;      /* the library's: 1 - score */
} AnalysisHypermeasure;

/* The hypermeter of one phrase: the hypermeasure length (2 or 4 measures) that fills the phrase and holds the cadence
   arrival in its last hypermeasure, and the hypermeasures that follow one another from the phrase's first measure
   (the library fills the last one to its length, so it can run past the phrase's measures). The first measure of each is
   a hyperdownbeat. Elided cadences are not reported by the form adapter, so no phrase is read as elided and the library
   reports no reinterpretation. */
typedef struct {
    const char *framework;
    size_t phrase;           /* index into phrases */
    int first_measure;       /* the phrase's first full measure (a pickup is not counted) */
    int measures;            /* of the phrase's span, its extension included */
    unsigned length;         /* the hypermeasure length chosen: 2 or 4 */
    double score;
    bool has_arrival;
    int arrival_measure;     /* valid only when has_arrival */
    AnalysisHypermeasure *hypermeasures; /* owned; in time order */
    size_t hypermeasure_count;
    size_t *note_ids;        /* owned; ImportedNotes indices of the notes in the phrase */
    size_t note_id_count;
} AnalysisHypermeter;

/* A run of tuplet notes: consecutive attacks of the piece that divide the meter's steps the way it does not. The
   Analyzer finds them from the notes' times alone (the importers carry no notated tuplets, R-056): times are rounded
   to a 1/96 grid (a time further than a quarter of a step from it is not read), and a note whose attack or length is
   not a whole step of the library's 1/32 grid is a triplet note (3:2, which includes sextuplets); in a compound meter
   two notes that halve a beat are a duplet (2:3). Other ratios are not looked for. Naming the ratio borrowed, and
   the level it occurs at, is the library's (borrowed_rules.h). */
typedef struct {
    const char *framework;   /* the library's framework name for the claim */
    unsigned actual;         /* the ratio actual:normal, e.g. 3:2 */
    unsigned normal;
    const char *name;        /* "triplet", "duplet" (the only ratios the library names) or "tuplet" */
    const char *meter;       /* the meter of the first note: "simple" or "compound" */
    bool borrowed;           /* the library's BORROWED rule: a triplet in a simple meter or a duplet in a compound one */
    bool has_level;          /* the library's LEVEL rule could place the group */
    const char *level;       /* "beat", "subdivision" or "multi-beat"; valid only when has_level */
    bool level_typical;      /* the library's TYPICAL_LEVEL rule; valid only when has_level */
    int measure;             /* of the first note */
    double beat;
    unsigned attacks;        /* distinct attacks in the run, at least 2 */
    size_t *note_ids;        /* owned; ImportedNotes indices, in time order */
    size_t note_id_count;
} AnalysisTuplet;

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
    AnalysisProlongation *prolongations; /* owned; in order of first_entry */
    size_t prolongation_count;
    AnalysisProgression *progressions; /* owned; the recurring progressions, ranked: most occurrences, then longest, then
                                          earliest */
    size_t progression_count;
    AnalysisMeasureChanges *harmonic_rhythm; /* owned; one per measure from the first chord's to the last chord's,
                                                every measure between included; empty when the piece has no chords or
                                                the library cannot place them on the meter */
    size_t harmonic_rhythm_count;
    AnalysisCadenceApproach *cadence_approaches; /* owned; one per cadence, in the cadences' order; empty when
                                                    harmonic_rhythm is */
    size_t cadence_approach_count;
    AnalysisPhrase *phrases; /* owned; in time order; empty when the library found none */
    size_t phrase_count;
    AnalysisIdea *ideas; /* owned; phrase by phrase, in time order */
    size_t idea_count;
    AnalysisSubphrase *subphrases; /* owned; phrase by phrase */
    size_t subphrase_count;
    AnalysisTheme *themes; /* owned; the library's themes, in its order; empty when it found none */
    size_t theme_count;
    AnalysisExpansion *expansions; /* owned; the library's expansions and contractions, in its order */
    size_t expansion_count;
    AnalysisSection *sections; /* owned; tile the phrases, in time order; empty when there are no phrases */
    size_t section_count;
    AnalysisThematicReturn *thematic_returns; /* owned; by later theme, then earlier theme; empty when the library
                                                 found none */
    size_t thematic_return_count;
    AnalysisMotive *motives; /* owned; the library's motive families, in its order; empty when it found none */
    size_t motive_count;
    AnalysisMelody *melodies; /* owned; one per phrase with melody notes, in phrase order */
    size_t melody_count;
    AnalysisDensity *densities; /* owned; per phrase, all notes first and then each line that has a note in it */
    size_t density_count;
    AnalysisSyncopation *syncopations; /* owned; each line's in onset order, the lines in order */
    size_t syncopation_count;
    AnalysisHemiola *hemiolas; /* owned; in time order */
    size_t hemiola_count;
    AnalysisHypermeter *hypermeters; /* owned; one per phrase that has a measure, in phrase order */
    size_t hypermeter_count;
    AnalysisTuplet *tuplets; /* owned; grouped by ratio (the smaller actual count first), each in time order */
    size_t tuplet_count;
    AnalysisTexturePassage *texture_passages; /* owned; in time order; empty when the library found no texture */
    size_t texture_passage_count;
    AnalysisTextureChange *texture_changes; /* owned; texture_passage_count - 1 of them */
    size_t texture_change_count;
    AnalysisVoiceLeading voice_leading; /* the voices' motion; present is false when the library declined the grid */
    size_t home_region; /* first region in the home key; valid only when key_region_count > 0 */
    long home_duration_numerator; /* total time in the home key across its regions, whole notes */
    unsigned long home_duration_denominator;
} AnalysisReport;

/* Frees every entry's labels and note ids, the entries array, the key regions and changes, the cadences, the
   prolongations, the progressions, the harmonic rhythm, the phrases, ideas, subphrases, themes, expansions, sections,
   thematic returns, motives, melodies, densities, syncopations, hemiolas, hypermeters, tuplets, texture passages and changes
   and the voice leading. */
void analysis_report_destroy(AnalysisReport *report);

#endif
