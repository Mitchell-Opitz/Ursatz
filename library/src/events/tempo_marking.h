#ifndef MUSIC_EVENTS_TEMPO_MARKING_H
#define MUSIC_EVENTS_TEMPO_MARKING_H

#include <stdbool.h>

#include "time/metronome_marking.h"
#include "time/temporal_anchor.h"

/*
 * TempoMarking: a tempo indication at a position. A tempo is given
 * specifically, by a MetronomeMarking (quarter = 60), less specifically,
 * by a textual indication (allegro, andante cantabile, rit., a tempo), or
 * both -- "Allegro (quarter = 120)" is common. At least one of the two is
 * required; a TempoMarking with neither says nothing.
 *
 * text is an opaque, uninterpreted string, not a closed enum, the same
 * treatment events/dynamics.h and events/articulation.h give their
 * marking/kind: the vocabulary (allegro, rit., accel., a tempo, assai,
 * espressivo, ...) is explicitly open-ended. What the words mean -- their
 * grouping and order, that a ritardando is a gradual slowing -- is
 * framework knowledge (frameworks/standard-markings), not this type's.
 *
 * Positioned by a TemporalAnchor, like ControlEvent
 * (events/control_event.h): a tempo change occurs at a point, not on a
 * single note. No mutator: a revision is a new TempoMarking.
 */

typedef struct TempoMarking TempoMarking;

typedef enum {
    TEMPO_MARKING_OK = 0,
    TEMPO_MARKING_ERR_NULL_TEMPORAL_ANCHOR,
    TEMPO_MARKING_ERR_EMPTY, /* neither text nor metronome marking supplied */
    TEMPO_MARKING_ERR_ALLOC
} TempoMarkingResult;

/*
 * Takes ownership of temporal_anchor on success, and of metronome_marking
 * if non-NULL (both destroyed by tempo_marking_destroy). text is copied;
 * pass NULL (or "") for none. At least one of text and metronome_marking
 * must be supplied.
 */
TempoMarkingResult tempo_marking_create(TemporalAnchor *temporal_anchor, const char *text,
                                        MetronomeMarking *metronome_marking, TempoMarking **out);
void tempo_marking_destroy(TempoMarking *tempo_marking);

const TemporalAnchor *tempo_marking_temporal_anchor(const TempoMarking *tempo_marking);

/* NULL when no text was supplied. */
const char *tempo_marking_text(const TempoMarking *tempo_marking);
bool tempo_marking_has_text(const TempoMarking *tempo_marking);

/* NULL when no metronome marking was supplied. */
const MetronomeMarking *tempo_marking_metronome_marking(const TempoMarking *tempo_marking);
bool tempo_marking_has_metronome_marking(const TempoMarking *tempo_marking);

#endif
