#ifndef MUSIC_GENERATION_INTENT_COMPILER_H
#define MUSIC_GENERATION_INTENT_COMPILER_H

#include "context/context.h"
#include "core/value.h"
#include "generation/intent.h"

/*
 * IntentCompiler (Component / Interface, Registry #22): compile(
 * request_text, context, config) -> Intent (TDD Sec154).
 *
 * The natural-language-processing layer itself is OUT OF SCOPE for this
 * module -- only the CONTRACT (input/output shape) is required here, the
 * same "declares the contract, doesn't execute" treatment Transformation
 * gives its deterministic flag (transform/transformation.h) and Analyzer
 * gives analyze() (analysis/analyzer.h, Registry #12). IntentCompiler is
 * a function-pointer interface, not a data type with its own create/
 * destroy: a concrete compiler (an actual NLP-backed implementation, out
 * of scope for this pass) is a module that implements IntentCompilerFn
 * and exposes it, alongside a name, as an IntentCompiler value -- it
 * does not subclass or extend this struct.
 *
 * request_text is a borrowed (non-owned), required, non-empty C string:
 * the raw natural-language request. IntentCompiler MUST NOT retain a
 * reference to it past the call -- Intent (generation/intent.h) is the
 * only object that may outlive compile(), and Intent owns its own
 * copies of any string content it needs.
 *
 * context is a borrowed, optional reference to a Context (context/
 * context.h) -- not owned, may be NULL when no context narrows the
 * request (e.g. an ongoing composition's key/style). config is a
 * borrowed, optional Value (core/value.h) carrying compiler-specific
 * configuration -- not owned, may be NULL when a compiler takes none.
 * Both mirror Analyzer's context/config parameters exactly.
 *
 * On INTENT_COMPILER_OK, *out is a newly-created Intent the caller owns
 * (destroy with intent_destroy); on any other result, *out is left
 * untouched. Natural language MUST NOT directly manipulate the IR (TDD
 * Sec154) -- enforced structurally here by IntentCompiler only ever
 * producing an Intent, never reaching into the Canonical IR itself.
 */

typedef enum {
    INTENT_COMPILER_OK = 0,
    INTENT_COMPILER_ERR_NULL_REQUEST_TEXT,
    INTENT_COMPILER_ERR_NULL_OUT,
    INTENT_COMPILER_ERR_ALLOC,
    INTENT_COMPILER_ERR_COMPILATION_FAILED
} IntentCompilerResult;

typedef IntentCompilerResult (*IntentCompilerFn)(const char *request_text, const Context *context,
                                                  const Value *config, Intent **out);

typedef struct {
    const char *name;
    IntentCompilerFn compile;
} IntentCompiler;

#endif