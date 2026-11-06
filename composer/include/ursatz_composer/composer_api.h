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
 * Versioning: a request may carry "api_version": <major>; a different major is refused. The major version changes on any
 * removed or changed member or entry; the minor version on an addition (an entry, a request member, a response member).
 * 1.0 is the first stable API; 1.1 adds the theme form, style weight and seed vocabularies.
 */

#define COMPOSER_API_VERSION_MAJOR 1
#define COMPOSER_API_VERSION_MINOR 7

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

/*
 * Plans a whole short piece from a form template (docs/cli-plan.md); nothing is composed. Request: {"form": a form name
 * (absent with "given": the ternary form of 24 bars, the material in section B), "key": "D" or "D minor", "mode":
 * "major"|"minor" (not with a mode in "key"), "meter": "4/4", "bars", "count", "modulate", "motive": {"input": {"format",
 * "text"}, "source": <a name to show>} (a motive request, read in the meter and key), "corpus": <a corpus directory> with
 * "like": <a passage file> or a "like_progression" given, "accompaniment", "texture", "seed" (0 to 4294967295; default
 * from the clock), "style" (a style model path), "style_weight", "style_without", "blend", "blend_weight", "blend_weights",
 * "given": [{"kind": "melody"|"bass" with "motive" as above, or "progression" with "numerals", or "like_progression" with
 * "numerals", "at": "B:3" (the section's letter and the bar), "cells", "source": <a name to show>}]}. json: what `plan
 * --format json` prints, without "file"; plan_json: the plan. "notices" lists stale corpus documents.
 */
ComposerStatus composer_api_plan(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Realizes a plan (docs/cli-fill.md). Request: {"plan": <plan object>, "seed", "check", "candidates" (1 to 32), "style",
 * "style_weight", "style_without", "blend", "blend_weight", "blend_weights" (as for composer_api_plan; they replace the
 * plan's own), "midi", "musicxml", "plain_midi", "tempo", "record": true (the text of the stage records in "record" of the
 * document), "explain" ("text" or "json": "explanation")}. json: what `fill --format json` prints, without "files".
 * plan_json, midi and musicxml are filled. When no seed completes the plan the status is COMPOSER_ERR_UNSATISFIABLE and
 * json is {"stuck": {"first", "last", "stage", "key_rejected", "open": [{"what", "reason"}], "more"}}. Progress per seed
 * ("seed") and per compose stage; a cancel is COMPOSER_ERR_CANCELLED.
 */
ComposerStatus composer_api_fill(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Locks a node of a plan, or one lane of it, as Given (docs/cli-fill.md). Request: {"plan", "node" (an id) or "section" (a
 * letter), "lane"}. json: {"locked": {"nodes", "spans"}, "what": "section B"}; plan_json: the plan.
 */
ComposerStatus composer_api_lock(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Writes one node of a plan again (docs/cli-regenerate.md). Request: {"plan", "node" or "section", "lane", "seed",
 * "midi", "musicxml", "plain_midi", "tempo", "record", "explain"}. json: what `regenerate --format json` prints, without
 * "files": as fill's, with "reopened": {"sections", "spans"}. plan_json, midi and musicxml are filled. A refused target is
 * COMPOSER_ERR_UNSATISFIABLE.
 */
ComposerStatus composer_api_regenerate(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Several distinct versions of one node of a plan (docs/cli-variations.md). Request: {"plan", "node" or "section", "lane",
 * "count" (1 to 16), "seed", "musicxml", "plain_midi", "tempo"}. json: {"variations": [{"index", "seed", "plan": <the plan
 * object>, "musicxml" (when asked), "sections": [...]}], "kept", "requested", "seeds_tried"}; the MIDI file of a
 * variation is composer_api_export of its plan. COMPOSER_ERR_UNSATISFIABLE when none differs from the plan.
 */
ComposerStatus composer_api_variations(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * As composer_api_variations for the harmony of a section whose melody is given (docs/cli-variations.md, harmonize).
 * Request: {"plan", "section", "count", "seed", "musicxml", "plain_midi", "tempo"}.
 */
ComposerStatus composer_api_harmonize(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Writes a chord progression for one phrase, with a bass line, ending in the cadence asked for (docs/cli-progression.md).
 * Request: {"key": "D minor", "meter": "4/4" (2/4, 3/4, 4/4 or 6/8), "bars": 1 to 64, "cadence": "PAC"|"IAC"|"HC"|"DC"|"PC"
 * (all four required), "seed" (0 to 4294967295; default from the clock; if no bass line suits the progression the next
 * seeds are tried, up to 16, and the one that worked is the "seed" of the document), "embellish" ("none", "low", "medium"
 * or "high"), "bass" (default true; false writes only the chords), "schema" and "sequence" (names: the start of the phrase
 * on a galant schema, a continuation on a harmonic sequence), "suggest" (1 to 8: that many alternatives instead of one),
 * "plans" (each progression's plan object in the document), "midi", "musicxml" (the first progression's files, which need
 * the bass), "tempo", "plain_midi"}. {"list_schemas": true} alone lists the names: {"schemas": [{"name", "function",
 * "modes": ["major", "minor"]}], "sequences": [name]}. json: {"key", "meter", "bars", "cadence", "seed", "heard",
 * "chords": [{"bar", "beat", "chord", "function", "bass"}]} (with "suggest": "alternatives": [{"heard", "chords"}] in
 * place of the last two; each entry adds "plan" with "plans" and "musicxml" with "musicxml"), and "notices" (strings) when
 * fewer alternatives than asked for were found. plan_json, midi and musicxml: the first progression's. Progress per seed
 * tried ("progression") and per fill stage; a cancel is COMPOSER_ERR_CANCELLED. No seed that works is
 * COMPOSER_ERR_UNSATISFIABLE, as is a schema that cannot be written in the key or a phrase too short for the cadence.
 */
ComposerStatus composer_api_progression(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Writes a counter-melody against a line in the first, second, third or fourth species of strict counterpoint
 * (docs/cli-counterpoint.md). Request: the motive request of composer_api_motive for the line ("input": {"format", "form",
 * "text"}, "pickup", "harmony"...; any length) with "species" (1 to 4), "key" (a tonic alone with "mode": "major"|"minor"),
 * "meter" (all three required), "position" ("below": the line is the melody, the default; "above": the line is the bass),
 * "seed" (0 to 4294967295; default from the clock), "suggest" (1 to 16 different counter-melodies), "plans", "midi",
 * "musicxml" (the first counter-melody's files), "tempo", "plain_midi". json: {"species", "position", "key", "meter",
 * "results": [{"seed", "line": [{"midi_pitch", "offset": {"num", "den"}, "duration": {"num", "den"}}], "counter_melody":
 * [...], "groups": [{"name", "hard_held", "soft_held"}]}]} (each result adds "plan" with "plans" and "musicxml" with
 * "musicxml"), and "notices" when fewer than "suggest" were found. A line the species cannot be written against, or no
 * counter-melody that holds the rules from the seeds tried (16 for each one asked for), is COMPOSER_ERR_UNSATISFIABLE.
 * Progress per seed ("counterpoint"); a cancel is COMPOSER_ERR_CANCELLED.
 */
ComposerStatus composer_api_counterpoint(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Makes motives related to a motive, best first (docs/cli-develop.md). Request: the motive request of composer_api_motive
 * ("input", "meter", "key", ...) with "op" (required: "restate", "invert", "retrograde", "interval", "fragment",
 * "liquidate", "augment" or "diminish"), "seed" (0 to 4294967295, default 0), "suggest" (1 to 16, default 5) and the members
 * the operation takes, each as the option of the command: "by", "degree", "tonal" (restate), "axis", "tonal" (invert),
 * "interval", "amount", "keep_contour" (interval), "position", "length", "min" (fragment), "drop", "keep_rhythm", "min"
 * (liquidate), "ratio" (augment, diminish); a member the operation does not take is refused. "midi", "musicxml", "tempo",
 * "plain_midi": the files of the source and the best candidate (midi, musicxml, plan_json). json: {"source": <note list>,
 * "operation", "seed", "candidates": [{"index", "label", "score_numerator", "score_denominator", "text"}]}. Nothing that
 * can be developed as asked is COMPOSER_ERR_UNSATISFIABLE. Progress once ("develop"); a cancel is COMPOSER_ERR_CANCELLED.
 */
ComposerStatus composer_api_develop(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * Grows a theme from a motive (docs/cli-grow.md): its structure, a harmony, a bass line and a melody, ending in the cadence
 * asked for. Request: the motive request of composer_api_motive ("input", "harmony", "features", "role"; or, instead of
 * "input", "corpus": a corpus directory with "like": a passage file, whose first bar of a similar passage becomes the motive,
 * which the document reports as "source") and the brief of the command: "into" ("sentence", "period" or "hybrid"), "hybrid"
 * ("<beginning>+<ending>"), "key", "mode" ("major", "minor" or "style"), "meter", "bars" (4 or 8), "cadence", "first_cadence",
 * "accompaniment", "accompaniment_motive" (a motive request of its own, read in the meter, role accompaniment), "texture",
 * "voicing" (true), "check", "style", "style_weight", "style_without" (up to 8 categories), "blend", "blend_weight",
 * "blend_weights", "seed" (0 to 4294967295; default from the clock; if a seed gets stuck the next, up to 16, are tried),
 * "suggest" (1 to 16 different themes), "tempo", and what to make besides the document: "midi", "musicxml" (the first theme's
 * files, and each theme's MusicXML as "musicxml" in its entry), "plans" (each theme's plan in the document), "explain"
 * ("text" or "json": each theme's explanation as "explanation"), "plain_midi". "into", "key" and "meter" are required.
 * json: what `grow --format json` prints (without the files it wrote), and "notices" (strings) when a corpus document is
 * stale or fewer themes than asked for were found. plan_json, midi and musicxml: the first theme's. Progress per compose stage
 * and per seed tried ("grow"); a cancel is COMPOSER_ERR_CANCELLED with no partial plan.
 */
ComposerStatus composer_api_grow(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/*
 * The words an application offers and the requests accept, in the order an application shows them (R-270: the Composer's
 * lists win). json: {"meters": [{"value", "label", "beats", "chord_onset_step", "second_chord_beat" (only for a meter whose
 * bar takes a second chord)}], "modes", "piece_modes" (modes and "style"), "keys" (the 24 keys spelled by the Composer),
 * "pitch_class_names": [{"value": 0 to 11, "major", "minor"}], "cadences" and "theme_cadences", "pickups" ({"<meter>":
 * ["0", "1/2", ...]} in beats), "motive_forms", "roles", "functions", "distances", "theme_slots", "theme_ideas" (each with
 * its "slot"), "theme_motive_categories", "theme_pools", "accompaniment_patterns", "piece_parts", "piece_intos",
 * "fill_kinds", "tone_labels" (each with an "abbreviation"), "variant_operations" (what composer_api_variant_operations
 * gives)}. Every list but those noted is [{"value", "label"}]; a request accepts every value.
 */
ComposerStatus composer_api_vocabulary(ComposerResponse *out);

/*
 * Reads a request as its entry does and composes nothing. kind: "motive" (the request of composer_api_motive), "theme" (of
 * composer_api_theme), "piece" (the piece request object, docs/piece-request.md, not the wrapper of composer_api_piece) or
 * "random_motive" (of composer_api_random_motive). json: {"request": <the request with the Composer's defaults filled>}: a
 * motive gets its "meter", "role", "input" "format" and "form" when it has none and its "harmony" as [{"numeral", "onset"}]
 * (the onset in beats of the meter, as "3/2"); a piece gets the "cadence" of every section that has none and the default
 * ending section; a random motive gets its "seed" and the "mode" and "meter" a "random" drew; a theme comes back as it is
 * after its plan is built without seeding: a theme request is refused as Compose would refuse it before seeding, with the
 * path of the field (COMPOSER_ERR_UNSATISFIABLE), one refusal at a time.
 * A request that an entry would refuse is refused the same way, with its message and path. An unknown kind is
 * COMPOSER_ERR_REQUEST with path /kind.
 */
ComposerStatus composer_api_validate(const char *kind, const char *request_json, ComposerResponse *out);

#endif
