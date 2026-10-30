#ifndef URSATZ_ANALYZER_TEXTURE_PIPELINE_H
#define URSATZ_ANALYZER_TEXTURE_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_chain/analysis_chain.h>

/*
 * The texture of the piece, from the library chain (analysis_chain_texture: Ursatz's standard-texture analysis over every
 * note with the voice the kernel's rule-based voice separation gives it, run per stream of the import, a MIDI track or a
 * MusicXML part). The analyzer adds no music theory (R-018): it copies the library's results; every texture, category,
 * feature and confidence is the library's. The library measures every measure, classifies it with the default
 * configuration and merges equal measures into passages.
 *
 * A piece the chain has no texture for has no texture section, which is not an error.
 */

/* Fills the report's texture passages and changes. Returns false only on allocation failure. */
bool texture_stage_collect(const ImportedNotes *notes, const AnalysisChain *library, AnalysisReport *report);

#endif
