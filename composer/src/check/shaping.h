#ifndef URSATZ_COMPOSER_SHAPING_H
#define URSATZ_COMPOSER_SHAPING_H

/*
 * Schoenberg's shaping preferences (R-164): soft rules the melody and harmony checkers score with their others.
 * A checker that finds one broken sets its bit in PhraseVerdict.shaping_broken.
 */
#define SHAPING_ENDING_REDUCTION (1u << 0) /* ch. II  */
#define SHAPING_WAVES (1u << 1)            /* ch. XI  */
#define SHAPING_CENTRAL_RANGE (1u << 2)    /* ch. XI  */
#define SHAPING_MONOTONY (1u << 3)         /* ch. XII */
#define SHAPING_ROOT_PROGRESSION (1u << 4) /* ch. XII */
#define SHAPING_COUNT 5

#endif
