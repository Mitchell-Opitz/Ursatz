#ifndef URSATZ_READING_READING_BUILD_H
#define URSATZ_READING_READING_BUILD_H

#include "chain/analysis_chain.h"
#include "reading/reading.h"

/*
 * Builds the reading of a chain run (R-629): the claims of its collected result (analysis_chain_result), in the order of the result's
 * arrays, each keeping the index of the element it came from as its `detail`. `note_ids[i]` is the performer note id of the chain's
 * note index i (the ImportedScore index the result's note_ids hold; imported_score_realization gives them); an index at or beyond
 * `note_count` is REALIZATION_ERR_BAD_VALUE. The reading's detail is the chain, which has to outlive it.
 *
 *   regions   from key_regions: the tonic, the material, strength ESTABLISHED (the chain grades none), the label, no confidence, the
 *             notes of the chord segments inside the region as evidence, each once;
 *   chords    from the entries that have a chord (a label other than "no match"): the span, the chord reading of the sonority when the
 *             chain gives one, the Roman numeral in its key region, the function, the label, the chord's confidence, the notes sounding;
 *   cadences  from cadences: the type, the arrival at its first chord's time and the span to the next chord (or the end of the score),
 *             the alternatives that name a cadence type, the arrival chords' notes;
 *   groups    the sections (top level), then the themes (under the section that holds their start), then the phrases, subphrases and
 *             ideas (under the theme, else the section, that holds them);
 *   motive occurrences, meters (one per region of the score's meter map), lines (the soprano and the bass of outer_lines: roles MELODY
 *             and BASS) and voice faults (chain_result_voice_faults).
 *
 * Claim ids are region-<n>, chord-<n>, cadence-<n>, group-<n>, motive-<n>, meter-<n>, line-<n> and fault-<n>, n from 1. A chain whose
 * result is empty (not every perception stage ran) gives a reading with only the meters and the voice faults the chain can still give.
 */
RealizationResult reading_build(const AnalysisChain *chain, const ReadingSource *source, const ReadingTarget *target,
                                const char (*note_ids)[REALIZATION_ID_MAX], size_t note_count, Reading **out);

#endif
