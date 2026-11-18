#include <ursatz_composer/export.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <io/midi_renderer.h>
#include <io/movement_export.h>
#include <realization/work.h>

#include "render/movement_build.h"
#include "util/util.h"

static void set_message(PlanExportError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

PlanExportOptions plan_export_options_default(void) {
    PlanExportOptions options;
    memset(&options, 0, sizeof(options));
    options.track_names = true;
    options.section_markers = true;
    return options;
}

static PlanExportOptions resolve_options(const PlanExportOptions *options) {
    return options ? *options : plan_export_options_default();
}

/* ---------------------------------------------------------- the piano part */

/*
 * Both exports build the work of the rendered plan (movement_build.h): one movement whose lines are all assigned to one piano part,
 * then write it through the library's movement export (io/movement_export.h).
 */
static PlanExportResult from_movement(MovementExportResult result, const char *message, PlanExportError *error) {
    switch (result) {
    case MOVEMENT_EXPORT_OK:
        return PLAN_EXPORT_OK;
    case MOVEMENT_EXPORT_ERR_NULL:
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    case MOVEMENT_EXPORT_ERR_NO_NOTES:
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    case MOVEMENT_EXPORT_ERR_TOO_MANY_VOICES:
        set_message(error, "more than %d lines in the performer parts; MIDI has %d channels for them", MIDI_RENDER_MAX_VOICE_TRACKS,
                    MIDI_RENDER_MAX_VOICE_TRACKS);
        return PLAN_EXPORT_ERR_TOO_MANY_VOICES;
    case MOVEMENT_EXPORT_ERR_ALLOC:
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    case MOVEMENT_EXPORT_ERR_NOT_BUILT:
    case MOVEMENT_EXPORT_ERR_NOT_REPRESENTABLE:
        break;
    }
    set_message(error, "%s", message[0] ? message : "the plan cannot be written");
    return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
}

PlanExportResult rendered_plan_midi_bytes(const KnowledgeSnapshot *knowledge, const RenderedPlan *rendered,
                                          const PlanExportOptions *options_in, unsigned char **out_data, size_t *out_size,
                                          PlanExportError *error) {
    PlanExportOptions options = resolve_options(options_in);
    if (!rendered || !out_data || !out_size) {
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }
    Work *work = NULL;
    PlanExportResult built = rendered_plan_work(rendered, &options, knowledge, &work, error);
    if (built != PLAN_EXPORT_OK) {
        return built;
    }
    /* The plain export (no track names) also has no section markers: rendered_plan_work leaves them out with the option. */
    MovementMidiOptions midi_options = {options.track_names};
    char message[PLAN_EXPORT_MESSAGE_MAX] = "";
    MovementExportResult result = movement_export_midi(work_movement_at(work, 0), &midi_options, out_data, out_size, message, sizeof(message));
    work_destroy(work);
    return from_movement(result, message, error);
}

PlanExportResult rendered_plan_musicxml_text(const KnowledgeSnapshot *knowledge, const RenderedPlan *rendered,
                                             const PlanExportOptions *options_in, char **out_xml, PlanExportError *error) {
    PlanExportOptions options = resolve_options(options_in);
    if (!rendered || !out_xml) {
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }
    Work *work = NULL;
    PlanExportResult built = rendered_plan_work(rendered, &options, knowledge, &work, error);
    if (built != PLAN_EXPORT_OK) {
        return built;
    }
    char message[PLAN_EXPORT_MESSAGE_MAX] = "";
    MovementExportResult result = movement_export_musicxml(NULL, work_movement_at(work, 0), out_xml, message, sizeof(message));
    work_destroy(work);
    return from_movement(result, message, error);
}

PlanExportResult plan_export_render(const PiecePlan *plan, RenderedPlan *rendered, PlanExportError *error) {
    switch (plan_render(plan, rendered)) {
    case PLAN_RENDER_OK:
        return PLAN_EXPORT_OK;
    case PLAN_RENDER_ERR_NULL:
        set_message(error, "plan is NULL");
        return PLAN_EXPORT_ERR_NULL;
    case PLAN_RENDER_ERR_NOT_REPRESENTABLE:
        set_message(error, "the plan has a meter or time that cannot be written");
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    case PLAN_RENDER_ERR_ALLOC:
        break;
    }
    set_message(error, "out of memory");
    return PLAN_EXPORT_ERR_ALLOC;
}
