#ifndef URSATZ_COMPOSER_FORM_HEAR_H
#define URSATZ_COMPOSER_FORM_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Hearing the phrase-level form of a written plan through the Ursatz Analyzer (R-257). Only the plan's written lanes over
 * the span are read, never its constraints, its nodes or its planned chords: the plan is rendered, the notes that start
 * inside the span are heard by analyzer_hear in the plan's meter map and the key under the span's start, and the themes,
 * phrases and cadences are those of the Analyzer's result (analysis_chain_themes, analysis_chain_phrases and
 * analysis_chain_cadences of analyzer_result_chain). The Analyzer identifies the chords from the notes.
 *
 * The span must lie in one meter lane span, start on a barline and be whole bars long.
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

/* A cadence heard over the range, as the recognizer found it, in the key of the chord it arrives on. */
typedef struct {
    Rational arrival; /* where the chord the cadence arrives on starts, in whole notes from the start of the plan */
    int type;         /* CadenceType */
} FormHeardCadence;

typedef struct {
    FormHeardTheme *themes; /* owned; in the adapter's order; NULL when none */
    size_t count;
    size_t phrase_count;    /* the phrases standard-form segmented, with a theme or not */
    FormHeardCadence *cadences; /* owned; in order of arrival; NULL when none */
    size_t cadence_count;
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

/* A key the Analyzer heard from start on, until the next region's start (or the end of the span). */
typedef struct {
    Rational start; /* in whole notes from the start of the plan */
    AnalysisKey key;
} FormHeardKey;

/*
 * Hears the key regions of plan over span: one hearing of the span as form_hear does, then the key regions of the
 * Analyzer's chain (analysis_chain_key_regions), in order. *out is owned by the caller (free), NULL with *count 0 when
 * none were heard. The same errors as form_hear.
 */
FormHearResult form_hear_keys(const PiecePlan *plan, PlanSpan span, FormHeardKey **out, size_t *count);

/*
 * form_hear with the starts of the sections or phrases the caller knows of, plan times inside span. The Analyzer's hearing
 * takes no given boundaries, so the starts are not passed on and this is form_hear.
 */
FormHearResult form_hear_with_sections(const PiecePlan *plan, PlanSpan span, const Rational *section_starts,
                                       size_t section_count, FormHeard *out);

/* standard-form's own name of a PhraseLevelForm ("sentence", "period", ...). */
const char *form_hear_form_name(int form);

#endif
