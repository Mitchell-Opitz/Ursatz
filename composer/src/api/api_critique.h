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

/* {"numerator", "denominator"}; NULL when memory runs out. */
cJSON *api_critic_ratio_json(Rational value);

/* The evaluators of a judgment, [{"rule", "penalty", "weight", "reversed"}] in evaluator order; NULL when memory runs out. */
cJSON *api_critic_evaluators_json(const CriticFinding findings[CRITIC_EVALUATOR_COUNT], const CriticSettings *settings);

#endif
