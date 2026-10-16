#ifndef URSATZ_COMPOSER_GIVEN_H
#define URSATZ_COMPOSER_GIVEN_H

#include <stdbool.h>
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
    GIVEN_ERR_NOT_REALIZED, /* the section to give is not realized: harmony, bass and melody are not all written */
    GIVEN_ERR_LENGTH,      /* the section to give is not as long as the section, pickup included */
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

/* One chord at an exact place: onset from the section's start, length to its end. */
typedef struct {
    const char *numeral;
    PlanTime onset;
    PlanTime length;
} GivenChord;

/*
 * One Given harmony span for each chord, exactly its onset and length (not on the cell grid), numerals read as
 * plan_give_progression reads them. The chords lie inside the section, each inside one key span, clear of each other
 * and of the harmony already there; the meter must be one the harmony operators support. A refused call leaves the
 * plan as it was.
 */
GivenResult plan_give_chords(PiecePlan *plan, NodeId section, const GivenChord *chords, size_t count);

/* A bass span, as long as the notes need in whole bars, Given. */
GivenResult plan_give_bass(PiecePlan *plan, NodeId section, size_t bar, const PlanNote *notes, size_t count);

/*
 * A realized section of another plan (a theme the theme command composed) given to a section of this one: its harmony,
 * bass, melody and inner voice spans become Given spans owned by the section, moved to its start and transposed by the
 * key difference between the two (the key lanes must have the same shape and mode), and its phrase and idea nodes are
 * copied under the section so that form_hear hears it as the theme it was. The section ends Realized, so no stage
 * targets it. References to motive nodes of the source plan are dropped: the plan holds none of them.
 *
 * The section must be a theme section of the plan with no nodes under it and nothing of the copy's lanes in it. The
 * source must be realized (GIVEN_ERR_NOT_REALIZED), as long as the section (GIVEN_ERR_LENGTH, which also refuses a
 * span that would start before the section's pickup) and in the same meter (GIVEN_ERR_METER). A refused call leaves
 * the plan as it was.
 */
GivenResult plan_give_section(PiecePlan *plan, NodeId section, const PiecePlan *source, NodeId source_section);

/*
 * The parts of span that no Given span of the lane and voice covers, in time order: *out receives up to capacity
 * spans, and the count in all is returned. A span nothing Given touches is its own single gap; a span covered wholly
 * has none. The operators that open targets (grow-theme) open them only over the gaps.
 */
size_t given_gaps(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, PlanSpan *out, size_t capacity);

/* Whether a Given span of the lane and voice overlaps the span. */
bool given_touches(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span);

/*
 * A copy of the Given material of a plan that the user placed (harmony, bass, melody and inner voice spans owned by a
 * section; the meter, the key and the motive m0 are the form's), to check after a run that nothing changed it.
 */
typedef struct GivenSnapshot GivenSnapshot;

/* NULL when out of memory; a plan with no such material gives a snapshot of none. */
GivenSnapshot *given_snapshot(const PiecePlan *plan);
void given_snapshot_destroy(GivenSnapshot *snapshot);

/* One run of Given spans in a row (a progression is a span a chord), as the snapshot found it and as the plan holds it now. */
typedef struct {
    PlanLaneKind lane;
    int voice;
    NodeId owner;
    PlanSpan span;    /* from the start of the first span to the end of the last */
    size_t spans;
    bool unchanged;   /* every span is still there, still Given, with the same payload byte for byte */
} GivenReport;

/* The runs in the snapshot, in the order of their spans; up to capacity are written, the count in all is returned. */
size_t given_snapshot_report(const GivenSnapshot *snapshot, const PiecePlan *plan, GivenReport *out, size_t capacity);

#endif
