#ifndef WORKSHOP_RATINGS_H
#define WORKSHOP_RATINGS_H

/*
 * Listening sessions and ratings (R-674): what was listened to and what Michelle thought of it, kept in the data folder so a setting can be kept as a
 * taste default with the ratings that say why. A session (ratings/sessions/<ses-id>.json, format ursatz-listening-session) holds its batches, the
 * ratings of their samples and the picks between batches; the ratings of the versions of an item are in ratings/items/<item id>.json (format
 * ursatz-item-ratings). Every change is one transaction. Ids come from workshop_new_id: ses, rat and pck.
 */

#include <stdbool.h>

#include <cJSON.h>

#include <common/status.h>

/* Opens a session; *out is its content (the caller's). what is motifs or themes; taste defaults to none; style and note may be NULL. */
UrsatzStatus workshop_ratings_session_create(const char *what, const char *taste, const char *style, const char *note, const char *actor, cJSON **out,
                                             UrsatzError *error);

/* The session's content; NOT_FOUND at /params/session when there is none. */
UrsatzStatus workshop_ratings_session_read(const char *id, cJSON **out, UrsatzError *error);

/* The sessions as [{id, created_at, what, taste, batches, ratings, picks}], newest first. */
UrsatzStatus workshop_ratings_sessions(cJSON **out, UrsatzError *error);

/*
 * Records a batch (round 1 to 10000, a name of 1 to 8 letters and digits, up to 64 samples each {index, seed, status, files, settings}); a batch
 * recorded already is refused ("batch B of round 1 is recorded"). *out is the session after the change.
 */
UrsatzStatus workshop_ratings_batch(const char *session, int round, const char *batch, const cJSON *samples, cJSON **out,
                                    UrsatzError *error);

/* Rates a sample (recorded with status ok) of a batch from 1 to 5; a new rating of it replaces the earlier one. *out_id is the rating's id (free it). */
UrsatzStatus workshop_ratings_rate(const char *session, int round, const char *batch, long long seed, int rating, const char *note, const char *actor,
                                   char **out_id, UrsatzError *error);

/* Records which batch of a round sounded better at a seed: the name of a batch recorded for the round, or "same". *out_id is the pick's id. */
UrsatzStatus workshop_ratings_pick(const char *session, int round, long long seed, const char *better, const char *note, const char *actor, char **out_id,
                                   UrsatzError *error);

/* Rates version of an item from 1 to 5. *out_id is the rating's id. NOT_FOUND for an item or a version that is not there. */
UrsatzStatus workshop_ratings_rate_version(const char *item_id, int version, int rating, const char *note, const char *actor, char **out_id,
                                           UrsatzError *error);

/* The ratings of an item as {item_id, ratings: [...]}; an item with none gives an empty list. */
UrsatzStatus workshop_ratings_item(const char *item_id, cJSON **out, UrsatzError *error);

/*
 * The critic's judgments of an item and her verdicts (R-723, ratings/critiques/<item id>.json, format ursatz-item-critiques): {item_id, critiques,
 * verdicts}; an item with none gives empty lists. Each write is one transaction whose target is the item, so the item's history lists it.
 */
UrsatzStatus workshop_ratings_critiques(const char *item_id, cJSON **out, UrsatzError *error);

/*
 * Records one candidate of a critique response ({score, evaluators, observations}) as a critique of version `version` of the item, minting
 * the ids of the critique (crt-) and of each observation (obs-). *out is the record written, the caller's.
 */
UrsatzStatus workshop_ratings_critique_add(const char *item_id, int version, const cJSON *candidate, const char *note, const char *actor, cJSON **out,
                                           UrsatzError *error);

/* Records her verdict (agree or disagree) on an observation of the item's critiques; an observation id the file does not hold is refused (NOT_FOUND
 * at /params/observation). *out_id is the verdict's id (vrd-), to free. */
UrsatzStatus workshop_ratings_verdict(const char *item_id, const char *observation, const char *verdict, const char *note, const char *actor, char **out_id,
                                      UrsatzError *error);

/* Whether id is the id of a recorded rating or pick (of a session or of an item). */
bool workshop_ratings_id_recorded(const char *id);

#endif
