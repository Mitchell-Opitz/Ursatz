#ifndef URSATZ_CHAIN_ANALYSIS_CHAIN_INTERNAL_H
#define URSATZ_CHAIN_ANALYSIS_CHAIN_INTERNAL_H

#include "chain/analysis_chain.h"
#include "chain/chain_result.h"
#include "chain/kernel_owners.h"
#include <io/imported_score.h>
#include "theory/tonality/common_practice_materials.h"

/*
 * The fields of an AnalysisChain: analysis_chain.c writes them. Not staged: no application includes this header.
 */

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

/* What a pass of the stages from the keys to the form is given: the phrase ends of the pass before as the key regions' boundaries
   (NULL for none) and whether it is the final pass (the form stage then finds the phrases first, on the grid). */
typedef struct {
    const MusicalTime *boundaries;
    size_t boundary_count;
    bool final;
} AnalysisChainPass;

struct AnalysisChain;

/*
 * One stage of the chain. The table below lists them in dependency order, which is the order they run: every row needs only rows
 * before it. `needs` are the stages a wanted stage pulls in (analysis_chain_run closes the wanted set over them in one backward pass);
 * `reads_pitches` marks a row that reads the notes' pitches, so they are built when one is wanted; `error` is the status a failure of
 * the row reports (ANALYSIS_CHAIN_ERR_SEGMENTS plus the bit's index). A row has any of the three steps, each run over the wanted rows
 * in table order: `prepare` once before the passes, `run_pass` in every pass of the stages from the keys to the form, `finish` once after
 * them.
 */
typedef struct {
    unsigned bit;
    unsigned needs;
    bool reads_pitches;
    AnalysisChainStatus error;
    const char *name;
    AnalysisChainStatus (*prepare)(struct AnalysisChain *chain);
    AnalysisChainStatus (*run_pass)(struct AnalysisChain *chain, const AnalysisChainPass *pass);
    AnalysisChainStatus (*finish)(struct AnalysisChain *chain);
} AnalysisChainStage;

extern const AnalysisChainStage analysis_chain_stages[ANALYSIS_CHAIN_STAGE_COUNT];

struct ChainCollect;

struct AnalysisChain {
    AnalysisChainConfig config;
    CommonPracticeMaterials materials; /* resolved once from config.knowledge; the stage configs point here */
    ChainRelationContext relation_context; /* the key the motive stage reads generic shapes in */
    const ImportedScore *score;     /* analysis_chain_run_score: the score the notes were built from */
    ChainResult result;             /* the collected result; empty unless collected */
    struct ChainCollect *collect;   /* what the collection keeps beside the chain (chain_collect.h); owned */
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
    MusicalTime *boundaries; /* the second pass: the phrase ends of the first, owned */
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
