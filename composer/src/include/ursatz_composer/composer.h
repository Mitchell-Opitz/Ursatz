#ifndef URSATZ_COMPOSER_COMPOSER_H
#define URSATZ_COMPOSER_COMPOSER_H

#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/plan_json.h>

const char *composer_version(void);

/*
 * The stamp the Composer gives a plan it writes: its name and version, the producer key of the library chain it was built on, no
 * composition key yet and the hash of `knowledge` ("" for NULL).
 */
PlanProducer composer_plan_producer(const KnowledgeSnapshot *knowledge);

#endif
