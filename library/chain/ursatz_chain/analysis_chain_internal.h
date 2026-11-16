#ifndef URSATZ_CHAIN_ANALYSIS_CHAIN_INTERNAL_H
#define URSATZ_CHAIN_ANALYSIS_CHAIN_INTERNAL_H

#include "ursatz_chain/analysis_chain.h"
#include "ursatz_chain/kernel_owners.h"

/*
 * The fields of an AnalysisChain: analysis_chain.c writes them; tests read the meter results through
 * tests/support/chain_test_access.h. Not staged: no application includes this header.
 */

typedef struct {
    StandardMeterSyncopations syncopations;
    StandardMeterHemiolas hemiolas;
    StandardMeterHypermeter hypermeter;
    StandardMeterDensities densities;
} AnalysisChainMeter;

/* The results of the stages from the key regions to the form, which run twice when the keys are refined. */
typedef struct {
    CommonPracticeKeyRegions regions;
    bool have_regions;
    CommonPracticeWeightedResult *chords; /* one per segment */
    StandardTonalityKeyRegionInput *region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    int *segment_pitches; /* the members' pitches, segment after segment, for the chord bass */
    StandardTonalityChordInput *tonal_inputs;
    size_t tonal_count;
    StandardTonalityRomanResult *romans;
    ChainHarmonyInputs harmony;
    bool have_harmony;
    HarmonySegment *prepared;
    HarmonyCadences cadences;
    bool have_cadences;
    StandardFormPhraseGrid grid;
    bool have_grid;
    ChainFormInput form_input;
    bool have_form_input;
    StandardFormPhrases phrases;
    bool have_phrases;
    StandardFormSegmentation ideas;
    bool have_ideas;
    StandardFormThemes themes;
    bool have_themes;
    StandardFormExpansions expansions;
    bool have_expansions;
} Pass;

struct AnalysisChain {
    AnalysisChainConfig config;
    ChainRelationContext relation_context; /* the key the motive stage reads generic shapes in */
    unsigned stages;
    const AnalysisNoteSet *notes;
    const AnalysisKeyEvent *key_events;
    size_t key_event_count;

    ChainPitches pitches;
    bool have_pitches;
    ChainSegments segs;
    bool have_segs;
    bool spans_merged; /* the segments are harmonic spans, each with its bass */
    size_t count; /* the segments inside the scope */
    ChainSegments voice_segs;
    bool have_voice_segs;
    OuterVoiceLines lines;        /* on the voice segments */
    bool have_lines;
    OuterVoiceLines segment_lines; /* the same lines on the chord segments */
    bool have_segment_lines;
    CommonPracticeKeySegment *key_segments;
    MusicalTime **boundaries; /* the second pass: the phrase ends of the first, owned copies */
    size_t boundary_count;
    int home;
    StandardFormRepeats repeats; /* the written-out repeats and returns, found once in prepare (R-400) */
    CommonPracticeKeyPhrase *key_phrases; /* the third pass: the phrases of the second, on the grid (R-397); owned */
    size_t key_phrase_count;
    CommonPracticeKeyRepeat *key_repeats; /* the exact repeats, as the key regions read them (R-400); owned */
    size_t key_repeat_count;

    Pass pass;

    ChainFormInput motive_input;
    bool have_motive_input;
    StandardFormPhrases motive_phrases;
    bool have_motive_phrases;
    StandardFormSegmentation motive_ideas;
    bool have_motive_ideas;
    StandardFormThemes motive_themes;
    bool have_motive_themes;
    StandardFormMotives motives;
    bool have_motives;

    ChainMeterInput meter_input;
    bool have_meter_input;
    AnalysisChainMeter meter;
    bool have_syncopations;
    bool have_hemiolas;
    bool have_hypermeter;
    bool have_densities;

    ChainTextureInput texture_input;
    bool have_texture_input;
    StandardTextureAnalysis texture;
    bool have_texture;

    ChainVoiceGrid grid;
    bool have_grid;
    StandardCounterpointPreparedGrid prepared_grid;
    bool have_prepared_grid;
    AnalysisChainVoiceLeading voice_leading;
    bool have_motions;
    bool have_perfects;
    bool have_resolutions;

    AnalysisChainMelody melody;
    bool have_melody_line;
    bool have_intervals;
    bool have_summaries;
    bool have_climaxes;
    bool have_recoveries;
    bool have_counts;

    ChainGalantSegments galant;
    bool have_galant;
    GalantSchemaMatches schemas;
    bool have_schemas;
    ScaleSequenceMatches sequences;
    bool have_sequences;
};

#endif
