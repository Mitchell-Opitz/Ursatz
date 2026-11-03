#ifndef URSATZ_COMPOSER_COMPOSER_API_H
#define URSATZ_COMPOSER_COMPOSER_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The Composer API (R-219, R-267): everything an application asks of the Composer, as in-process C calls that take a
 * JSON text request and fill a ComposerResponse. No Composer type, no cJSON type and no file path (except style models
 * and corpus directories) crosses it. See docs/api.md.
 *
 * Versioning: a request may carry "api_version": <major>; a different major is refused. Each entry added raises the
 * minor version; 1.0 is the first stable API.
 */

#define COMPOSER_API_VERSION_MAJOR 0
#define COMPOSER_API_VERSION_MINOR 2

/* "<major>.<minor>". */
const char *composer_api_version(void);

typedef enum {
    COMPOSER_OK = 0,
    COMPOSER_ERR_REQUEST,        /* the request is malformed or asks for something invalid */
    COMPOSER_ERR_UNSATISFIABLE,  /* the request is valid but nothing satisfies it */
    COMPOSER_ERR_CANCELLED,      /* the job's cancelled callback returned true */
    COMPOSER_ERR_ALLOC,
    COMPOSER_ERR_INTERNAL
} ComposerStatus;

const char *composer_status_name(ComposerStatus status);

typedef struct {
    char message[512];
    char path[128]; /* JSON Pointer of the request member at fault, "" when none */
} ComposerError;

/*
 * Progress and cancel for the long-running entries. A NULL job, or a NULL member, means no reports and no cancel. The
 * callbacks are called from the calling thread only.
 */
typedef struct {
    void (*progress)(void *user, const char *stage, unsigned done, unsigned total);
    bool (*cancelled)(void *user);
    void *user;
} ComposerJob;

/*
 * Every output. An entry zeroes it first; on failure only error is set. The caller frees it with
 * composer_response_destroy. Unused members stay NULL (or 0).
 */
typedef struct {
    char *json;
    char *plan_json;
    uint8_t *midi;
    size_t midi_size;
    char *musicxml;
    ComposerError error;
} ComposerResponse;

/* Frees and zeroes the response. Safe on a zeroed response and when called twice. */
void composer_response_destroy(ComposerResponse *response);

/* json: {"composer_version", "api_version", "analyzer_version", "ursatz_commit", "plan_format_version"} */
ComposerStatus composer_api_versions(ComposerResponse *out);

/*
 * Renders a plan once and exports it. Request: {"plan": <plan object>, "midi": bool, "musicxml": bool, "tempo": <quarter
 * notes per minute, 20 to 400, a whole number or a string like "195/2", or null>, "plain_midi": bool}; each flag defaults
 * to false. midi/midi_size and musicxml are filled for the formats asked for. json: {"skipped": [{"span", "lane", "reason"}]},
 * the lane spans the render left out.
 */
ComposerStatus composer_api_export(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Analyzes a motive: its intervals, contour, rhythm, range, scale degrees, implied harmony, base harmony and tones.
 * Request: {"input": {"format": "auto"|"notes"|"degrees"|"json"|"musicxml"|"rhythm"|"contour", "form": "full"|
 * "rhythm"|"contour", "text": "<the motive, as text>"}, "meter": "4/4", "key": "D minor", "pickup": 1 or "1/2" (beats
 * of the motive's meter), "harmony": "I V6@2" (the motive's own chords; "" clears them), "features": ["a:0-2", ...],
 * "role": "melodic"|"accompaniment", "tones": ["3=passing", ...] (note, 1-based, and a role)}. Only "input" with its
 * "text" is required; "format" defaults to auto, "form" to full. MusicXML arrives as text; MIDI as bytes through
 * composer_api_motive_from_midi. json: the analysis document the "motive" command prints with --json.
 */
ComposerStatus composer_api_motive(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * As composer_api_motive for a motive that arrives as the bytes of a Standard MIDI File (type 0 or 1), read in memory.
 * The request has the same members without "input"; a file that declares a meter or a key conflicts with the request's
 * the way it does for the command line. A buffer that is not a MIDI file, or holds more than one voice, is
 * COMPOSER_ERR_UNSATISFIABLE.
 */
ComposerStatus composer_api_motive_from_midi(const uint8_t *bytes, size_t size, const char *request_json,
                                             const ComposerJob *job, ComposerResponse *out);

/*
 * As composer_api_motive, and renders the motive: the request adds "render_key" ("G minor": hear the motive in this key,
 * the same degrees; default its own), "tempo" (quarter notes per minute, 20 to 400, a whole number or a string like
 * "195/2"), "musicxml": true and "midi": true. musicxml and midi/midi_size are filled for the files asked for (a pickup
 * is an implicit first measure). json: the analysis document with the top-level "rendered" object
 * {"key", "notes": [{"midi_pitch", "offset", "duration"}]}, always present. A key the motive cannot be moved to, or a file
 * that cannot be made, is COMPOSER_ERR_UNSATISFIABLE with path /render_key, /midi or /musicxml.
 */
ComposerStatus composer_api_motive_render(const char *request_json, const ComposerJob *job, ComposerResponse *out);

#endif
