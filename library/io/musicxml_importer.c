#include "io/musicxml_importer.h"
#include <common/files.h>
#include <common/hash.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezxml.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"
#include "model/time/tuplet.h"
#include "model/events/dynamics.h"
#include "model/events/tempo_marking.h"
#include "model/time/metronome_marking.h"
#include "model/time/time_signature.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "io/musicxml_import_internal.h"

/* --- warnings and errors --- */

void musicxml_import_warn(Ctx *ctx, const char *format, ...) {
    MusicXmlImportWarnings *warnings = &ctx->out->warnings;
    warnings->count++;
    if (warnings->message_count >= MUSICXML_IMPORT_MAX_WARNING_MESSAGES) {
        return;
    }
    char text[MUSICXML_IMPORT_WARNING_MESSAGE_SIZE - 48]; /* room for the "part ..., measure ...: " prefix */
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    snprintf(warnings->messages[warnings->message_count++], MUSICXML_IMPORT_WARNING_MESSAGE_SIZE, "part %s, measure %s: %s",
             ctx->part_id != NULL ? ctx->part_id : "?", ctx->measure_number != NULL ? ctx->measure_number : "?", text);
}

MusicXmlImportStatus musicxml_import_fail(Ctx *ctx, MusicXmlImportStatus status, const char *format, ...) {
    if (ctx->status != MUSICXML_IMPORT_OK) {
        return ctx->status; /* the first error wins */
    }
    ctx->status = status;
    char text[MUSICXML_IMPORT_ERROR_MESSAGE_SIZE - 64];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    snprintf(ctx->out->error_message, MUSICXML_IMPORT_ERROR_MESSAGE_SIZE, "part %s, measure %s: %s",
             ctx->part_id != NULL ? ctx->part_id : "?", ctx->measure_number != NULL ? ctx->measure_number : "?", text);
    return status;
}

/* --- reading text --- */

/* The trimmed text of an element into buffer; false when it has none. */
bool musicxml_import_read_text(ezxml_t element, char *buffer, size_t size) {
    if (element == NULL) {
        return false;
    }
    const char *text = ezxml_txt(element);
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        length--;
    }
    if (length == 0 || length >= size) {
        return false;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return true;
}

/* A whole number in an element's text; false when absent or not a number. */
bool musicxml_import_read_long(ezxml_t element, long *out) {
    char text[32];
    if (!musicxml_import_read_text(element, text, sizeof(text))) {
        return false;
    }
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

/* --- notes --- */

bool musicxml_import_add_note(Ctx *ctx, const XNote *note, size_t *out_index) {
    if (ctx->note_count == ctx->note_capacity) {
        size_t capacity = ctx->note_capacity == 0 ? 256 : ctx->note_capacity * 2;
        XNote *grown = (XNote *)realloc(ctx->notes, capacity * sizeof(XNote));
        if (grown == NULL) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            return false;
        }
        ctx->notes = grown;
        ctx->note_capacity = capacity;
    }
    ctx->notes[ctx->note_count] = *note;
    *out_index = ctx->note_count++;
    return true;
}

/* --- the document --- */

static MusicXmlImportStatus import_document(Ctx *ctx, ezxml_t root) {
    const char *root_name = ezxml_name(root);
    if (root_name == NULL || strcmp(root_name, "score-partwise") != 0) {
        if (root_name != NULL && strcmp(root_name, "score-timewise") == 0) {
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_WRONG_ROOT, "score-timewise is not supported, only score-partwise");
        }
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_WRONG_ROOT, "the root element is <%s>, not <score-partwise>",
                    root_name != NULL ? root_name : "");
    }

    size_t part_count = 0;
    char **part_ids = NULL;
    for (ezxml_t part = ezxml_child(root, "part"); part != NULL; part = part->next) {
        part_count++;
    }
    part_ids = (char **)calloc(part_count > 0 ? part_count : 1, sizeof(char *));
    ctx->part_staves = (int *)calloc(part_count > 0 ? part_count : 1, sizeof(int));
    ctx->part_end = (Rational *)calloc(part_count > 0 ? part_count : 1, sizeof(Rational));
    if (part_ids == NULL || ctx->part_staves == NULL || ctx->part_end == NULL) {
        free(part_ids);
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
    }
    size_t index = 0;
    for (ezxml_t part = ezxml_child(root, "part"); part != NULL && ctx->status == MUSICXML_IMPORT_OK;
         part = part->next, index++) {
        const char *id = ezxml_attr(part, "id");
        char fallback[32];
        snprintf(fallback, sizeof(fallback), "part-%zu", index);
        part_ids[index] = strdup(id != NULL && id[0] != '\0' ? id : fallback);
        if (part_ids[index] == NULL) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            break;
        }
        ctx->part_id = part_ids[index];
        ctx->measure_number = NULL;
        musicxml_import_process_part(ctx, part, index);
    }
    ctx->part_id = NULL;

    MusicXmlImport *out = ctx->out;
    if (ctx->status == MUSICXML_IMPORT_OK) {
        out->parts = (MusicXmlPart *)calloc(part_count > 0 ? part_count : 1, sizeof(MusicXmlPart));
        if (out->parts == NULL) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        }
        for (size_t i = 0; i < part_count && ctx->status == MUSICXML_IMPORT_OK; i++) {
            const char *name = "";
            ezxml_t list = ezxml_child(root, "part-list");
            for (ezxml_t listed = list != NULL ? ezxml_child(list, "score-part") : NULL; listed != NULL;
                 listed = listed->next) {
                const char *listed_id = ezxml_attr(listed, "id");
                if (listed_id != NULL && strcmp(listed_id, part_ids[i]) == 0) {
                    name = ezxml_txt(ezxml_child(listed, "part-name"));
                    break;
                }
            }
            out->parts[i].id = strdup(part_ids[i]);
            out->parts[i].name = strdup(name);
            out->parts[i].staff_count = ctx->part_staves[i] > 0 ? ctx->part_staves[i] : 1;
            if (out->parts[i].id == NULL || out->parts[i].name == NULL) {
                musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            }
            out->part_count = i + 1;
        }
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        musicxml_import_build_meter_map(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        musicxml_import_build_key_signatures(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        musicxml_import_build_tempo_map(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        musicxml_import_build_tempo_markings(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        musicxml_import_build_dynamics(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK && ctx->unclassified_words > 0) {
        musicxml_import_warn(ctx, "%zu direction text(s) not classified as tempo text (no <metronome> or <sound tempo> with them)",
             ctx->unclassified_words);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        size_t count = ctx->note_count;
        SortItem *order = (SortItem *)calloc(count > 0 ? count : 1, sizeof(SortItem));
        out->events = (NoteEvent **)calloc(count > 0 ? count : 1, sizeof(NoteEvent *));
        out->pitch_values = (PitchNumericValue **)calloc(count > 0 ? count : 1, sizeof(PitchNumericValue *));
        out->part_index = (size_t *)calloc(count > 0 ? count : 1, sizeof(size_t));
        out->staff = (int *)calloc(count > 0 ? count : 1, sizeof(int));
        EntityID *tuning_id = NULL;
        TuningSystem *tuning = NULL;
        if (order == NULL || out->events == NULL || out->pitch_values == NULL || out->part_index == NULL ||
            out->staff == NULL || entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        } else if (tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
            entity_id_destroy(tuning_id);
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        }
        if (ctx->status == MUSICXML_IMPORT_OK) {
            for (size_t i = 0; i < count; i++) {
                order[i].onset = ctx->notes[i].onset;
                order[i].part = ctx->notes[i].part;
                order[i].order = ctx->notes[i].order;
                order[i].index = i;
            }
            qsort(order, count, sizeof(SortItem), musicxml_import_compare_items);
            IdSet ids = {NULL, 16, out->events};
            while (ids.capacity < 2 * count + 2) {
                ids.capacity *= 2;
            }
            ids.slots = (size_t *)calloc(ids.capacity, sizeof(size_t));
            if (ids.slots == NULL) {
                musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            }
            for (size_t i = 0; i < count && ctx->status == MUSICXML_IMPORT_OK; i++) {
                const XNote *note = &ctx->notes[order[i].index];
                char id_text[160];
                musicxml_import_choose_event_id(ctx, &ids, note, part_ids[note->part], id_text, sizeof(id_text));
                if (musicxml_import_build_event(ctx, note, id_text, part_ids[note->part], tuning, &out->events[i], &out->pitch_values[i])) {
                    out->part_index[i] = note->part;
                    out->staff[i] = note->staff;
                    out->count = i + 1;
                    musicxml_import_id_set_add(&ids, i);
                }
            }
            free(ids.slots);
        }
        tuning_system_destroy(tuning);
        free(order);
    }
    for (size_t i = 0; i < part_count; i++) {
        free(part_ids[i]);
    }
    free(part_ids);
    free(ctx->part_staves);
    free(ctx->part_end);
    ctx->part_staves = NULL;
    ctx->part_end = NULL;
    return ctx->status;
}

MusicXmlImportStatus musicxml_import_buffer(const char *data, size_t size, MusicXmlImport *out) {
    if (out == NULL) {
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    musicxml_import_clear_import(out);
    if (data == NULL) {
        snprintf(out->error_message, sizeof(out->error_message), "no document");
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    if (size > MUSICXML_IMPORT_MAX_BYTES) {
        snprintf(out->error_message, sizeof(out->error_message), "the document is %zu bytes, over the %u byte limit", size,
                 (unsigned)MUSICXML_IMPORT_MAX_BYTES);
        return MUSICXML_IMPORT_ERR_TOO_LARGE;
    }

    /* the parser edits its buffer in place: give it a copy, without a UTF-8 byte order mark or trailing space */
    const char *start = data;
    size_t length = size;
    if (length >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
        length -= 3;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        length--;
    }
    bool utf16 = length >= 2 && (((unsigned char)start[0] == 0xFF && (unsigned char)start[1] == 0xFE) ||
                                 ((unsigned char)start[0] == 0xFE && (unsigned char)start[1] == 0xFF));
    if (length == 0 || (!utf16 && memchr(start, '\0', length) != NULL)) {
        snprintf(out->error_message, sizeof(out->error_message), "malformed XML: %s",
                 length == 0 ? "the document is empty" : "the document contains a NUL byte");
        return MUSICXML_IMPORT_ERR_MALFORMED_XML;
    }
    char *copy = (char *)malloc(length + 2);
    if (copy == NULL) {
        snprintf(out->error_message, sizeof(out->error_message), "out of memory");
        return MUSICXML_IMPORT_ERR_ALLOC;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    copy[length + 1] = '\0';

    ezxml_t root = ezxml_parse_str(copy, length);
    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.status = MUSICXML_IMPORT_OK;
    MusicXmlImportStatus status = MUSICXML_IMPORT_OK;
    if (root == NULL) {
        status = musicxml_import_fail(&ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
    } else if (ezxml_error(root)[0] != '\0') {
        snprintf(out->error_message, sizeof(out->error_message), "malformed XML: %s", ezxml_error(root));
        ctx.status = status = MUSICXML_IMPORT_ERR_MALFORMED_XML;
    } else {
        status = import_document(&ctx, root);
    }
    ezxml_free(root);
    free(copy);
    free(ctx.notes);
    free(ctx.ties);
    free(ctx.times);
    free(ctx.keys);
    free(ctx.tempos);
    free(ctx.markings);
    free(ctx.dynamics);
    if (status != MUSICXML_IMPORT_OK) {
        char message[MUSICXML_IMPORT_ERROR_MESSAGE_SIZE];
        snprintf(message, sizeof(message), "%s", out->error_message);
        musicxml_import_destroy(out);
        snprintf(out->error_message, sizeof(out->error_message), "%s", message);
    }
    return status;
}

MusicXmlImportStatus musicxml_import_file(const char *path, MusicXmlImport *out) {
    if (out == NULL) {
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    musicxml_import_clear_import(out);
    char *buffer = NULL;
    size_t size = 0;
    UrsatzStatus read = path != NULL ? common_file_read(path, &buffer, &size, NULL) : URSATZ_ERR_NOT_FOUND;
    if (read == URSATZ_ERR_ALLOC) {
        snprintf(out->error_message, sizeof(out->error_message), "out of memory");
        return MUSICXML_IMPORT_ERR_ALLOC;
    }
    if (read != URSATZ_OK) {
        snprintf(out->error_message, sizeof(out->error_message), "cannot open \"%s\"", path != NULL ? path : "(null)");
        return MUSICXML_IMPORT_ERR_IO;
    }
    if (size > MUSICXML_IMPORT_MAX_BYTES) {
        free(buffer);
        snprintf(out->error_message, sizeof(out->error_message), "\"%s\" is %ld bytes, over the %u byte limit", path, (long)size,
                 (unsigned)MUSICXML_IMPORT_MAX_BYTES);
        return MUSICXML_IMPORT_ERR_TOO_LARGE;
    }
    MusicXmlImportStatus status = musicxml_import_buffer(buffer, size, out);
    free(buffer);
    return status;
}
