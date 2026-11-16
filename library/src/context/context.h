#ifndef MUSIC_CONTEXT_CONTEXT_H
#define MUSIC_CONTEXT_CONTEXT_H

/*
 * Context (Category / base type): the conditions under which an
 * assertion is evaluated. Only the opaque type exists: the rule
 * callbacks (ConstraintEvaluateFn, PreferenceScoreFn) take a
 * `const Context *`, and nothing constructs one.
 */

typedef struct Context Context;

#endif
