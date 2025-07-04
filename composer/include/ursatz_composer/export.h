#ifndef URSATZ_COMPOSER_EXPORT_H
#define URSATZ_COMPOSER_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/render.h>

/*
 * Export: a plan as a MIDI file or a MusicXML file. The Composer writes neither
 * format itself. It renders the plan (render.h) and hands the NoteEvents,
 * meter and key signatures to Ursatz's MIDI renderer (midi_renderer_render)
 * and MusicXML exporter (musicxml_export_write_score), then writes the file.
 *
 * MIDI: one track per part, at 480 ticks per quarter when every time lands on a
 * whole tick there and otherwise at the smallest division that does. A plan
 * with no tempo carries none (the file default, 120, applies). With
 * PlanExportOptions.track_names (the default) the conductor track is named for
 * the form ("piece" when the form has no name) and each part's track for its
 * lane, as the MusicXML parts are ("melody", "bass", "inner_voice_2").
 *
 * MusicXML: one part per lane voice, named for its lane ("melody", "bass",
 * "inner_voice_2"). With no meter span the document is flagged
 * meter-defaulted (4/4).
 *
 * A file is written to "<path>.tmp" and renamed, so a failed export leaves no
 * half-written file at path.
 */

#define PLAN_EXPORT_MESSAGE_MAX 160

typedef enum {
    PLAN_EXPORT_OK = 0,
    PLAN_EXPORT_ERR_NULL,
    PLAN_EXPORT_ERR_NO_CONTENT,         /* no Given or Realized note to write */
    PLAN_EXPORT_ERR_IO,                 /* the file could not be written */
    PLAN_EXPORT_ERR_NOT_REPRESENTABLE,  /* Ursatz refused the notes, meter or key as given */
    PLAN_EXPORT_ERR_TOO_MANY_VOICES,    /* MIDI has 15 voice tracks */
    PLAN_EXPORT_ERR_OVERLAP,            /* two notes overlap within one voice (MusicXML) */
    PLAN_EXPORT_ERR_ALLOC
} PlanExportResult;

/* Filled on failure. Pass NULL when the details are not wanted. */
typedef struct {
    char message[PLAN_EXPORT_MESSAGE_MAX];
} PlanExportError;

/* What an export adds to the notes. Every export function takes const PlanExportOptions *; NULL means the default. */
typedef struct {
    bool track_names; /* MIDI: name the conductor track and each part's track */
} PlanExportOptions;

/* Track names on. */
PlanExportOptions plan_export_options_default(void);

/* From an already rendered plan, to a buffer the caller frees with free(). */
PlanExportResult rendered_plan_midi_bytes(const RenderedPlan *rendered, const PlanExportOptions *options,
                                          unsigned char **out_data, size_t *out_size, PlanExportError *error);
PlanExportResult rendered_plan_musicxml_text(const RenderedPlan *rendered, const PlanExportOptions *options,
                                             char **out_xml, PlanExportError *error);

/* From an already rendered plan, to a file. */
PlanExportResult rendered_plan_export_midi(const RenderedPlan *rendered, const PlanExportOptions *options,
                                           const char *path, PlanExportError *error);
PlanExportResult rendered_plan_export_musicxml(const RenderedPlan *rendered, const PlanExportOptions *options,
                                               const char *path, PlanExportError *error);

/* Renders the plan, then writes the file. Use the rendered_plan_ functions to render once and write both. */
PlanExportResult plan_export_midi(const PiecePlan *plan, const PlanExportOptions *options, const char *path,
                                  PlanExportError *error);
PlanExportResult plan_export_musicxml(const PiecePlan *plan, const PlanExportOptions *options, const char *path,
                                      PlanExportError *error);

#endif
