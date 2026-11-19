#ifndef URSATZ_COMPOSER_EXPORT_H
#define URSATZ_COMPOSER_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <plan/plan.h>
#include <ursatz_composer/render.h>

/*
 * Export: a plan as a MIDI file or a MusicXML file. The Composer writes neither format itself. It renders the plan (render.h), makes
 * a work of it (render/movement_build.h) and hands that to the library's movement export (io/movement_export.h), which writes the
 * file; the Composer returns the bytes and the caller writes the file (R-599).
 *
 * The work is one movement whose lines are all assigned to one piano part (the K12 entry `omt.instrument.piano`, from the knowledge
 * snapshot the call takes): a line for each lane voice, named for its lane ("melody", "bass", "inner_voice_2"), the melody on the
 * grand staff's upper staff, the bass on the lower and an inner voice on the upper when the mean of its notes is middle C or above,
 * else on the lower. A plan's note ids go with it: the performer note of a plan note is "piano-n<id>" (the MusicXML xml:id), and a
 * line note names the plan note it was made from where the plan records one.
 *
 * MIDI: track 0 is the conductor and track 1 the piano, named "Piano", with one channel per line in the order the lines first sound and
 * a program change (the piano, 0) on each; at 480 ticks per quarter when every time lands on a whole tick there and otherwise at the
 * smallest division that does. A plan with no tempo carries none (the file default, 120, applies). With PlanExportOptions.track_names
 * (the default) the conductor track is named for the form ("piece" when the form has no name) and the piano track "Piano". With
 * section_markers (the default) each section that is not material gets a marker in the conductor track at its start, its
 * SECTION_LETTER ("A", "B", "A'") or else its node name. With a tempo the file carries one tempo region from 0 (a plan stores none;
 * the tempo is an export option only).
 *
 * With loop_markers the conductor track also carries a marker "loopStart" at loop_start and one "loopEnd" at loop_end,
 * after the section markers; a game reads them to repeat the region between. MusicXML has none.
 *
 * MusicXML: one part "piano" of two staves (one when no line is on the lower staff), named "Piano", with the tempo written as
 * <sound tempo>. Names and markers are MIDI only (Ursatz's MusicXML export writes no rehearsal marks). With no meter span the
 * document is flagged meter-defaulted (4/4).
 */

#define PLAN_EXPORT_MESSAGE_MAX 160

typedef enum {
    PLAN_EXPORT_OK = 0,
    PLAN_EXPORT_ERR_NULL,
    PLAN_EXPORT_ERR_NO_CONTENT,         /* no Given or Realized note to write */
    PLAN_EXPORT_ERR_NOT_REPRESENTABLE,  /* Ursatz refused the notes, meter or key as given */
    PLAN_EXPORT_ERR_TOO_MANY_VOICES,    /* the piano part holds 15 lines on MIDI's 15 channels */
    PLAN_EXPORT_ERR_OVERLAP,            /* two notes overlap within one voice; MusicXML now writes them as extra voices, so this is unused */
    PLAN_EXPORT_ERR_ALLOC
} PlanExportResult;

/* Filled on failure. Pass NULL when the details are not wanted. */
typedef struct {
    char message[PLAN_EXPORT_MESSAGE_MAX];
} PlanExportError;

/* What an export adds to the notes. Every export function takes const PlanExportOptions *; NULL means the default. */
typedef struct {
    unsigned long tempo_numerator;   /* quarter notes per minute as a ratio; 0 and 0 is no tempo */
    unsigned long tempo_denominator;
    bool track_names;                /* MIDI: name the conductor track and each part's track */
    bool section_markers;            /* MIDI: a marker at the start of each section that is not material */
    bool loop_markers;               /* MIDI: markers "loopStart" at loop_start and "loopEnd" at loop_end (R-172) */
    Rational loop_start;             /* whole notes from the start of the piece; used with loop_markers */
    Rational loop_end;
} PlanExportOptions;

/* No tempo, no loop markers, track names and section markers on. A tempo that is not 0/0 needs both parts at least 1 and rounds to
   a whole number of microseconds per quarter note, at most 0xFFFFFF. */
PlanExportOptions plan_export_options_default(void);

/*
 * From an already rendered plan, to a buffer the caller frees with free(). `knowledge` is the snapshot the piano and its ensemble are
 * read from (the call context's); PLAN_EXPORT_ERR_NOT_REPRESENTABLE when it lacks them. More than 15 lines are
 * PLAN_EXPORT_ERR_TOO_MANY_VOICES: MIDI has 15 channels for the piano part.
 */
PlanExportResult rendered_plan_midi_bytes(const KnowledgeSnapshot *knowledge, const RenderedPlan *rendered,
                                          const PlanExportOptions *options, unsigned char **out_data, size_t *out_size,
                                          PlanExportError *error);
PlanExportResult rendered_plan_musicxml_text(const KnowledgeSnapshot *knowledge, const RenderedPlan *rendered,
                                             const PlanExportOptions *options, char **out_xml, PlanExportError *error);

/*
 * Renders plan for an export into *rendered, which the caller destroys with rendered_plan_destroy: the one place a
 * command renders a plan to write files, then gets any number of them as bytes or text with the rendered_plan_ functions.
 * error may be NULL.
 */
PlanExportResult plan_export_render(const PiecePlan *plan, RenderedPlan *rendered, PlanExportError *error);

#endif
