#include <ursatz_composer/composer.h>

#include <stdio.h>

#include <chain/analysis_chain.h>

const char *composer_version(void) {
    return URSATZ_COMPOSER_VERSION;
}

PlanProducer composer_plan_producer(const KnowledgeSnapshot *knowledge) {
    PlanProducer producer;
    snprintf(producer.name, sizeof(producer.name), "%s", "ursatz-composer");
    snprintf(producer.version, sizeof(producer.version), "%s", composer_version());
    snprintf(producer.producer_key, sizeof(producer.producer_key), "%s", analysis_chain_producer_key());
    producer.composition_key[0] = '\0';
    snprintf(producer.knowledge_hash, sizeof(producer.knowledge_hash), "%s", knowledge ? knowledge_snapshot_hash(knowledge) : "");
    return producer;
}
