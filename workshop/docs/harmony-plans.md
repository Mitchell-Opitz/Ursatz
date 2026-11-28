# Harmony plans

A harmony plan is an item of type `harmony` (id prefix `hrm-`, R-767): a progression request of the lab ([harmony-lab.md](../../composer/docs/harmony-lab.md)) that is composed into chords with a bass and, with an accompaniment, the chords sounded with no melody. Back to the [docs index](README.md).

## The body

`body.json` is format `ursatz-harmony-body` 1 (`workshop/schema/ursatz-harmony-body.v1.schema.json`): `{uses, request, result?}`. `request` is the Composer's progression request (`key`, `meter`, `bars`, `cadence`, `palette`, `accompaniment`, `seed`); `uses` holds the versions it refers to (none today). The store checks the shape only; the Composer judges the request at validate and compose. `item.validate` with `type` `harmony` runs the request through `composer_api_progression` with no files and no suggestions and answers `{"complete": true, "request"}`, `{"complete": false, "message"}` for a request that lacks key, meter, bars or cadence, or the Composer's refusal with the member at fault as its path.

## Composing

`item.compose` on a harmony item is a job like a theme's (params `seed`, `update_uses`; no settings). It sends the stored request to `composer_api_progression` with `midi`, `musicxml` and `plans` true and no `suggest`, and the new version holds `request.json` (what the Composer read), `plan.json`, `harmony.mid` and `harmony.musicxml` beside its body, whose `result` is `{seed, harmony, measures, files}`: the response's `harmony` (`chords` as text, `stages` as OMT numerals) and `measures` (a tension and brightness for each bar). `item.playback` with `file` `harmony` plays the MIDI file; `item.export` gives the text export, with the chords as facts and the MusicXML as its draft.

## Using a plan

A theme's request `chords` may be `{"$item": {"item_id", "version"}}` of a composed harmony version, and a piece section's `harmony` too. The workshop resolves it before the Composer is called: a theme gets the version's `result.harmony.chords` with each bar less one (a theme counts bars from 0), a section `{"chords": [...]}` with the bars as they are (from 1). A reference to a version that is not composed is `URSATZ_ERR_NOT_FOUND` ("the harmony version is not composed"). The body's `uses` copies the harmony version, as it does a motif's, so lineage is kept and the copy is refreshed when the use is updated.

`item.apply-harmony` (target a theme or a piece item, `base` the version read) takes `harmony` (an `hrm-` id), `version`, `section` (a piece's section name: required for a piece, refused for a theme) and `note`. It writes a new version of the target with the reference in place of its chords or the named section's harmony, and the use of the harmony version, in one transaction. It is refused when the harmony version is not composed (`/params/version`), the target is no theme or piece (`/target`), or the section is unknown (`/params/section`, not found).
