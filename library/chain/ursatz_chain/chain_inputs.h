#ifndef URSATZ_CHAIN_CHAIN_INPUTS_H
#define URSATZ_CHAIN_CHAIN_INPUTS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/outer_voice.h"
#include "analysis/voice_separation.h"
#include "standard-form/analysis/analysis_expansion.h"
#include "standard-counterpoint/analysis/analysis_melody.h"
#include "standard-form/analysis/analysis_phrase.h"
#include "standard-harmony/analysis/analysis_cadence.h"
#include "standard-tonality/analysis/chord_input.h"
#include "galant-schemas/analysis/galant_analysis_input.h"
#include "common-practice-minimal/key_regions.h"
#include "standard-meter/analysis/meter_analysis_input.h"
#include "standard-texture/analysis/texture_analysis_input.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_note.h"
#include "semantics/beat_strength.h"
#include "semantics/voice_grid.h"
#include "semantics/sonority_segmentation.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "common-practice-minimal/weighted_chord_identification.h"

/*
 * The chain's input builders (R-241): each turns neutral notes (semantics/analysis_note.h) and the results of an earlier stage into the
 * plain input one framework reads. Every default is the library's; nothing is chosen here. Each builder owns what it allocates and has a
 * matching destroy; a failed builder leaves its output untouched.
 */

typedef enum {
    CHAIN_INPUT_OK = 0,
    CHAIN_INPUT_ERR_NULL_ARGUMENT,
    CHAIN_INPUT_ERR_BAD_NOTES,    /* an invalid note set, or no note */
    CHAIN_INPUT_ERR_NO_EVENT,     /* a note without its NoteEvent, which segmentation needs */
    CHAIN_INPUT_ERR_SEGMENTATION, /* adaptive segmentation failed */
    CHAIN_INPUT_ERR_UNKNOWN_NOTE, /* a segment names a note that is not in the set */
    CHAIN_INPUT_ERR_MEMBER,       /* a note that does not sound in a segment it belongs to */
    CHAIN_INPUT_ERR_WEIGHTS,
    CHAIN_INPUT_ERR_KEYS,
    CHAIN_INPUT_ERR_ALLOC
} ChainInputStatus;

/*
 * The segments of a piece: the adaptive sonority segments (the owner, sonority_builder_build_adaptive, on the standard-meter beat grid),
 * each one's id ("seg-<n>") and window in whole notes, and, once chain_build_members has run, the members of every segment.
 */
typedef struct {
    AdaptiveSonorities sonorities;       /* owned */
    char (*ids)[32];                     /* owned; count ids */
    double *starts;                      /* owned; each segment's window in whole notes */
    double *ends;
    size_t count;
    CommonPracticeMember *members;       /* owned; every segment's members, one after another; NULL before chain_build_members */
    CommonPracticeSegment *segments;     /* owned; count entries viewing members; NULL before chain_build_members */
} ChainSegments;

/* Segments the notes. config may be NULL for the library's defaults. Every note needs its event. */
ChainInputStatus chain_build_segments(const AnalysisNoteSet *notes, const AdaptiveSegmentationConfig *config, ChainSegments *out);

/*
 * The strength of an attack at `onset`, read in the signature of its measure (standard-meter's beat strength of the metrical position);
 * weak when it cannot be read. This is the one function the test glue copied twice (weighted_pipeline.h and chorale_chain.c).
 */
BeatStrength chain_attack_strength(const MeterMap *map, const MusicalTime *onset);

/*
 * The members of every segment, through common_practice_member_from_analysis_note: the pitch (midi), the duration sounding inside the
 * segment, the held flag the segmentation recorded, the BeatStrength of the attack (strong for a held note), the voice id and the
 * onset. `notes` must be the set the segments were built from.
 */
ChainInputStatus chain_build_members(const AnalysisNoteSet *notes, ChainSegments *segments);

/* Destroys the sonorities, ids, windows, members and segments. NULL is a no-op; the struct is zeroed. */
void chain_segments_destroy(ChainSegments *segments);

/* The weighted key segment of each of the first `count` segments (window, weights, source members) for key_regions_estimate. weighting
   may be NULL for the defaults. Needs the members. Free with chain_key_segments_destroy. */
ChainInputStatus chain_build_key_segments(const ChainSegments *segments, size_t count, const CommonPracticeWeightedConfig *weighting,
                                          CommonPracticeKeySegment **out);
void chain_key_segments_destroy(CommonPracticeKeySegment *key_segments);

/*
 * The local key regions (the owner, key_regions_estimate) with the key signature events as AnalysisKeyEvents. events may be NULL with
 * event_count 0. config may be NULL for the defaults. Destroy with common_practice_key_regions_destroy.
 */
ChainInputStatus chain_estimate_key_regions(const CommonPracticeKeySegment *key_segments, size_t count, const MeterMap *map,
                                            const AnalysisKeyEvent *events, size_t event_count,
                                            const CommonPracticeKeyRegionsConfig *config, CommonPracticeKeyRegions *out);

/*
 * The outer lines on the chord segments. The outer voices are read on the voice segments (one per melody note, not merged), the harmony,
 * form, meter, galant and melody builders index them by chord segment, so this maps them: entry i of both lines is the entry of the voice
 * segment that sounds at the start of chord segment i of the first `count` of `segments` (the last voice segment starting at or before it;
 * absent, over the chord segment's window, when none does). `lines` are the lines of `voice_segments`. The entries are copies, with their own
 * note id, pitch and span, and segment_index is the chord segment's; the result is freed with outer_voice_lines_destroy.
 */
ChainInputStatus chain_build_segment_lines(const ChainSegments *voice_segments, const OuterVoiceLines *lines, const ChainSegments *segments,
                                           size_t count, OuterVoiceLines *out);

/*
 * The inputs of standard-harmony's cadence adapter, one HarmonySegmentInput per segment, as the Roman numerals and the outer voices read them
 * (HarmonySegmentInput, analysis_input.h): the segment (through harmony_segment_input_set_segment), the numeral's degree, intervals,
 * inversion, applied target, mode and uncertainty, and the degrees of the outer lines' notes in the local key (0 for a chromatic or an
 * absent note). A segment with no numeral is passed as not a chord (degree 0). One beat length, the first beat of measure 1's, serves
 * every segment.
 *
 * `chords` and `romans` are the standard-tonality chord inputs and Roman results of the first `count` segments, `lines` the outer lines
 * of the same segmentation (count entries at least): on the chord segments (chain_build_segment_lines), not the voice segments. The result borrows from them and from the segments and map.
 */
typedef struct {
    HarmonySegmentInput *inputs;  /* owned; count entries */
    MetricalPosition **positions; /* owned */
    Duration *beat_length;        /* owned */
    size_t count;
} ChainHarmonyInputs;

ChainInputStatus chain_build_harmony_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                              const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                              const OuterVoiceLines *lines, ChainHarmonyInputs *out);
void chain_harmony_inputs_destroy(ChainHarmonyInputs *inputs);

/*
 * Dominants before a section start are half cadences of the earlier section (R-038). A section starts where the key regions come back to
 * `home_key_index` (0..23, the key_index of a region) after leaving it; the regions are the ones the chords were read in. Adds the
 * boundary readings to `cadences` (harmony_analysis_add_boundary_cadences). `prepared` is the output of harmony_analysis_prepare over
 * the same `count` segments.
 */
ChainInputStatus chain_add_boundary_cadences(const HarmonySegment *prepared, size_t count, const CommonPracticeKeyRegions *regions,
                                             int home_key_index, HarmonyCadences *cadences);

/*
 * The key changes' extents confirmed by the cadences found (B5): a region is a modulation only when a cadence arrives inside it, and the
 * change out of an excursion takes the excursion's extent. `segment_starts` is each segment's start in whole notes
 * (ChainSegments.starts), indexed like the cadences' arrivals.
 */
ChainInputStatus chain_confirm_key_changes(CommonPracticeKeyRegions *regions, const double *segment_starts, const HarmonyCadences *cadences);

/*
 * The notes' events and sounding pitches as the kernel's analyses take them (outer_voice_extract, voice_separate): a NoteEvent array
 * parallel to the note set (borrowed) and a PitchNumericValue per note under one MIDI tuning system (owned here, the semitone number as the
 * position, as the importers make them). Every note needs its event and a known pitch.
 */
typedef struct {
    NoteEvent **events;             /* owned array of borrowed events; count entries, in the note set's order */
    PitchNumericValue **values;     /* owned; parallel to events */
    TuningSystem *tuning;           /* owned */
    size_t count;
} ChainPitches;

ChainInputStatus chain_build_pitches(const AnalysisNoteSet *notes, ChainPitches *out);
void chain_pitches_destroy(ChainPitches *pitches);

/*
 * The input of standard-form's adapter (analysis_input.h) as the chain builds it: spans copied for the input to borrow, kept here.
 */
typedef struct {
    StandardFormInput input;
    TimeSpan **spans;     /* owned: every span the input borrows */
    size_t span_count;
    size_t span_capacity;
} ChainFormInput;

/*
 * The form input over the measures [first_measure, last_measure] (MeterMap numbers): the soprano of the outer lines as notes (consecutive
 * segments on one note id are one note), every segment starting in the measures as a segment, and the cadences found (NULL for none),
 * each arriving over the windows of its first to last arrival segment, then finished. `lines` are the outer lines on the chord segments
 * `segments` (chain_build_segment_lines; the notes' events are found in `notes` by id). Destroy with chain_form_input_destroy.
 */
ChainInputStatus chain_build_form_input(const AnalysisNoteSet *notes, const ChainSegments *segments, const OuterVoiceLines *lines,
                                        int first_measure, int last_measure, const HarmonyCadences *cadences, ChainFormInput *out);

/*
 * The form input for the motive analysis (analysis_motive.h): a NOTE-LEVEL soprano, which the outer-voice line (one note per segment)
 * cannot give. The soprano is the highest sounding note at each attack: at every onset, among the notes sounding then, the highest (the
 * later attack among equals); it is a soprano note when it is attacked then. Each note carries its scale degree in the key on
 * `tonic_pitch_class` (a chromatic note its altered degree). Only notes starting in the measures [first_measure, last_measure]. The
 * cadences (NULL for none) are added over the windows of `segments`. Finished.
 */
ChainInputStatus chain_build_motive_input(const AnalysisNoteSet *notes, const ChainSegments *segments, int first_measure,
                                          int last_measure, int tonic_pitch_class, CommonPracticeKeyMode mode,
                                          const HarmonyCadences *cadences, ChainFormInput *out);
void chain_form_input_destroy(ChainFormInput *input);

/*
 * The input of standard-meter's adapter (meter_analysis_input.h): the notes starting in the measures [first_measure, last_measure] (line 0
 * for a note of the outer soprano, 1 for the bass, 2 for any other), a chord change at the start of every segment whose chord label
 * differs from the one before (both non-NULL; `chord_labels` has one entry per segment, compared as text), and the phrases of the form
 * adapter (a span with its extension, the arrival chord of its cadence, its length change). `form`, `phrases` and `expansions` may be NULL
 * for none. The form adapter reports no elided cadences, so a detected phrase is never elided.
 */
typedef struct {
    StandardMeterInput input;
    TimeSpan **spans;      /* owned: every span the input borrows */
    size_t span_count;
    size_t span_capacity;
    MusicalTime **times;   /* owned: every time the input borrows */
    size_t time_count;
    size_t time_capacity;
} ChainMeterInput;

/* The meter input over the measures [first_measure, last_measure]; `lines` are the outer lines on the chord segments (chain_build_segment_lines). */
ChainInputStatus chain_build_meter_input(const AnalysisNoteSet *notes, const OuterVoiceLines *lines, const ChainSegments *segments,
                                         const char *const *chord_labels, int first_measure, int last_measure,
                                         const StandardFormInput *form, const StandardFormPhrases *phrases,
                                         const StandardFormExpansions *expansions, ChainMeterInput *out);
void chain_meter_input_destroy(ChainMeterInput *input);

/*
 * The input of standard-texture's adapter (texture_analysis_input.h): every note with the voice the kernel's rule-based voice separation
 * (voice_separate, L2.7) gives it. The separation runs per stream, and a note's stream is its track (a MIDI track chunk is usually a hand);
 * a note with no track is in stream 0. The note's voice is the separation's index inside its stream (0 the top) and its confidence the
 * separation's. Destroy with chain_texture_input_destroy.
 */
typedef struct {
    StandardTextureInput input;
    VoiceAssignment assignment;  /* owned; one entry per note, in the note set's order */
    size_t stream_count;
} ChainTextureInput;

ChainInputStatus chain_build_texture_input(const AnalysisNoteSet *notes, const ChainPitches *pitches, ChainTextureInput *out);
void chain_texture_input_destroy(ChainTextureInput *input);

/*
 * The voice grid (semantics/voice_grid.h) both counterpoint and harmony read: each voice's sounding note in each of the first `count`
 * segments. The voices are the kernel's rule-based separation (voice_separate, L2.7) of ALL notes as one stream, so voice 0 is the top and
 * the last voice the bottom. A note is in a segment's row when it sounds at the segment's start (attacked when it starts there); two notes of
 * one voice in a segment keep the first. Cells borrow the pitches, spellings and ids of `pitches` and `notes`. Destroy with
 * chain_voice_grid_destroy.
 */
typedef struct {
    VoiceAssignment assignment;    /* owned; one entry per note, in the note set's order */
    AnalysisVoiceCell *cells;      /* owned; count * voice_count */
    const char **segment_ids;      /* owned array of borrowed ids */
    AnalysisVoiceGrid grid;        /* views the arrays above */
} ChainVoiceGrid;

ChainInputStatus chain_build_voice_grid(const AnalysisNoteSet *notes, const ChainPitches *pitches, const ChainSegments *segments,
                                        size_t count, ChainVoiceGrid *out);
void chain_voice_grid_destroy(ChainVoiceGrid *grid);

/*
 * The melody line of standard-counterpoint (analysis_melody.h) from one outer line (the soprano or the bass of the outer lines), over the
 * first `count` segments:
 *   - consecutive segments on one note id are ONE note (held across segments); a note repeated at the same pitch is a second note.
 *     Segments where the line is absent become ONE rest.
 *   - pitch: the line's PitchNumericValue (borrowed from the lines). No spelling.
 *   - degree and alteration: the scale degree label of the pitch class in the key on `tonic_pitch_class` and `mode`.
 *   - strength: the BeatStrength of the note's attack, from the segment members.
 *   - nct: from the chord result of the note's FIRST segment (`results`, one per segment): no chord match gives UNCLASSIFIED; a note listed
 *     among the chord's leftover tones gives NON_CHORD_TONE with its NonChordTone (UNCLASSIFIED when it was left unclassified); a note not
 *     listed gives CHORD_TONE. nct_ref is "<segment id>/<note id>".
 * `entries` are one outer line on the chord segments (chain_build_segment_lines), `count` entries.
 * `segment[i]` is the first segment of entry i. Destroy with chain_melody_line_destroy.
 */
typedef struct {
    StandardCounterpointMelodyLine line; /* owned */
    size_t *segment;                     /* owned; line.count entries */
} ChainMelodyLine;

ChainInputStatus chain_build_melody_line(const ChainSegments *segments, size_t count, const OuterVoiceEntry *entries,
                                         const CommonPracticeWeightedResult *results, int tonic_pitch_class,
                                         CommonPracticeKeyMode mode, ChainMelodyLine *out);
void chain_melody_line_destroy(ChainMelodyLine *melody);

/*
 * The segments of the schema and sequence adapters (galant_analysis_input.h) from the first `count` segments, their outer lines, the local
 * key of each (its chord input's tonic and mode) and its Roman numeral. A pitch class is a degree of a key by its offset above
 * the tonic, with the alteration against the major scale: 0 1 2 3 4 5 6 7 8 9 10 11 -> 1  2- 2  3- 3  4  4+ 5  6- 6  7- 7. The key's own
 * tonic is a degree of the home key (`home_key_index`, 0..23) by the same table. Consecutive segments with the same key, outer-voice
 * degrees and numeral (a held chord) are ONE segment, with the uncertainty of the least certain. A segment's uncertainty is its numeral's
 * (0.5 with none). `lines` are on the chord segments (chain_build_segment_lines). The root of an applied chord is the sounding root, from the tonicized degree and the chord's degree. Destroy with
 * chain_galant_segments_destroy.
 */
typedef struct {
    GalantSegmentList list;  /* owned */
    TimeSpan **spans;        /* owned: the spans of merged segments */
    size_t span_count;
    size_t span_capacity;
} ChainGalantSegments;

ChainInputStatus chain_build_galant_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                             const OuterVoiceLines *lines, int home_key_index,
                                             const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                             ChainGalantSegments *out);
void chain_galant_segments_destroy(ChainGalantSegments *segments);

#endif
