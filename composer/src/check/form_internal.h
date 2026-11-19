#ifndef URSATZ_COMPOSER_FORM_INTERNAL_H
#define URSATZ_COMPOSER_FORM_INTERNAL_H

#include <ursatz_composer/form_hear.h>

#include "check/hearing.h"

/*
 * Hears the form of plan over span into *out, which the caller destroys (on failure *out is empty), over a hearing the
 * caller already has (check/hearing.h): the Analyzer's result over span, kept by the hearing, is read instead of a new
 * analysis. The plan is the one the hearing was made from.
 */
FormHearResult form_hear_in(ComposerHearing *hearing, const Plan *plan, PlanSpan span, FormHeard *out);

#endif
