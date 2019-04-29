#ifndef MUSIC_ANALYSIS_ANALYZER_H
#define MUSIC_ANALYSIS_ANALYZER_H

#include <stddef.h>

#include "context/context.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * AnalysisResult (Category / base type): the output of an Analyzer
 * (music-analysis Registry #12).
 *
 * An Analyzer never writes a bare conclusion -- it produces
 * Interpretation objects (interpretation/interpretation.h, L6), each
 * already carrying its own Evidence and Uncertainty (TDD Sec242).
 * AnalysisResult therefore holds only opaque EntityID references to the
 * Interpretations a single analyze() call produced -- referenced, never
 * embedded, the same reference pattern Interpretation itself uses for
 * target/context/evidence.
 *
 * interpretation_ids may be empty (interpretation_id_count == 0, pass
 * NULL for interpretation_ids in that case): an analyzer that finds
 * nothing still returns a valid, empty AnalysisResult, not an error.
 *
 * provenance is a required ProvenanceReference: every AnalysisResult
 * records what produced it.
 */

typedef struct AnalysisResult AnalysisResult;

typedef enum {
    ANALYSIS_RESULT_OK = 0,
    ANALYSIS_RESULT_ERR_NULL_ID,
    ANALYSIS_RESULT_ERR_NULL_INTERPRETATION_ID,
    ANALYSIS_RESULT_ERR_NULL_PROVENANCE,
    ANALYSIS_RESULT_ERR_ALLOC
} AnalysisResultResult;

/*
 * Takes ownership of id, each EntityID pointed to by interpretation_ids,
 * and provenance on success (all destroyed by analysis_result_destroy).
 * interpretation_ids is copied into an internally-owned array; pass
 * NULL with interpretation_id_count == 0 for an empty result. No entry
 * of a non-empty interpretation_ids may be NULL.
 */
AnalysisResultResult analysis_result_create(EntityID *id, EntityID **interpretation_ids,
                                             size_t interpretation_id_count, ProvenanceReference *provenance,
                                             AnalysisResult **out);
void analysis_result_destroy(AnalysisResult *result);

const EntityID *analysis_result_id(const AnalysisResult *result);
size_t analysis_result_interpretation_id_count(const AnalysisResult *result);
const EntityID *analysis_result_interpretation_id_at(const AnalysisResult *result, size_t index);
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
 * holding Interpretation references, never a bare conclusion field.
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