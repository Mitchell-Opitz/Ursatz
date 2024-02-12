#ifndef URSATZ_COMPOSER_FORM_HEAR_H
#define URSATZ_COMPOSER_FORM_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * Hearing the phrase-level form of a written plan, the way standard-form's analysis adapter hears real
 * music. Only the plan's written lanes over the span are read, never its constraints or its nodes:
 *
 *   - the melody lane (voice 0) is the soprano, the bass lane the bass, each rendered by plan_render;
 *   - the meter lane gives the MeterMap, the key lane the scale degree of every note;
 *   - each harmony lane chord is one segment;
 *   - the cadences are what standard-harmony's cadence recognizer hears over the chords, the soprano of
 *     each chord being the melody note sounding at its start (not the soprano a cadence constraint
 *     implies), each with harmony_cadence_strength's rank and uncertainty 0.
 *
 * Then standard-form segments the phrases (through the kernel's Phrase Boundary Detection), divides them
 * into ideas and identifies the themes, with its default configuration and the bass included.
 *
 * The span must lie in one meter lane span and one key lane span, start on a barline and be whole bars
 * long, and its chords must tile it, each a whole number of strong beats.
 */

/* One theme heard. Bars are 0-based from the start of the plan. */
typedef struct {
    int form;           /* PhraseLevelForm */
    int beginning;      /* PhraseBeginning, -1 when the form has none */
    int ending;         /* PhraseEnding, -1 likewise */
    int commonality;    /* StandardFormCommonality of the pairing, -1 when it has none */
    size_t first_bar;
    size_t bars;
    size_t phrase_count;
    int first_cadence;  /* CadenceType of the first part's phrase, -1 when absent */
    int second_cadence; /* CadenceType of the second part's phrase, -1 when absent */
    bool archetypal;
    bool has_fragmentation;
} FormHeardTheme;

typedef struct {
    FormHeardTheme *themes; /* owned; in the adapter's order; NULL when none */
    size_t count;
    size_t phrase_count;    /* the phrases standard-form segmented, with a theme or not */
} FormHeard;

void form_heard_destroy(FormHeard *heard);

typedef enum {
    FORM_HEAR_OK = 0,
    FORM_HEAR_ERR_NULL,
    FORM_HEAR_ERR_ALLOC,
    FORM_HEAR_ERR_NOT_WRITTEN, /* a melody, bass or harmony span over the range is Constrained */
    FORM_HEAR_ERR_METER,       /* no meter, or a span the meter cannot place */
    FORM_HEAR_ERR_ANALYSIS     /* a lane the adapter cannot read, or an analysis that failed */
} FormHearResult;

const char *form_hear_result_name(FormHearResult result);

/* Hears the form of plan over span into *out, which the caller destroys. On failure *out is empty. */
FormHearResult form_hear(const PiecePlan *plan, PlanSpan span, FormHeard *out);

/* standard-form's own name of a PhraseLevelForm ("sentence", "period", ...). */
const char *form_hear_form_name(int form);

#endif
