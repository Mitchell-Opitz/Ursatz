# Settings

The settings of the Composer's stages and of exploration, kept in scope files, resolved for a request with the scope each value came from (R-671, R-676). Back to the [docs index](README.md).

Code: `workshop/src/settings/` (`settings_keys`, `settings_scopes`, `settings_resolve`), the queries in `coordinator/api_settings.c`, the commands in `coordinator/api_commands_settings.c`.

## Keys and rules

A setting is named by its dotted key into the Composer's preferences document (`melody.weights.melodic`, `motif.durations.beat`). The workshop reads the key table and the rule table from the Composer's vocabulary (`preference_keys` and `rules`, [Composer preferences](../../composer/docs/preferences.md#the-key-and-rule-tables-r-672)) when it opens, and adds its own two keys: `explore.share` (a ratio from `0/1` to `1/2`, denominator up to 16, default `0/1`) and `explore.distance` (1 to 8, default 2). A key is **hard** when it changes what a request may ask for or what a candidate may be, else **soft**. `settings.keys` answers `{keys, rules}`.

A value is a number, a boolean, a string (a ratio is the string `n/d`) or a list of numbers and strings: the command param type `value`. It is checked when it is set or imported: an unknown key at `/params/key` ("unknown setting 'x'"), `motif.plan` in no scope file ("a chord plan is intent, not a setting: give it with the request"), an `explore.*` value against its limits, any other by the Composer (`composer_api_validate` of kind `random_motive`), whose message arrives at `/params/value` (`/params/document/...` for an import).

## Scope files

| Scope | File | Holds |
|---|---|---|
| `taste:<name>` | `settings/taste/<name>.json` | `values`, `kept` |
| `style:<id>` | `settings/styles/<id>/settings.json` | `values`, `taste` |
| `project:<id>` | `projects/<id>/settings.json` | `values`, `overrides`, `off` (`style`, `taste`), `taste` |
| `item:<id>` | `projects/<id>/items/<id>/settings.json` | `values`, `overrides`, `off` (also `project`), `taste` |

Format `ursatz-settings` 1: `scope`, `values` (dotted key to value), and the optional members above. `overrides` are `{rule, action, weight, note, actor, at}` with `action` `off`, `reverse`, `reweight` or `waive`. `kept` entries are `{key, value, ratings, note, actor, at}`: what an import and keep as default recorded and why. A taste or style file never holds `overrides` or `off`, a taste file never names a `taste`, and only a taste file has `kept`. A missing file is an empty scope. `settings.scope` reads one, `settings.list` names the files present.

## Resolution

`settings.resolve` (params `project_id`, `item_id`, `style_id`, `taste`, `settings`, `overrides`, `off`, `all`) answers `{taste, off, values: [{key, value, from}], overrides: [{rule, action, weight, from}], document}`. The scopes apply in this order, each over the one before: the defaults, the style, `taste:all`, `taste:<name>`, the project, the item, the request; a scope's values first, then its overrides. The profile name is the request's `taste`, else the item's, the project's, the style's, else `none`. A scope an off list names is skipped (the request's and the item's, and the project's when it is not itself switched off); `taste` off skips both profiles.

`from` is `default`, `style:<id>`, `taste:all`, `taste:<name>`, `project:<id>`, `item:<id>` or `request`. `values` lists the keys not at their default (every key with `all`); `document` holds them nested by their dots, the `explore.*` keys left out, or is null when there are none. A request with no settings anywhere resolves to a null document, and sends the Composer nothing.

An override maps through the rule table: `reweight` sets the group's weight (`closure` both of its weights), `off` or `waive` on a soft group sets its weight to 0, on a hard rule adds its name to `melody.waive`, `reverse` adds the group to `melody.reverse`.

## Commands

All take a `scope`, are journaled, and answer with the scope's content after the change. A scope that names a style, project or item that is not there is `NOT_FOUND` at `/params/scope`.

```
settings.set scope item:itm-1 key melody.keep value 3
settings.unset scope item:itm-1 key melody.keep
settings.override scope item:itm-1 rule parallel-fifths action waive
settings.switch scope item:itm-1 off [taste]
settings.use-taste scope project:prj-1 taste none
```

`settings.set` and `settings.unset` refuse a taste scope ("a taste setting changes only by keep as default (ratings.keep)"). `settings.import` takes a `document` (flattened to dotted keys, each checked as `set` checks) and `replace`; for a taste profile it needs a `note` and appends `{key: "*", value: null, ratings: [], note, actor, at}` to `kept`. `settings.override` (project and item only; `clear` removes a rule's override; a rule has one) names an unknown rule's alternatives, needs a `weight` for `reweight` and takes `reverse` only on a soft group. `settings.switch` replaces the off list (`project` only on an item). `settings.use-taste` names the profile a style, project or item uses, or clears it. After a committed `settings.` command the event `settings-changed` is published ([jobs and events](jobs-and-events.md#events)).
