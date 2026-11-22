# Listening sessions and ratings

What was listened to and what was thought of it, kept in the data folder so a setting can become the taste default together with the ratings that say why (R-674). Back to the [docs index](README.md).

Code: `workshop/src/ratings/ratings.c` and `critiques.c`, the queries in `coordinator/api_ratings.c`, the commands in `coordinator/api_commands_ratings.c`.

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

`ratings.session` answers `{session}`. `ratings.batch` (`samples`: up to 64 objects, each with `index`, `seed`, `status`, `files` and `settings`) refuses a batch already recorded ("batch B of round 1 is recorded"). `ratings.rate` needs the sample recorded in that batch with status `ok` (a seed not there, or a stuck one, is refused at `/params/seed`); a new rating of a sample replaces the earlier one; the answer is `{id}`. `ratings.pick` needs `better` to name a batch recorded for the round, or `same`. `ratings.rate-version` (target an item, `version` from 1 to its current version) takes `base_version` and answers `{id}`; `ursatz rate` sends the version it rates.

## The critic's judgments (R-723)

`ratings/critiques/<item id>.json` (format `ursatz-item-critiques` 1, [schema](../schema/ursatz-item-critiques.v1.schema.json)): `{item_id, critiques, verdicts}`.

- `critiques`: `{id, version, at, actor, note, score: {numerator, denominator}, evaluators: [{rule, penalty, weight, reversed}], observations: [{id, rule, scored, bar, beat, message}]}`. One is one candidate of the Composer's critique answer ([critic](../../composer/docs/critic.md#the-critique-entry)) recorded against a version of the item, with an id (`crt-`) and an id (`obs-`) for each observation, so a verdict can name it. `bar` and `beat` are null for an observation with no place.
- `verdicts`: `{id, observation, verdict, note, actor, at}`: her verdict, `agree` or `disagree`, on an observation of one of the item's critiques (`vrd-`).

`ratings.critique` (target a motif or a theme, `version`, optional `settings` and `note`; it takes `base_version`) reads the version's plan (a theme's `plan.json`) or motive (a motif's analyzed motive), sends the Composer's `critique` entry the item's resolved settings (or the `settings` param: a preferences document whose `critic` group sets the weights), and keeps the record. It answers `{id, version, score, observations}` with the ids. `ratings.verdict` (target an item, `observation`, `verdict`, optional `note`) is refused when the item's file holds no such observation (`/params/observation`). Each write is one transaction whose target is the item, so `ursatz history item:<id>` lists it. The query `ratings.critiques` (param `item`) answers the file, or empty lists.

A theme composed with `critic.rank` on keeps the critic's choice with its version: the body's `result.critic` is `{critic, rejected}`, the judgment of the theme and the candidates the critic passed over ([ranking](../../composer/docs/critic.md#ranking-theme-candidates-r-722)); a version composed without ranking has none. Listening rounds do not write verdicts through these commands yet (the listen skill, R-674).

## Keep as default

`ratings.keep` is how a value gets into a taste profile (`settings.set` refuses a taste scope): it cites at least one recorded rating or pick id ("keep as default cites the ratings behind it"; an id that was never recorded: "<id> is not a recorded rating or pick" at `/params/ratings/<i>`), checks the value as `settings.set` does, and sets or drops (a null or absent `value`) the key in `settings/taste/<taste>.json`, made when missing. It appends `{key, value, ratings, note, actor, at}` to the profile's `kept` list, so every default says which listening produced it ([settings](settings.md)).

## Queries

`ratings.session` (param `session`) answers the session; `ratings.sessions` answers `{sessions: [{id, created_at, what, taste, batches, ratings, picks}]}` newest first; `ratings.item` (param `item_id`) answers `{item_id, ratings}`; `ratings.critiques` (param `item`) answers `{item_id, critiques, verdicts}`. After a committed `ratings.` command the event `ratings-changed` is published ([jobs and events](jobs-and-events.md#events)).
