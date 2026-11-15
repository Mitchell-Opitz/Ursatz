#ifndef URSATZ_COMPOSER_RETURN_HEAR_H
#define URSATZ_COMPOSER_RETURN_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>

/*
 * Hearing the thematic returns of a written plan (R-070), the way the kernel's thematic return detection hears real
 * music. Only the plan's written lanes are read, never its constraints:
 *
 *   - each section of the piece (every section but the material) is heard with form_hear (form_hear.h), which finds the
 *     themes in it;
 *   - each theme becomes one entry of the detection: the Contour (up, down or same between successive notes) and the
 *     RhythmicPattern (the durations of the notes) of its melody lane notes, the notes that start inside the theme's
 *     bars, as Ursatz's tests/integration/framework_v1_thematic_returns.c builds them from a real piece, and as
 *     Ursatz-Analyzer's form plan stage ports it (rewritten here with error returns);
 *   - thematic_return_detection_build_relationships_configured, with the default configuration (windows of 6 events,
 *     similarity of at least 0.8, the lower of the contour score and the rhythm score of the best stretch pair),
 *     compares them and gives one "resembles" relationship from each later theme to each earlier one it matches.
 *
 * A theme the detection compares needs at least four melody notes; a section in which no theme is heard (a theme of
 * 4 bars is seldom heard) has no entry and so no return can be heard from or to it.
 */

/* One return heard: the later section's theme resembles a theme of the earlier section. */
typedef struct {
    NodeId earlier; /* the section node the earlier theme is in */
    NodeId later;   /* the section node of the later theme */
    double contour_score;
    double rhythm_score;
    double similarity; /* the lower of the two: the similarity qualifier of the relationship */
} ReturnHeardPair;

typedef struct {
    ReturnHeardPair *pairs; /* owned; NULL when none; one for each earlier and later section, the best of their themes */
    size_t count;
} ReturnHeard;

typedef enum {
    RETURN_HEAR_OK = 0,
    RETURN_HEAR_ERR_NULL,
    RETURN_HEAR_ERR_ALLOC,
    RETURN_HEAR_ERR_NOT_WRITTEN, /* a melody, bass or harmony span of a section is Constrained */
    RETURN_HEAR_ERR_ANALYSIS     /* a section form_hear cannot read, or a detection that failed */
} ReturnHearResult;

/* Hears the returns of plan into *out, which the caller destroys. On failure *out is empty. */
ReturnHearResult return_hear(const PiecePlan *plan, ReturnHeard *out);

void return_heard_destroy(ReturnHeard *heard);

/* The pair for earlier and later, or NULL when that return was not heard. */
const ReturnHeardPair *return_heard_find(const ReturnHeard *heard, NodeId earlier, NodeId later);

#endif
