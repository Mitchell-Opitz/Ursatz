#ifndef MUSIC_ANALYSIS_ANALYZER_H
#define MUSIC_ANALYSIS_ANALYZER_H

#include <stddef.h>

#include "context/context.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"

/*
 * AnalysisResult (Category / base type): the output of an Analyzer
 * (music-analysis Registry #12).
 *
 * An Analyzer never writes a bare conclusion -- it derives new
 * Interpretation objects (interpretation/interpretation.h, L6), each
 * backed by its own Evidence (interpretation/evidence.h) and carrying
 * its own Uncertainty (TDD Sec242). These are DERIVED objects, the same
 * relationship Transformation has to what it produces (TDD Sec140) --
 * not references into some pre-existing store (no Interpretation/
 * Evidence store or registry exists in this codebase). AnalysisResult
 * therefore OWNS the Interpretations and Evidences a single analyze()
 * call created, not just their ids: there is nowhere else for those
 * objects to live once analyze() returns.
 *
 * interpretations and evidences are independent arrays (an
 * Interpretation may cite an Evidence by id, interpretation_evidence_id,
 * but nothing here forces a 1:1 count between the two -- an
 * Interpretation may cite no Evidence, or multiple Interpretations may
 * cite the same Evidence). Both may be empty (count == 0, pass NULL for
 * the corresponding array): an analyzer that found nothing still
 * returns a valid, empty AnalysisResult, not an error.
 *
 * provenance is a required ProvenanceReference: every AnalysisResult
 * records what produced it.
 */

typedef struct AnalysisResult AnalysisResult;

typedef enum {
    ANALYSIS_RESULT_OK = 0,
    ANALYSIS_RESULT_ERR_NULL_ID,
    ANALYSIS_RESULT_ERR_NULL_INTERPRETATION,
    ANALYSIS_RESULT_ERR_NULL_EVIDENCE,
    ANALYSIS_RESULT_ERR_NULL_PROVENANCE,
    ANALYSIS_RESULT_ERR_ALLOC
} AnalysisResultResult;

/*
 * Takes ownership of id, each Interpretation in interpretations, each
 * Evidence in evidences, and provenance on success (all destroyed by
 * analysis_result_destroy). interpretations/evidences are each copied
 * into an internally-owned array; pass NULL with the matching count ==
 * 0 for an empty array. No entry of a non-empty array may be NULL.
 */
AnalysisResultResult analysis_result_create(EntityID *id, Interpretation **interpretations,
                                             size_t interpretation_count, Evidence **evidences, size_t evidence_count,
                                             ProvenanceReference *provenance, AnalysisResult **out);
void analysis_result_destroy(AnalysisResult *result);

const EntityID *analysis_result_id(const AnalysisResult *result);
size_t analysis_result_interpretation_count(const AnalysisResult *result);
const Interpretation *analysis_result_interpretation_at(const AnalysisResult *result, size_t index);
size_t analysis_result_evidence_count(const AnalysisResult *result);
const Evidence *analysis_result_evidence_at(const AnalysisResult *result, size_t index);
const ProvenanceReference *analysis_result_provenance(const AnalysisResult *result);

/*
 * Analyzer (Interface, Registry #12): analyze(input, context, config)
 * -> AnalysisResult. Depends on the Canonical IR (opaque input),
 * Context (L6), and Framework (L7, now a legal L8 -> L7 dependency).
 *
 * This is a function-pointer interface, not a data type with its own
 * create/destroy: a concrete analyzer (IntervalAnalysis, ScaleAnalysis,
 * ... later commits in this pass) is a module that implements
 * AnalyzerFn and exposes it, alongside a name, as an Analyzer value --
 * it does not subclass or extend this struct.
 *
 * input is an opaque, borrowed (non-owned) pointer: its concrete type
 * is whatever Canonical IR object or collection the specific analyzer
 * expects. No generic type-registry concept exists yet in the canonical
 * IR (the same gap Transformation's opaque input_type/output_type
 * strings work around, transform/transformation.h); here there is not
 * even a string tag, since a caller invoking a specific concrete
 * Analyzer already knows what input it requires.
 *
 * context is a borrowed, optional reference to a Context
 * (context/context.h) -- not owned, may be NULL when no context narrows
 * the analysis. config is a borrowed, optional Value (core/value.h)
 * carrying analyzer-specific configuration -- not owned, may be NULL
 * when an analyzer takes no configuration.
 *
 * On ANALYZER_OK, *out is a newly-created AnalysisResult the caller
 * owns (destroy with analysis_result_destroy); on any other result,
 * *out is left untouched. A concrete analyzer MUST NOT mutate input
 * (TDD Sec140 referential transparency, the same rule Transformation
 * follows) and MUST report its findings only as Interpretations (TDD
 * Sec242) -- enforced structurally here by AnalysisResult only ever
 * holding Interpretation/Evidence objects, never a bare conclusion
 * field.
 */

typedef enum {
    ANALYZER_OK = 0,
    ANALYZER_ERR_NULL_INPUT,
    ANALYZER_ERR_NULL_OUT,
    ANALYZER_ERR_ALLOC,
    ANALYZER_ERR_ANALYSIS_FAILED
} AnalyzerResult;

typedef AnalyzerResult (*AnalyzerFn)(const void *input, const Context *context, const Value *config,
                                     AnalysisResult **out);

typedef struct {
    const char *name;
    AnalyzerFn analyze;
} Analyzer;

#endif