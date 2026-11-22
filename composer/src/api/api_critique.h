#ifndef URSATZ_COMPOSER_API_CRITIQUE_H
#define URSATZ_COMPOSER_API_CRITIQUE_H

#include <cJSON.h>

#include <ursatz_composer/critic.h>

/*
 * A judgment as the critique response and the theme response write it (R-721, R-722): {"score", "evaluators": [{"rule", "penalty",
 * "weight", "reversed"}], "observations": [{"rule", "scored", "bar", "beat", "message"}]}, every Rational as {"numerator",
 * "denominator"}, bar and beat null for an observation with no place. The caller deletes it; NULL when memory runs out.
 */
cJSON *api_critic_json(const CriticJudgment *judgment, const CriticSettings *settings);

#endif
