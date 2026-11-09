#ifndef URSATZ_CHAIN_ANALYSIS_CHAIN_H
#define URSATZ_CHAIN_ANALYSIS_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/outer_voice.h"
#include "analysis/voice_separation.h"
#include "standard-harmony/analysis/analysis_cadence.h"
#include "standard-form/analysis/analysis_expansion.h"
#include "standard-counterpoint/analysis/analysis_grid.h"
#include "standard-form/analysis/analysis_ideas.h"
#include "standard-counterpoint/analysis/analysis_melody.h"
#include "standard-counterpoint/analysis/analysis_melody_climax.h"
#include "standard-counterpoint/analysis/analysis_melody_leaps.h"
#include "standard-counterpoint/analysis/analysis_melody_nct.h"
#include "standard-counterpoint/analysis/analysis_motion.h"
#include "standard-form/analysis/analysis_motive.h"
#include "standard-counterpoint/analysis/analysis_perfects.h"
#include "standard-form/analysis/analysis_phrase.h"
#include "standard-form/analysis/analysis_phrase_grid.h"
#include "standard-form/analysis/analysis_repeats.h"
#include "standard-harmony/analysis/analysis_resolution.h"
#include "standard-form/analysis/analysis_theme.h"
#include "standard-tonality/analysis/chord_input.h"
#include "galant-schemas/analysis/galant_analysis_match.h"
#include "common-practice-minimal/key_regions.h"
#include "standard-meter/analysis/meter_analysis_density.h"
#include "standard-meter/analysis/meter_analysis_hemiola.h"
#include "standard-meter/analysis/meter_analysis_hypermeter.h"
#include "standard-meter/analysis/meter_analysis_syncopation.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"
#include "scale-harmonization/analysis/scale_analysis_detect.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_note.h"
#include "semantics/sonority_segmentation.h"
#include "standard-texture/analysis/texture_analysis_classify.h"
#include "ursatz_chain/chain_inputs.h"
#include "common-practice-minimal/weighted_chord_identification.h"

/*
 * The production analysis chain (R-217, R-241): one run from neutral notes to every analysis the libraries own, in the order of
 * dependency-spec.md "Analysis chain":
 *
 *   segments            adaptive sonority segments and their members (the owner of segmentation)
 *   keys                local key regions, with the key signature events (key_regions_estimate)
 *   chords              weighted chord identification in each region's own key and mode, a minor region in the minor vocabulary
 *   numerals            Roman numerals in the regions' keys (standard-tonality)
 *   outer voices        the soprano and bass lines of the segmentation that keeps every melody note (kernel analysis)
 *   cadences            the cadences, with the boundary cadences at section starts, and the key changes they confirm (standard-harmony)
 *   form                phrases, ideas, themes, expansions (standard-form, the phrase detector injected from the kernel), and the motive
 *                       families on a note-level soprano (the window matcher injected from the kernel)
 *   meter               syncopations, hemiolas, hypermeter and density (standard-meter)
 *   texture             windows, passages and changes over the separated voices (standard-texture)
 *   voice leading       motions and perfect-interval violations (standard-counterpoint), resolutions (standard-harmony)
 *   melody              the soprano as a melody: intervals, contour, climax, leap recovery, non-chord tones (standard-counterpoint)
 *   schemas             galant schemas (galant-schemas) and scale sequences (scale-harmonization)
 *
 * KEYS AND PHRASES. The key regions depend on the phrases, which depend on the cadences, which depend on the keys. With the form stage the
 * run goes through the stages once for the keys from the pitch classes alone; the chords of that pass are then evidence for the keys and
 * the phrase ends their boundaries, and every stage from the keys on runs again on that answer (config.refine_keys). No further iteration.
 *
 * ONE-STAGE RUNS. A stage pulls in the stages it needs: keys pull segments; chords keys; numerals chords; cadences numerals and outer
 * voices; form cadences; meter form, chords and outer voices; voice leading cadences; melody chords and outer voices; schemas numerals and
 * outer voices. Texture reads only the notes. Accessors of a stage that did not run return NULL.
 *
 * The notes, their events and the key events are borrowed and must outlive the chain. AnalysisChain owns every result.
 */

#define ANALYSIS_CHAIN_STAGE_SEGMENTS      (1u << 0)
#define ANALYSIS_CHAIN_STAGE_KEYS          (1u << 1)
#define ANALYSIS_CHAIN_STAGE_CHORDS        (1u << 2)
#define ANALYSIS_CHAIN_STAGE_NUMERALS      (1u << 3)
#define ANALYSIS_CHAIN_STAGE_OUTER_VOICES  (1u << 4)
#define ANALYSIS_CHAIN_STAGE_CADENCES      (1u << 5)
#define ANALYSIS_CHAIN_STAGE_FORM          (1u << 6)
#define ANALYSIS_CHAIN_STAGE_METER         (1u << 7)
#define ANALYSIS_CHAIN_STAGE_TEXTURE       (1u << 8)
#define ANALYSIS_CHAIN_STAGE_VOICE_LEADING (1u << 9)
#define ANALYSIS_CHAIN_STAGE_MELODY        (1u << 10)
#define ANALYSIS_CHAIN_STAGE_SCHEMAS       (1u << 11)
#define ANALYSIS_CHAIN_STAGE_ALL           ((1u << 12) - 1u)

/*
 * The seventh penalty (CommonPracticeWeightedConfig.seventh_penalty) of the chord identification on harmonic spans (R-376). A span reads
 * the union of a beat or more, so a passing tone completes a seventh over a triad; with this charge a seventh outranks its own triad only
 * when its seventh pitch class carries more than 15% of the span's weight. Used only when harmonic_spans is on, by the chord identification
 * and by the grouping (R-379 (1)), which reads a part as one segment and would otherwise let a passing tone complete a seventh there too;
 * the key segments and the key regions keep the weighting config as given.
 */
#define ANALYSIS_CHAIN_SPAN_SEVENTH_PENALTY 0.15

/*
 * One field per stage config; analysis_chain_config_default() fills every one with the library's default. `segmentation` cuts the segments
 * the chords are read in; the outer voices use the same config with merge_unchanged_harmony off (one segment per melody note).
 *
 * first_measure / last_measure: the part of the piece analysed (MeterMap measure numbers). The segments, keys, chords, numerals, cadences,
 * voice leading, melody and schemas run over the segments that START before the end of last_measure (last_measure 0: all); the form,
 * meter and motive stages read the notes and segments of the measures first_measure to last_measure.
 *
 * PHRASES FIRST. With phrase_grid (default on; R-389 to R-391) the final pass of the form stage, the second pass of the key refinement
 * (the only pass when there is none), finds the phrases first: the grid of units of 4, 8 or 16 measures is read from the soprano and the
 * cadences found, the cadences are claimed at the grid's points (R-390), and phrases and themes are segmented on the grid (R-391). The
 * first pass of the refinement keeps the cadence-ended phrases, so the key regions do not move. Off, every pass runs as before.
 *
 * KEYS FROM PHRASES (R-397, R-398; keys_from_phrases, default on). After the second pass a third reads the keys again, now placed on the
 * phrases of the grid the second pass found: a key changes only when a phrase establishes the new key, the cadences the second pass claimed
 * are the evidence, and exact repeats read the same. Everything from the keys on runs once more on that answer. It runs only with the form
 * stage, refine_keys and phrase_grid, and only when the second pass found a grid. Off, the chain is as it was before it.
 *
 * refine_with_given_boundaries: with no form stage there are no phrases to bound the second key pass; when set (and refine_keys is on) the
 * second pass runs anyway, on the first pass's chords as evidence and key_regions.phrase_boundaries as the boundaries (possibly none), as a
 * caller that finds its own phrase ends does (the accuracy harness). The boundaries are borrowed and must outlive the run. Default off.
 *
 * inject_kernel_owners (default true): the chain hands the kernel owners to the frameworks (ursatz_chain/kernel_owners.h): motive_tone_roles
 * labels the leftover tones of the weighted chord identifier and the notes of the melody stage, and motive_variant_recognize decides the
 * transposed, inverted and retrograde occurrences of the motive stage. Off: the frameworks' own fallback rules, as before R-243. A
 * classifier or matcher the caller already set in weighting or form is kept.
 *
 * home_key_index: the home key as a key index 0..23 (key_regions.h), or -1 for the key of the first key region. It is the key of the
 * boundary cadences, the motive degrees, the melody degrees and the schema key degrees.
 *
 * harmonic_spans (default true): the chords are read on harmonic spans, not on every segment (R-369 to R-375). Before the key regions
 * exist, the segments are grouped into spans (harmonic_spans_group, read in no key) and replaced by one segment per span, each carrying
 * the bass its span was read over; the chords then require that bass as a chord tone and the Roman numerals take it as the inversion's
 * bass. Off: one chord per segment as before. It is the switch for measuring the change.
 *
 * applied_chords (default true): the chain reads applied chords (R-380). The chord identification also ranks the key's applied chords
 * (V/x, V7/x, vii°/x, vii°7/x) and the Roman numerals name an identified applied chord directly, with the diatonic re-check as an applied
 * chord off. The grouping, the key segments and the key regions do not see it. Off: diatonic chords only and the re-check, as before. It
 * is the switch for measuring the change; it applies with harmonic_spans on or off.
 */
typedef struct {
    unsigned stages;
    AdaptiveSegmentationConfig segmentation;
    CommonPracticeWeightedConfig weighting;
    CommonPracticeKeyRegionsConfig key_regions;
    OuterVoiceConfig outer_voices;
    HarmonyCadenceConfig cadences;
    StandardFormConfig form;
    StandardMeterConfig meter;
    StandardTextureConfig texture;
    HarmonyResolutionConfig resolutions;
    StandardCounterpointMelodyConfig melody;
    GalantMatchConfig schemas;
    ScaleDetectConfig sequences;
    bool refine_keys;
    bool refine_with_given_boundaries;
    bool phrase_grid;
    bool keys_from_phrases;
    int first_measure;
    int last_measure;
    int home_key_index;
    bool inject_kernel_owners;
    bool harmonic_spans;
    bool applied_chords;
} AnalysisChainConfig;

AnalysisChainConfig analysis_chain_config_default(void);

typedef enum {
    ANALYSIS_CHAIN_OK = 0,
    ANALYSIS_CHAIN_ERR_NULL_ARGUMENT,
    ANALYSIS_CHAIN_ERR_BAD_CONFIG,
    ANALYSIS_CHAIN_ERR_NOTES,          /* an invalid note set, a note without its event or with an unknown pitch, or a scope with no segment */
    ANALYSIS_CHAIN_ERR_SEGMENTS,
    ANALYSIS_CHAIN_ERR_KEYS,
    ANALYSIS_CHAIN_ERR_CHORDS,
    ANALYSIS_CHAIN_ERR_NUMERALS,
    ANALYSIS_CHAIN_ERR_OUTER_VOICES,
    ANALYSIS_CHAIN_ERR_CADENCES,
    ANALYSIS_CHAIN_ERR_FORM,
    ANALYSIS_CHAIN_ERR_METER,
    ANALYSIS_CHAIN_ERR_TEXTURE,
    ANALYSIS_CHAIN_ERR_VOICE_LEADING,
    ANALYSIS_CHAIN_ERR_MELODY,
    ANALYSIS_CHAIN_ERR_SCHEMAS,
    ANALYSIS_CHAIN_ERR_ALLOC
} AnalysisChainStatus;

/* The name of a status, e.g. "keys"; "ok" for ANALYSIS_CHAIN_OK. */
const char *analysis_chain_status_name(AnalysisChainStatus status);

typedef struct AnalysisChain AnalysisChain;

/*
 * Runs the chain. `notes` is sorted (analysis_note_set_valid) and every note has its NoteEvent and a known pitch. key_events may be NULL
 * with key_event_count 0. config may be NULL for the defaults. On OK, *out is owned by the caller (analysis_chain_destroy); on an error
 * *out is untouched and the status names the failing stage.
 */
AnalysisChainStatus analysis_chain_run(const AnalysisNoteSet *notes, const AnalysisKeyEvent *key_events, size_t key_event_count,
                                       const AnalysisChainConfig *config, AnalysisChain **out);

/* NULL is a no-op. */
void analysis_chain_destroy(AnalysisChain *chain);

/* --- the results --- */

/* How many segments the stages ran over (those starting inside the scope). */
size_t analysis_chain_segment_count(const AnalysisChain *chain);
/* The segments of the chords (sonorities, ids, windows in whole notes, members). */
const ChainSegments *analysis_chain_segments(const AnalysisChain *chain);
/* The home key as a key index, 0..23; -1 when no key stage ran. */
int analysis_chain_home_key(const AnalysisChain *chain);

const CommonPracticeKeyRegions *analysis_chain_key_regions(const AnalysisChain *chain);
/* One result per segment (analysis_chain_segment_count). */
const CommonPracticeWeightedResult *analysis_chain_chords(const AnalysisChain *chain);
/* The chord input and the Roman numeral of each segment (analysis_chain_segment_count of each). */
const StandardTonalityChordInput *analysis_chain_chord_inputs(const AnalysisChain *chain);
const StandardTonalityRomanResult *analysis_chain_numerals(const AnalysisChain *chain);
/* The segmentation the outer lines are read on (one segment per melody note), and the lines (one entry per such segment): the melody and meter stages read them (R-377). */
const ChainSegments *analysis_chain_voice_segments(const AnalysisChain *chain);
const OuterVoiceLines *analysis_chain_outer_voices(const AnalysisChain *chain);
/* The same lines on the chord segments: one entry per segment of analysis_chain_segments, as the harmony, galant and form builders read them (the form input moves to the voice lines in P41, R-377). */
const OuterVoiceLines *analysis_chain_segment_outer_voices(const AnalysisChain *chain);
const HarmonyCadences *analysis_chain_cadences(const AnalysisChain *chain);
/* The stage inputs the chain builds and reads, kept until analysis_chain_destroy. NULL when the chain is NULL or the stage did not run. */
/* The pitches of every note (outer voices, texture and voice leading). */
const ChainPitches *analysis_chain_pitches(const AnalysisChain *chain);
/* The harmony inputs of the final pass, one per chord segment. */
const ChainHarmonyInputs *analysis_chain_harmony_inputs(const AnalysisChain *chain);
/* The prepared harmony segments of the final pass: analysis_chain_segment_count entries. */
const HarmonySegment *analysis_chain_prepared_harmony(const AnalysisChain *chain);
/* The form input of the final pass: the phrases, ideas and themes were read from it. */
const ChainFormInput *analysis_chain_form_input(const AnalysisChain *chain);
/* The note-level soprano input the motive stage read, over the home key. */
const ChainFormInput *analysis_chain_motive_input(const AnalysisChain *chain);
/* The voice grid the voice-leading stage read. */
const ChainVoiceGrid *analysis_chain_voice_grid(const AnalysisChain *chain);
/* The segments of the schema and sequence adapters. */
const ChainGalantSegments *analysis_chain_galant_segments(const AnalysisChain *chain);

const StandardFormPhrases *analysis_chain_phrases(const AnalysisChain *chain);
/* The phrase grid the final pass found (R-389): NULL without one (phrase_grid off, no form stage, or a pass that ran without it). */
const StandardFormPhraseGrid *analysis_chain_phrase_grid(const AnalysisChain *chain);
/* The written-out repeats and returns of the notes in scope (R-400), found once in prepare; the phrase grid follows them. An empty list
   (no items) when there are none or no segment stage ran; never NULL. */
const StandardFormRepeats *analysis_chain_repeats(const AnalysisChain *chain);
const StandardFormSegmentation *analysis_chain_ideas(const AnalysisChain *chain);
const StandardFormThemes *analysis_chain_themes(const AnalysisChain *chain);
const StandardFormExpansions *analysis_chain_expansions(const AnalysisChain *chain);
const StandardFormMotives *analysis_chain_motives(const AnalysisChain *chain);

typedef struct {
    StandardMeterSyncopations syncopations;
    StandardMeterHemiolas hemiolas;
    StandardMeterHypermeter hypermeter;
    StandardMeterDensities densities;
} AnalysisChainMeter;
const AnalysisChainMeter *analysis_chain_meter(const AnalysisChain *chain);

const StandardTextureAnalysis *analysis_chain_texture(const AnalysisChain *chain);

typedef struct {
    StandardCounterpointMotions motions;
    StandardCounterpointViolations perfects;
    HarmonyResolutions resolutions;
    size_t voice_count;
} AnalysisChainVoiceLeading;
const AnalysisChainVoiceLeading *analysis_chain_voice_leading(const AnalysisChain *chain);

/* The soprano as a melody. The one span covering the whole line serves the span-based results. */
typedef struct {
    ChainMelodyLine line;
    StandardCounterpointMelodicIntervals intervals;
    StandardCounterpointContourSummaries summaries;
    StandardCounterpointClimaxes climaxes;
    StandardCounterpointLeapRecoveries recoveries;
    StandardCounterpointNctCountList counts;
} AnalysisChainMelody;
const AnalysisChainMelody *analysis_chain_melody(const AnalysisChain *chain);

const GalantSchemaMatches *analysis_chain_schemas(const AnalysisChain *chain);
const ScaleSequenceMatches *analysis_chain_sequences(const AnalysisChain *chain);

#endif
