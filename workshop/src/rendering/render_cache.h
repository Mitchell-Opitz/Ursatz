#ifndef WORKSHOP_RENDER_CACHE_H
#define WORKSHOP_RENDER_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The render of a motif (R-275): its degrees through the Composer into MusicXML and MIDI, and the cache of the render of a stored
 * version. cache/renders/<item>/v<n>/ holds score.musicxml and motif.mid, and render.json {api_version, composer_version, composition_key,
 * render_key, tempo} names what they were made with; a version never changes, so the files stand for the same key, tempo and Composer
 * build. On a failure every call returns its status, *http_status is the status to answer and error holds the message (and the request
 * member a Composer refusal names as its path).
 */

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?, harmony?, features?}: nothing is saved.
 * The answer {"ok": true, "musicxml", "playback"} in *answer. */
UrsatzStatus workshop_motif_render(const cJSON *body, cJSON **answer, int *http_status, UrsatzError *error);

/* The harmony lab's Hear (R-769): `request` is a progression request (R-766); it is written with its MIDI and MusicXML and nothing is saved. The answer is the
 * Composer's document with "ok", "musicxml" and the "playback" built from the MIDI, in *answer. A Composer refusal keeps its path. */
UrsatzStatus workshop_progression_preview(const cJSON *request, cJSON **answer, int *http_status, UrsatzError *error);

/* The score or the playback of version number of item id, rendered with render_key (NULL: the motif's own key). With playback_only
 * the playback is built and *playback set; else the MusicXML text is in *musicxml (caller frees). file (not NULL, with playback_only)
 * asks for the playback of a theme or piece version's own MIDI file ("theme" or a version name) instead. */
UrsatzStatus workshop_motif_version_render(const char *id, int number, const char *render_key, bool playback_only, const char *file,
                                           char **musicxml, cJSON **playback, int *http_status, UrsatzError *error);

#endif /* WORKSHOP_RENDER_CACHE_H */
