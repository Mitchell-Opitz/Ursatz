#ifndef URSATZ_COMPOSER_RENDER_MOVEMENT_BUILD_H
#define URSATZ_COMPOSER_RENDER_MOVEMENT_BUILD_H

#include <realization/work.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/render.h>

/*
 * Private: not installed, not part of the API.
 *
 * A rendered plan as a work for the export (R-599, R-601): the work `w1` (no title, no composer) whose ensemble is the K12 entry
 * `omt.instrument.solo-piano`, with one movement `m1` titled for the form ("piece" when it has no name). The movement holds the plan's
 * meter map (a copy), the tempo of the export options as its tempo map, the key signatures of the render, the markers (each section's
 * label when `section_markers`, then `loopStart` and `loopEnd` when `loop_markers`), and one line per rendered part, in part order:
 * `melody`, `bass` and `inner-<voice>`, with the roles melody, bass and accompaniment, named as the export names its parts. A line's
 * notes are the render's events, in order: id `n<plan note id>`, onset, duration, MIDI number, the spelling of the event, and where the
 * plan names a source, a derivation (`n<source>` with the operation copy, motive or return; a source may be a plan note that is not
 * rendered, such as the motive of a material section). Then the movement is reduced to the piano of the snapshot: every line in one
 * part `piano`, melody and bass on the staves the rule of R-599 gives. A note of a render that has no plan (id 0) is named `e<n>`.
 *
 * PLAN_EXPORT_ERR_NOT_REPRESENTABLE, with a message, when the snapshot lacks the ensemble or its instrument or a value cannot be held.
 */
PlanExportResult rendered_plan_work(const RenderedPlan *rendered, const PlanExportOptions *options, const KnowledgeSnapshot *knowledge,
                                    Work **out, PlanExportError *error);

#endif
