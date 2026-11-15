#include <ursatz_composer/texture_check.h>

#include <string.h>

#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/voicing.h>

bool texture_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                     const FillCandidate *candidate, void *context, FillCheck *out) {
    if (operator_ && strcmp(operator_->generator.name, VOICING_OPERATOR_NAME) == 0) {
        return voicing_checker(plan, target, operator_, candidate, context, out);
    }
    if (operator_ && strcmp(operator_->generator.name, COUNTERPOINT_OPERATOR_NAME) == 0) {
        return counterpoint_checker(plan, target, operator_, candidate, context, out);
    }
    return accompaniment_checker(plan, target, operator_, candidate, context, out);
}
