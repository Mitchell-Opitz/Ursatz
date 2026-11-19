#ifndef URSATZ_CHAIN_ANALYSIS_CHAIN_H
#define URSATZ_CHAIN_ANALYSIS_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/voices/outer_voice.h"
#include "analysis/voices/voice_separation.h"
#include "analysis/harmony/analysis_cadence.h"
#include "analysis/form/analysis_expansion.h"
#include "analysis/counterpoint/analysis_grid.h"
#include "analysis/form/analysis_ideas.h"
#include "analysis/counterpoint/analysis_melody.h"
#include "analysis/counterpoint/analysis_melody_climax.h"
#include "analysis/counterpoint/analysis_melody_leaps.h"
#include "analysis/counterpoint/analysis_melody_nct.h"
#include "analysis/counterpoint/analysis_motion.h"
#include "analysis/form/analysis_motive.h"
#include "analysis/counterpoint/analysis_perfects.h"
#include "analysis/form/analysis_phrase.h"
#include "analysis/form/analysis_phrase_grid.h"
#include "analysis/form/analysis_repeats.h"
#include "analysis/harmony/analysis_resolution.h"
#include "analysis/form/analysis_theme.h"
#include "analysis/harmony/chord_input.h"
#include "analysis/harmony/chord_reading.h"
#include "analysis/galant/galant_analysis_match.h"
#include "analysis/harmony/key_regions.h"
#include "analysis/meter/meter_analysis_density.h"
#include "analysis/meter/meter_analysis_hemiola.h"
#include "analysis/meter/meter_analysis_hypermeter.h"
#include "analysis/meter/meter_analysis_syncopation.h"
#include "analysis/harmony/roman_numeral_adapter.h"
#include "analysis/galant/scale_analysis_detect.h"
#include "analysis/notes/analysis_key.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/notes/sonority_segmentation.h"
#include "analysis/texture/texture_analysis_classify.h"
#include "chain/chain_inputs.h"
#include "chain/chain_result.h"
#include <io/imported_score.h>
#include "analysis/harmony/weighted_chord_identification.h"
#include "theory/knowledge/knowledge_snapshot.h"

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
#define ANALYSIS_CHAIN_STAGE_COUNT         12u
/*
 * The stages of the result only (R-615): the collected result of analysis_chain_run_score has six stages that no chain stage
 * computes, switched by these bits (the values the Analyzer's ANALYZER_STAGE_* had). analysis_chain_run ignores them.
 */
#define ANALYSIS_CHAIN_STAGE_PROGRESSIONS    (1u << 12)
#define ANALYSIS_CHAIN_STAGE_HARMONIC_RHYTHM (1u << 13)
#define ANALYSIS_CHAIN_STAGE_METER_CHECK     (1u << 14)
/* Bit 15 is unused: it was ANALYSIS_CHAIN_STAGE_RHYTHM, the collected rhythm stage, which is now the chain's one meter analysis
   (ANALYSIS_CHAIN_STAGE_METER, R-616). */
#define ANALYSIS_CHAIN_STAGE_MARKINGS        (1u << 16)
#define ANALYSIS_CHAIN_STAGE_FORM_PLAN       (1u << 17)
/* The twelve chain stages: the result is collected when a run has all of them. */
#define ANALYSIS_CHAIN_STAGE_PERCEPTION      ((1u << ANALYSIS_CHAIN_STAGE_COUNT) - 1u)
/* Every stage, the result-only ones included. */
#define ANALYSIS_CHAIN_STAGE_ALL             ((1u << 18) - 1u)

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
 * PHRASES FIRST (R-389 to R-391). The final pass of the form stage, the second pass of the key refinement (the only pass when there is
 * none), finds the phrases first: the grid of units of 4, 8 or 16 measures is read from the soprano and the cadences found, the cadences
 * are claimed at the grid's points (R-390), and phrases and themes are segmented on the grid (R-391). The first pass of the refinement
 * keeps the cadence-ended phrases, so the key regions do not move.
 *
 * KEYS FROM PHRASES (R-397, R-398). After the second pass a third reads the keys again, now placed on the phrases of the grid the second
 * pass found: a key changes only when a phrase establishes the new key, the cadences the second pass claimed are the evidence, and exact
 * repeats read the same. Everything from the keys on runs once more on that answer. It runs only with the form stage and refine_keys, and
 * only when the second pass found a grid.
 *
 * refine_with_given_boundaries: with no form stage there are no phrases to bound the second key pass; when set (and refine_keys is on) the
 * second pass runs anyway, on the first pass's chords as evidence and key_regions.phrase_boundaries as the boundaries (possibly none), as a
 * caller that finds its own phrase ends does (the accuracy harness). The boundaries are borrowed and must outlive the run. Default off.
 *
 * KERNEL OWNERS. The chain hands the kernel owners to the frameworks (ursatz_chain/kernel_owners.h): motive_tone_roles labels the
 * leftover tones of the weighted chord identifier and the notes of the melody stage. A classifier the caller already set in weighting is
 * kept. The motive stage always gets motive_variant_recognize as its relation matcher (it decides the transposed, inverted and retrograde
 * occurrences), unless the caller already set form.relation_match.
 *
 * home_key_index: the home key as a key index 0..23 (key_regions.h), or -1 for the key of the first key region. It is the key of the
 * boundary cadences, the motive degrees, the melody degrees and the schema key degrees.
 *
 * harmonic_spans (default true): the chords are read on harmonic spans, not on every segment (R-369 to R-375). Before the key regions
 * exist, the segments are grouped into spans (harmonic_spans_group, read in no key) and replaced by one segment per span, each carrying
 * the bass its span was read over; the chords then require that bass as a chord tone and the Roman numerals take it as the inversion's
 * bass. Off: one chord per segment as before. It is the switch for measuring the change.
 *
 * APPLIED CHORDS (R-380). The chord identification also ranks the key's applied chords (V/x, V7/x, vii°/x, vii°7/x) and the Roman
 * numerals name an identified applied chord directly, with no diatonic re-check. The grouping, the key segments and the key regions do not
 * see it. It applies with harmonic_spans on or off.
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
    bool refine_keys;                  /* test and harness hook */
    bool refine_with_given_boundaries; /* test and harness hook */
    int first_measure;                 /* test and harness hook */
    int last_measure;                  /* test and harness hook */
    int home_key_index;                /* test and harness hook */
    bool harmonic_spans;
    /* The knowledge snapshot the run reads its pitch materials from (borrowed; it outlives the run and every key made from it).
       Required: the default config has none, so a run without it is ANALYSIS_CHAIN_ERR_KNOWLEDGE. */
    const KnowledgeSnapshot *knowledge;
} AnalysisChainConfig;

AnalysisChainConfig analysis_chain_config_default(void);

/* The library's producer key (R-620): 16 hex digits, a hash of the library sources that produce an analysis (model, theory, analysis, io,
   chain and the vendored cJSON and ezxml; no test, doc or app). It changes when any of them changes and not otherwise. A plan and the
   Composer's versions record it as `ursatz_commit`. */
const char *analysis_chain_producer_key(void);

typedef enum {
    ANALYSIS_CHAIN_OK = 0,
    ANALYSIS_CHAIN_ERR_NULL_ARGUMENT,
    ANALYSIS_CHAIN_ERR_BAD_CONFIG,
    ANALYSIS_CHAIN_ERR_NOTES,          /* an invalid note set, a note without its event or with an unknown pitch, or a scope with no segment */
    ANALYSIS_CHAIN_ERR_KNOWLEDGE,      /* a config without a knowledge snapshot, or a snapshot without the four common-practice materials */
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

typedef struct AnalysisChain AnalysisChain;

/*
 * Runs the chain. `notes` is sorted (analysis_note_set_valid) and every note has its NoteEvent and a known pitch. key_events may be NULL
 * with key_event_count 0. config may be NULL for the defaults, which carry no knowledge snapshot (ANALYSIS_CHAIN_ERR_KNOWLEDGE). On OK, *out is owned by the caller (analysis_chain_destroy); on an error
 * *out is untouched and the status names the failing stage.
 */
AnalysisChainStatus analysis_chain_run(const AnalysisNoteSet *notes, const AnalysisKeyEvent *key_events, size_t key_event_count,
                                       const AnalysisChainConfig *config, AnalysisChain **out);

/*
 * Runs the chain on an imported score and collects its result (R-615). The note set and key events are built from the score
 * (the score's notes, their spelling when it is notated, its streams as tracks, its meter map and key signature events), the
 * stages run as analysis_chain_run runs them (config->stages & ANALYSIS_CHAIN_STAGE_PERCEPTION), and when the config has every
 * perception stage the ChainResult is collected (analysis_chain_result), with the result-only stages whose bits the config has.
 * config may be NULL for the defaults, which carry no knowledge snapshot (ANALYSIS_CHAIN_ERR_KNOWLEDGE). A score with no notes
 * gives a chain with nothing run and an empty result. The score is borrowed and must outlive the chain. On OK, *out is owned by
 * the caller (analysis_chain_destroy); on an error *out is untouched.
 */
AnalysisChainStatus analysis_chain_run_score(const ImportedScore *score, const AnalysisChainConfig *config,
                                             AnalysisChain **out);

/* NULL is a no-op. */
void analysis_chain_destroy(AnalysisChain *chain);

/* The collected result of analysis_chain_run_score. Never NULL: empty when it was not collected (a run of fewer stages, no score
   notes, analysis_chain_run). */
const ChainResult *analysis_chain_result(const AnalysisChain *chain);

/* The score analysis_chain_run_score ran on; NULL after analysis_chain_run. */
const ImportedScore *analysis_chain_score(const AnalysisChain *chain);

/* Moves the collected result out into *out (the caller releases it with chain_result_destroy); the chain keeps an empty one. */
void chain_result_take(AnalysisChain *chain, ChainResult *out);

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
/*
 * The chord of segment `index` as a reading of its sonority (chord_reading.h, R-592). A segment the chain gave a chord reads as that
 * chord: the sonority holds the chord's tones (its root, third, fifth and seventh when it has one; the leftover tones are not in it), the
 * root and the chord type are the chain's chord's (the snapshot's type for those tones), the bass is the chord input's when the
 * numerals stage ran (the lowest sounding chord tone), else the bass the span was read over, else -1, and the role is EMBELLISHING for a
 * cadential six-four, FUNCTIONAL otherwise. A segment with no chord reads as the aggregate of the pitch classes sounding in it: no root, no type, FUNCTIONAL. The span is
 * the segment's window. `borrowed_from` is NULL and there is no expected chord (no mixture or elision reading exists). Without the
 * cadences stage no segment is known as a cadential six-four, so each reads FUNCTIONAL. ANALYSIS_CHAIN_ERR_CHORDS when the chords stage
 * did not run or `index` is not below analysis_chain_segment_count; ANALYSIS_CHAIN_ERR_NULL_ARGUMENT for a NULL chain or out. `*out` is
 * untouched on an error. The chord type is borrowed from the config's snapshot. Nothing writes the readings until P62's reading layer.
 */
AnalysisChainStatus analysis_chain_chord_reading(const AnalysisChain *chain, size_t index, ChordReading *out);
/* The segmentation the outer lines are read on (one segment per melody note), and the lines (one entry per such segment): the melody and meter stages read them (R-377). */
const ChainSegments *analysis_chain_voice_segments(const AnalysisChain *chain);
const OuterVoiceLines *analysis_chain_outer_voices(const AnalysisChain *chain);
const HarmonyCadences *analysis_chain_cadences(const AnalysisChain *chain);
/* The stage inputs the chain builds and reads, kept until analysis_chain_destroy. NULL when the chain is NULL or the stage did not run. */
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
/* The written-out repeats and returns of the notes in scope (R-400), found once in prepare; the phrase grid follows them. An empty list
   (no items) when there are none or no segment stage ran; never NULL. */
const StandardFormRepeats *analysis_chain_repeats(const AnalysisChain *chain);
const StandardFormSegmentation *analysis_chain_ideas(const AnalysisChain *chain);
const StandardFormThemes *analysis_chain_themes(const AnalysisChain *chain);
const StandardFormExpansions *analysis_chain_expansions(const AnalysisChain *chain);
const StandardFormMotives *analysis_chain_motives(const AnalysisChain *chain);

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
