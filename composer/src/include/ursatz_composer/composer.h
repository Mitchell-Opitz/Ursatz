#ifndef URSATZ_COMPOSER_COMPOSER_H
#define URSATZ_COMPOSER_COMPOSER_H

#include <theory/knowledge/knowledge_snapshot.h>
#include <plan/plan_json.h>

const char *composer_version(void);

/* The key of the code that composes (R-626): 16 lowercase hex digits, a hash of its sources, built into the Composer. */
const char *composer_composition_key(void);

/*
 * The stamp the Composer gives a plan it writes: its name and version, the producer key of the library chain it was built on, no
 * composition key and the hash of `knowledge` ("" for NULL).
 */
PlanProducer composer_plan_producer(const KnowledgeSnapshot *knowledge);

#endif
