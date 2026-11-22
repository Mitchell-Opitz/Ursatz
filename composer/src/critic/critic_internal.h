#ifndef URSATZ_COMPOSER_CRITIC_INTERNAL_H
#define URSATZ_COMPOSER_CRITIC_INTERNAL_H

#include <analysis/dimension/dimension.h>

#include <ursatz_composer/critic.h>

/* What the files of the critic share. Internal to this folder. */

/*
 * How far the notes of an idea are from the notes of the idea it contrasts against, in every aspect, by the library's measure
 * (dimension_contrast over the two ideas' notes from their starts, the ideas' lengths and the line's beat). *measured is false when
 * either idea has no notes or the measure cannot read them: a pair the measure cannot read is not held against the line.
 */
CriticResult critic_pair_contrast(const CriticLine *line, const CriticIdeaPair *pair, ContrastVector *vector, bool *measured);

#endif
