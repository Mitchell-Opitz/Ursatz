#ifndef URSATZ_COMPOSER_MOTIVE_RENDER_H
#define URSATZ_COMPOSER_MOTIVE_RENDER_H

#include <ursatz_composer/export.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A motive as a file. The motive goes into a one-voice plan (a melody lane) as Given material, with its meter and, when
 * it has one, its key, and is written through export.h. To hear it in another key, move it first (motive_in_key).
 *
 * The plan is as long as the motive and starts at time 0, the first note's onset. A pickup is a short first meter span:
 * the pickup's length as its own meter (a pickup of 1 beat in 3/4 is a 1/4 span), then the motive's meter from the
 * first barline. Ursatz's MusicXML export writes that first bar as measure 0 with implicit="yes". A pickup that is not a
 * meter (a length whose denominator is not a power of two, such as 1/3 of a beat) is
 * PLAN_EXPORT_ERR_NOT_REPRESENTABLE. A motive that fits inside its pickup has the pickup's meter throughout.
 */

/* The plan the files are written from; NULL on failure. Caller frees with plan_destroy. */
PiecePlan *motive_render_plan(const Motive *motive);

PlanExportResult motive_export_midi(const Motive *motive, const PlanExportOptions *options, const char *path,
                                    PlanExportError *error);
PlanExportResult motive_export_musicxml(const Motive *motive, const PlanExportOptions *options, const char *path,
                                        PlanExportError *error);

#endif
