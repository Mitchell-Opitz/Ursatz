#include <ursatz_composer/composer.h>

#include <stdio.h>

#include <chain/analysis_chain.h>
#include <composer/composition_key.h>

const char *composer_version(void) {
    return URSATZ_COMPOSER_VERSION;
}

const char *composer_composition_key(void) {
    return URSATZ_COMPOSER_COMPOSITION_KEY;
}

PlanProducer composer_plan_producer(const KnowledgeSnapshot *knowledge) {
    PlanProducer producer;
    snprintf(producer.name, sizeof(producer.name), "%s", "ursatz-composer");
    snprintf(producer.version, sizeof(producer.version), "%s", composer_version());
    snprintf(producer.producer_key, sizeof(producer.producer_key), "%s", analysis_chain_producer_key());
    snprintf(producer.composition_key, sizeof(producer.composition_key), "%s", composer_composition_key());
    snprintf(producer.knowledge_hash, sizeof(producer.knowledge_hash), "%s", knowledge ? knowledge_snapshot_hash(knowledge) : "");
    return producer;
}
