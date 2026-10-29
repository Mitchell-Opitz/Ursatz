#ifndef URSATZ_TESTS_ACCURACY_SCORE_LAYOUT_H
#define URSATZ_TESTS_ACCURACY_SCORE_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include "romantext.h"
#include "core/rational.h"
#include "time/time_rational.h"
#include "time/meter_map.h"

/*
 * The measure layout of a MusicXML score (test infrastructure). The importer lays measures from the time
 * signature alone, so a score whose measures are not all one signature long (a pickup, a measure split by a
 * repeat or a fermata break, "7" then "7a") has barlines the MeterMap does not. A RomanText analysis counts
 * measures the way the score numbers them, so the harness reads where each measure of the first part starts
 * from the MusicXML itself.
 *
 *   - A measure's length is the furthest point its notes, backups and forwards reach (grace notes and chord
 *     members add nothing).
 *   - A measure whose number is the previous number plus a letter ("7a") continues that measure: its length is
 *     added to it, and it is not a measure of its own. So "7" is the whole of a measure the score split.
 *   - The signature is the first <time> of the part (beats / beat-type).
 *
 * The scan reads the tags it needs and nothing else; it is made for engraver-written MusicXML, not an XML
 * validator. All positions are in whole notes.
 */

typedef struct {
    int number;            /* the measure number as RomanText counts it (a pickup is 0) */
    Rational start;   /* whole notes from the start of the score */
    Rational length;
} ScoreMeasure;

typedef struct {
    unsigned time_top;     /* 0 when the score gives no <time> */
    unsigned time_bottom;
    ScoreMeasure *measures; /* owned; in order */
    size_t measure_count;
    size_t continued_count; /* measures folded into the one before ("7a") */
    size_t irregular_count; /* measures, other than a pickup or the last, not one signature long */
    bool has_pickup;        /* the first measure is shorter than one signature and numbered 0 */
    Rational end;      /* the end of the last measure */
} ScoreLayout;

typedef enum {
    SCORE_LAYOUT_OK = 0,
    SCORE_LAYOUT_ERR_NULL_ARGUMENT,
    SCORE_LAYOUT_ERR_NO_MEASURES,
    SCORE_LAYOUT_ERR_BAD_VALUE,
    SCORE_LAYOUT_ERR_ALLOC
} ScoreLayoutStatus;

ScoreLayoutStatus score_layout_read(const char *musicxml, size_t size, ScoreLayout *out);
void score_layout_destroy(ScoreLayout *layout);

/* The RomanText grid of the layout: measure, beat -> time on the score's own barlines. */
RomanTextGrid score_layout_grid(const ScoreLayout *layout);

/* A MeterMap of the layout: its signature from 0, and, with a pickup, a second region at the pickup's length so
   the pickup is measure 0 and measure 1 starts where the score's does. Caller destroys. NULL when the layout has
   no signature or the map cannot be built. */
MeterMap *score_layout_meter_map(const ScoreLayout *layout, const char *id);

#endif
