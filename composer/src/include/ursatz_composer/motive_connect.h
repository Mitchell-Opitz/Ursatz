#ifndef URSATZ_COMPOSER_MOTIVE_CONNECT_H
#define URSATZ_COMPOSER_MOTIVE_CONNECT_H

#include <stdbool.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/variant_registry.h>

/*
 * How well a motive-form connects to the idea before it in a theme (R-145), used when grow builds its ideas:
 *
 *   score = (2 x common + 1 x rhythm + 1 x harmony) / 4, times 1/2 when the candidate's remoteness is over the limit,
 *   3/4 (1/2 at a fast tempo: motive_connection_limited)
 *
 *   common   1 minus the candidate's distance from the basic motive (the tree root, not the idea before it)
 *   rhythm   the rhythm part of motive_recognizability of the candidate against the idea before it
 *   harmony  1 when the idea before's last chord to the candidate's first chord is a progression the phrase model
 *            allows (tonic to predominant, dominant or tonic; predominant to dominant; dominant to tonic), 1/2 for a
 *            repeated chord, 0 otherwise and when either has no chord (no key, or not a full motive)
 *
 * A chord is the motive's own harmony, else its best implied chord. The tonic chords are degrees I, iii and vi, the
 * predominants ii and IV, the dominants V and vii. Every part and the score are exact fractions in lowest terms.
 */

typedef struct {
    MotiveScore common;
    MotiveScore rhythm;
    MotiveScore harmony;
    /* The remoteness of the candidate: its distance from the basic motive (1 - common). */
    MotiveScore remoteness;
    /* The remoteness limit used: 3/4, or the limit given to motive_connection_limited. */
    MotiveScore limit;
    /* Remoteness was over the limit, so the total is halved. */
    bool halved;
    MotiveScore total;
} MotiveConnection;

/*
 * The distance of a chain of registry steps, summed over its relations' costs (R-142): cost / (cost + 4), in lowest
 * terms; the exact chain is 0/1. False for a chain with a step that is not in the registry.
 */
bool motive_chain_distance(const VariantChain *chain, MotiveScore *out);

/*
 * The parts and the score of candidate as a motive-form of basic that follows previous. distance is the candidate's
 * distance from basic when the caller knows it (motive_chain_distance of the chain that made it); NULL searches it with
 * the library's recognizer (variant_set_remoteness), which is slow for a candidate no chain reaches. previous may be
 * NULL, for basic. False for NULL arguments, a motive that is not valid or a search that failed; *out is untouched
 * then.
 */
bool motive_connection(const Motive *basic, const Motive *previous, const Motive *candidate, const MotiveScore *distance,
                       MotiveConnection *out);

/* The remoteness limit at a tempo of this many quarter notes per minute or more (R-163): 1/2 instead of 3/4. */
#define MOTIVE_FAST_TEMPO_QPM 140

/*
 * motive_connection with the remoteness limit given: limit NULL, or a denominator of 0, is 3/4 (motive_connection is
 * this with NULL).
 */
bool motive_connection_limited(const Motive *basic, const Motive *previous, const Motive *candidate,
                               const MotiveScore *distance, const MotiveScore *limit, MotiveConnection *out);

/* motive_connection, the total only, with the distance searched. */
bool motive_connection_score(const Motive *basic, const Motive *previous, const Motive *candidate, MotiveScore *out);

#endif
