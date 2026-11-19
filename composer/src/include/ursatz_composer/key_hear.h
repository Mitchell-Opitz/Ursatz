#ifndef URSATZ_COMPOSER_KEY_HEAR_H
#define URSATZ_COMPOSER_KEY_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/form_spec.h>
#include <plan/plan.h>

/* The most key spans of a plan's key lane: a span for the start and the end of every section of the largest form, and one more. */
#define MAX_SEGMENTS (2 * FORM_SPEC_MAX_SECTIONS + 1)

typedef struct ComposerHearing ComposerHearing;

typedef struct {
    bool ok;
    size_t changes;     /* the key changes in the key lane (a span in another key than the one before it) */
    size_t modulations; /* of them, those that open a key a section ends in and are confirmed as a modulation */
    char reason[96];    /* the first change that is not, when it is not ok */
} PieceKeyCheck;

/*
 * piece_check_keys_in (check/hearing.h) checks the keys of a composed piece against standard-harmony (R-069): every key is closely related to the home
 * key (the key of the first span), and every key change that opens a key a section ends in is a modulation, not a
 * tonicization. The change is classified on its length and what follows the cadence (harmony_key_change_classify:
 * the chords in the new key, the planned cadence in it, whether the key goes on after it), then confirmed by the
 * cadences heard (harmony_key_change_confirm): an authentic cadence must arrive inside the new key's span, as form_hear_in
 * hears it in the key of its chord. The plan's sections must be written. True when the plan could be read; the result
 * is in out.
 */

bool piece_check_keys_in(ComposerHearing *hearing, const Plan *plan, PieceKeyCheck *out);

#endif
