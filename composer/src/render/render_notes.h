#ifndef URSATZ_COMPOSER_RENDER_NOTES_H
#define URSATZ_COMPOSER_RENDER_NOTES_H

#include <ursatz_composer/render.h>

/*
 * Private: not installed, not part of the API.
 *
 * Notes that are in no plan yet, rendered the way a plan's are: a candidate the texture stage weighs against the melody
 * and the bass it would sound under. The same NoteEvents, in time order, spelled in key; a note's part is its lane and voice.
 */
typedef struct {
    Rational onset;    /* whole notes from the start of the notes */
    Rational duration;
    int midi;
    PlanLaneKind lane;
    int voice;
} RenderNote;

/* Renders count notes into *out, which the caller destroys with rendered_plan_destroy. No meter, key or section. */
PlanRenderResult render_notes(const RenderNote *notes, size_t count, const AnalysisKey *key, RenderedPlan *out);

#endif
