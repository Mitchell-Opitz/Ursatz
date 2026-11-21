# Listening sessions and ratings

What was listened to and what was thought of it, kept in the data folder so a setting can become the taste default together with the ratings that say why (R-674). Back to the [docs index](README.md).

Code: `workshop/src/ratings/ratings.c`, the queries in `coordinator/api_ratings.c`, the commands in `coordinator/api_commands_ratings.c`. Not here yet: observations, verdicts and kept rejected candidates (P68).

## The session file

`ratings/sessions/<ses-id>.json` (format `ursatz-listening-session` 1): `id`, `created_at`, `actor`, `what` (`motifs` or `themes`), `taste` (the profile it listens with, `none` by default), `style` (or null), `note`, and three lists:

- `batches`: `{round, batch, at, samples: [{index, seed, status, files: [{name, sha256}], settings}]}`. A batch is a set of samples made with one change of settings, named with 1 to 8 letters and digits; `files` are the file's base name and the sha256 of its bytes; `settings` is the resolution the sample was made with.
- `ratings`: `{id, round, batch, seed, rating, note, actor, at}`, a rating from 1 to 5 of one sample.
- `picks`: `{id, round, seed, better, note, actor, at}`: the batch of the round that sounded better at a seed, or `same`.

The ratings of the versions of an item are in `ratings/items/<item id>.json` (format `ursatz-item-ratings` 1): `{item_id, ratings: [{id, version, rating, note, actor, at}]}`. Ids come from the workshop's id service: `ses-`, `rat-` and `pck-`.

## Commands

All are journaled with their actor; each changes one file in one transaction.

```
ratings.session what motifs taste none
ratings.batch session ses-1 round 1 batch A samples [...]
ratings.rate session ses-1 round 1 batch A seed 1 rating 4
ratings.pick session ses-1 round 1 seed 1 better B
ratings.rate-version item:thm-1@v2 version 2 rating 5
ratings.keep taste none key motif.features.dotted value 1 ratings [pck-1] note "B won"
```

`ratings.session` answers `{session}`. `ratings.batch` (`samples`: up to 64 objects, each with `index`, `seed`, `status`, `files` and `settings`) refuses a batch already recorded ("batch B of round 1 is recorded"). `ratings.rate` needs the sample recorded in that batch with status `ok` (a seed not there, or a stuck one, is refused at `/params/seed`); a new rating of a sample replaces the earlier one; the answer is `{id}`. `ratings.pick` needs `better` to name a batch recorded for the round, or `same`. `ratings.rate-version` (target an item, `version` from 1 to its current version) answers `{id}`.

## Keep as default

`ratings.keep` is how a value gets into a taste profile (`settings.set` refuses a taste scope): it cites at least one recorded rating or pick id ("keep as default cites the ratings behind it"; an id that was never recorded: "<id> is not a recorded rating or pick" at `/params/ratings/<i>`), checks the value as `settings.set` does, and sets or drops (a null or absent `value`) the key in `settings/taste/<taste>.json`, made when missing. It appends `{key, value, ratings, note, actor, at}` to the profile's `kept` list, so every default says which listening produced it ([settings](settings.md)).

## Queries

`ratings.session` (param `session`) answers the session; `ratings.sessions` answers `{sessions: [{id, created_at, what, taste, batches, ratings, picks}]}` newest first; `ratings.item` (param `item_id`) answers `{item_id, ratings}`. After a committed `ratings.` command the event `ratings-changed` is published ([jobs and events](jobs-and-events.md#events)).
