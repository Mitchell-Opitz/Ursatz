#ifndef URSATZ_COMPOSER_TEXTURE_CHECK_H
#define URSATZ_COMPOSER_TEXTURE_CHECK_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>

/*
 * The checker for the texture stage, where the accompaniment and the voicing operators are registered together:
 * voicing_checker for the candidates of voicing-satb, accompaniment_checker for the accompaniment's (and, through
 * either, harmony_phrase_checker for any other operator's). Pass it as FillConfig.checker.
 */
bool texture_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                     const FillCandidate *candidate, void *context, FillCheck *out);

#endif
