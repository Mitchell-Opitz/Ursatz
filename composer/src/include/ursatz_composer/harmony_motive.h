#ifndef URSATZ_COMPOSER_HARMONY_MOTIVE_H
#define URSATZ_COMPOSER_HARMONY_MOTIVE_H

#include <ursatz_composer/fill_operator.h>

/*
 * The harmony operator for a target whose ideas were developed from motives (R-344): the chords the motives stage wrote
 * on the ideas' motive nodes stay as they are, and only the cells they leave open are filled (the cadence, a shape anchor the
 * chord breaks, a cell no motive chord covers), by the phrase model over each run of open cells. A target without motive
 * chords is not its: the other harmony operators write it.
 */

#define HARMONY_MOTIVE_OPERATOR_NAME "harmony-motive"

/* Registers the operator, working around Given material as harmony_phrase_register_given does. */
FillResult harmony_motive_register(FillOperatorRegistry *registry);

#endif
