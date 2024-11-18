#ifndef URSATZ_COMPOSER_GIVEN_H
#define URSATZ_COMPOSER_GIVEN_H

#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Given material placed anywhere in a plan (R-071): a phrase's melody, a progression or a bass line, as Given lane
 * spans owned by a section, from a bar of it. The stages grow the form around the spans and fill the rest, never
 * changing them.
 *
 * `bar` counts from 0, the first bar of the section. Every call checks the whole of the material before it adds any
 * of it, so a refused call leaves the plan as it was. The section must be a theme section of the plan (not the
 * material section). The material:
 *   - lies inside the section: a span that would cross the section's end is GIVEN_ERR_PAST_END;
 *   - lies inside one key span of the key lane (GIVEN_ERR_KEY), in the plan's meter;
 *   - does not overlap a span of its lane that is already there (GIVEN_ERR_OVERLAP).
 *
 * A melody or a bass line is one lane span from the bar, as many whole bars long as the notes need; a note's offset is
 * from the start of that bar. A melody may open and end with a rest; a bass line is written without rests, its notes
 * filling the bars, because the bass lane is read as one unbroken line. A progression is one harmony span for each
 * chord, cells_per_chord harmony cells long (a cell is a beat or a half bar, as harmony_phrase.h has it); its numerals
 * are parsed in the mode of the key under them (harmony_roman_parse, then harmony_roman_parse_applied_inverted) and
 * GIVEN_ERR_BAD_NUMERAL names the first that cannot be read.
 */
typedef enum {
    GIVEN_OK = 0,
    GIVEN_ERR_NULL,
    GIVEN_ERR_NOT_SECTION, /* the node is not a theme section of the plan */
    GIVEN_ERR_BAD_NOTES,   /* no notes, a pitch outside 0 to 127, a note that overlaps another, a bass line with a rest */
    GIVEN_ERR_BAD_NUMERAL, /* a numeral that cannot be read, or no chords, or no cells for a chord */
    GIVEN_ERR_BAD_BAR,     /* the bar is outside the section */
    GIVEN_ERR_PAST_END,    /* the material would cross the section's end */
    GIVEN_ERR_KEY,         /* the material is not inside one key span */
    GIVEN_ERR_METER,       /* the section has no meter the harmony operators support */
    GIVEN_ERR_OVERLAP,     /* a span of the same lane is already there */
    GIVEN_ERR_PLAN         /* the plan refused the span (allocation, or an unforeseen refusal) */
} GivenResult;

/* "ok", "not a theme section", "past the section's end" and so on. */
const char *given_result_name(GivenResult result);

/* A melody span, as long as the notes need in whole bars, Given. */
GivenResult plan_give_melody(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count);

/*
 * The numerals, one chord each, cells_per_chord cells long, from the bar. Chords may begin anywhere a cell does, so a
 * progression may end inside a bar.
 */
GivenResult plan_give_progression(PiecePlan *plan, NodeId section, size_t bar, const char *const *numerals, size_t count,
                                  size_t cells_per_chord);

/* A bass span, as long as the notes need in whole bars, Given. */
GivenResult plan_give_bass(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count);

#endif
