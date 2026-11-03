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
#define COMPOSER_API_VERSION_MINOR 4

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
 * to false. "loop": {"start": "2/1", "end": "14/1"} (times in whole notes from the start of the plan) puts the loopStart and
 * loopEnd markers into the MIDI file. midi/midi_size and musicxml are filled for the formats asked for. json: {"skipped":
 * [{"span", "lane", "reason"}]}, the lane spans the render left out.
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

/*
 * The variant operations of the registry: name, label, category, citation, parameters and the forms each applies to. json:
 * what `variants --list-ops --json` prints.
 */
ComposerStatus composer_api_variant_operations(ComposerResponse *out);

/*
 * Variants of a motive: each applies a chain of up to 4 registry operations, and a parameter the chain leaves open is
 * drawn from the seed. Request: the motive request of composer_api_motive (input, meter, key, pickup, harmony, features,
 * role) plus "basic" (a motive request with its own input, pickup and harmony, read in the source's meter, key, form and
 * role: the basic motive remoteness is measured from; the source when absent), "functions" (up to 4 of "beginning",
 * "continuation", "contrast", "cadential": keep the variants that suit any), "distance" ("close", "medium" or "far"),
 * "chain" ({"steps": [{"op", "params"}]}: the same chain for every variant), "each_op" (one variant for every operation
 * that applies to the motive's form; not with chain, count, distance or functions), "count" (1 to 16, default 6) and
 * "seed" (0 to 4294967231; default from the clock; variant i uses seed + i). Progress is reported once for each variant
 * tried, stage "variants", and a cancel is honored there. json: {"seed", "source", "variants": [...]}, with "skipped" for
 * each_op and "notices" (strings) when a draw was skipped or fewer variants than asked for were found. "detail": true adds
 * to each variant "remoteness_fraction": [numerator, denominator] and "leaves_harmony".
 */
ComposerStatus composer_api_variants(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Names the chain of registry operations that turns a source motive into a target. Request: {"source": <motive request:
 * input, harmony, features>, "target": <the same>, "meter", "key", "pickup" (the source's), "target_pickup", "role", "basic"
 * (as for variants), "depth" (1 to 4, default 4)}; both motives are read in the meter, key and role of the request, and
 * the form of the source's input. json: {"found", "chain", "distance", "remoteness", "kept", "changed", "derivation"};
 * "detail": true adds "distance_fraction" and "remoteness_fraction", each [numerator, denominator].
 */
ComposerStatus composer_api_recognize(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Random motives that keep the hard melodic rules of standard counterpoint; their length, pickup, number of notes, rhythm
 * and range are drawn from defaults in the Composer. Request: {"mode": "major"|"minor", "meter": "4/4", "key": "D minor"
 * (its mode must be "mode"; default C major or A minor), "form": "full"|"rhythm"|"contour", "role": "melodic"|
 * "accompaniment", "style": <a style model file path>, "style_without": [<style categories read without>, at most 8;
 * needs "style"], "count": 1 to 16 (default 1), "seed": 0 to 4294967295 (default from the clock)}. "mode" and "meter" are
 * required; either may be "random", drawn from the seed by the rule the GUI used (R-269): with m = ((seed + 1) * 2654435761)
 * mod 2^32, the mode is major when (m >> 8) is even and minor when it is odd, and the meter is "4/4", "3/4", "2/4" or
 * "6/8" by (m >> 16) mod 4. The same request and seed give the same motives. json: {"seed", "motives": [<the motive entry's document>]},
 * and "drawn": {"mode", "meter"} (only the members that were drawn) when something was.
 * A style that cannot be read is COMPOSER_ERR_UNSATISFIABLE with path /style.
 */
ComposerStatus composer_api_random_motive(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Grows a theme from the theme request (docs/theme-request.md, schema "ursatz-theme-request" version 2) with its motives
 * inline ("degrees" text; a motive with a "path" or "file" is refused with its path) and the options the command takes:
 * "seed", "suggest" (1 to 16 themes), "check", "style" (a style model path), "blend", "blend_weight", "blend_weights",
 * "tempo", "plain_midi", and what to make besides the document: "midi", "musicxml" (the first theme's files), "plans"
 * (each theme's plan in the document), "explain" ("text" or "json": each theme's explanation in the document).
 * json: what `theme --format json` prints (without the files it wrote). plan_json: the first theme's plan. midi and
 * musicxml: the first theme's files, when asked. Progress per compose stage and per seed tried ("grow"); a cancel is
 * COMPOSER_ERR_CANCELLED with no partial plan.
 */
ComposerStatus composer_api_theme(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Composes a piece (docs/piece-request.md): the loop version and the ending version built from it, or the loop plan of an
 * earlier piece with one section written again. Request: {"request": <the piece request: schema "ursatz-piece-request"
 * version 1, a theme fill carrying "plan": <plan object> and, for the display, "source": <a name> instead of a path>,
 * "from": <the loop plan object of an earlier piece, or absent>, "regenerate": {"section": <section id>|null, "lanes":
 * ["harmony"|"bass"|"melody"|"inner", ...] (none or "all": the whole section)} (null section: only the ending version is
 * built again; needs "from"), "seed" (0 to 4294967295; default from the clock), "check", "style" (a style model path that
 * replaces the request's), "midi", "musicxml" (the loop version's files), "ending_musicxml" (the ending version's MusicXML as
 * "ending_musicxml" in the document; the ending version's MIDI file is composer_api_export of "plans"."ending"), "plain_midi", "explain" ("text" or "json": the
 * explanation of both versions in the document)}. json: what `piece --format json` prints (without the files it wrote), with
 * "plans": {"loop": <plan object>, "ending": <plan object>}, the "plan" of each version null, and "explanation" when asked; a
 * section a regenerate left as it was has "in_scope": false. plan_json: the loop plan. midi and musicxml: the loop version's
 * files, when asked (the MIDI file has the loopStart and loopEnd markers). Progress per seed ("seed") and per compose stage; a
 * cancel is COMPOSER_ERR_CANCELLED with no partial plan. A theme fill with a path in "plan" is refused with its path.
 */
ComposerStatus composer_api_piece(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Variations of one section of a loop plan: the section is written again from one seed after another (as a regenerate does,
 * the seam and the loop's end included) and a version is kept when it sounds different from the plan and from the ones kept
 * before. Request: {"request": <the piece request, as for composer_api_piece>, "from": <the loop plan object>, "variations":
 * {"section": <section id>, "lanes": [...] (as "regenerate" of composer_api_piece), "count": 1 to 8}, "seed", "check",
 * "plain_midi", "style", "musicxml" (each variation's MusicXML as "musicxml" in its entry; its MIDI file is
 * composer_api_export of its plan with its "loop_markers" as "loop")}. json: {"schema": "ursatz-piece-variations", "section", "lane", "brief", "variations": [{"index",
 * "seed", "loop": <the version, its "plan" the plan object>, "loop_markers": {"start", "end"}}], "kept", "requested",
 * "seeds_tried"};
 * "loop_markers" is the region a MIDI file of the loop version marks with loopStart and loopEnd (composer_api_export's
 * "loop"). Progress once per seed tried, stage "variations" and the compose stages; a cancel is COMPOSER_ERR_CANCELLED.
 * COMPOSER_ERR_UNSATISFIABLE when no version differs from the plan.
 */
ComposerStatus composer_api_piece_variations(const char *request_json, const ComposerJob *job, ComposerResponse *out);

#endif
