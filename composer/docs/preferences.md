# Preferences

The composer's settings as one document (R-411, R-413): the numbers that shape what it makes, so that a listening session can change one and compose again with no code change and no rebuild. Back to the [docs index](README.md).

Header: `<ursatz_composer/preferences.h>`. Source: `src/preferences/preferences.c`. Schema: [`schema/api/preferences.schema.json`](../schema/api/preferences.schema.json), the one definition the theme and random motive requests and responses and the vocabulary refer to.

Every member is optional and its default is the value the Composer used before the document existed. With no preferences every output is byte-identical to what it was (R-413). A member that is unknown, of the wrong type or outside its limit is refused with its JSON path, for example `/preferences/melody/keep`; the functions give the path relative to the object (`/melody/keep`). A value is a whole number unless the table says otherwise; `null` is a wrong type.

## Functions

| Function | Does |
| --- | --- |
| `composer_preferences_default(out)` | Writes the defaults. |
| `composer_preferences_read(object, out, path, path_size, message, message_size)` | Starts from the defaults, applies every member present, and returns false at the first bad one with its `path` and a `message` naming the limit. |
| `composer_preferences_write(preferences)` | The full document, every member, as a `cJSON` object the caller deletes. Ratios are `"n/d"` text and `nct_types` is in the order passing, neighbor, appoggiatura, suspension, anticipation. |
| `composer_preferences_keys(count)` | The key table: one row per setting (below). Static. |
| `composer_preferences_rules(count)` | The rule table: the 13 soft groups and the 7 hard rules of the melody stage. Static. |

`ComposerPreferences` holds `MelodyFillParams melody`, `MotiveRandomPreferences motif`, `GrowFidelityTable development` and, from 1.19, `GrowContrast contrast` (the development group's contrast settings). The API carries the document as the `preferences` member of the random motive and theme requests (below).

## The melody group

The settings of the melody stage ([melody-fill.md](melody-fill.md)), stored in `MelodyFillParams`.

| Key | Default | Limit | What it shapes | Replaces |
| --- | --- | --- | --- | --- |
| `range.low` | 60 | 36 to 96 | The lowest melody pitch (MIDI). | `DEFAULT_RANGE_LOW` |
| `range.high` | 84 | `range.low` + 12 to 108 | The highest melody pitch. | `DEFAULT_RANGE_HIGH` |
| `nct_types` | `["passing", "neighbor"]` | a list of `passing`, `neighbor`, `appoggiatura`, `suspension`, `anticipation`, no repeats | The non-chord tones the generated notes may be. An empty list writes chord tones only. | `DEFAULT_NCT_TYPES` |
| `density` | 0 | 0 to 4 | Notes to a cell after the motive; 0 repeats the motive's rhythm. | `DEFAULT_DENSITY` |
| `climax_position` | 667 | 0 to 1000 | Where the climax falls, in thousandths of the phrase. | `DEFAULT_CLIMAX` |
| `climax_rise` | 4 | 1 to 12 | How far above the motive's top the drawn climax may rise; the peak is drawn from 2 to `climax_rise` + 1 above it. | `CLIMAX_RISE` |
| `climax_slack` | 150 | 0 to 1000 | How far from `climax_position`, in thousandths, a climax still scores as in place. | `POSITION_SLACK` |
| `second_nearest_chord_tone` | `"1/3"` | a ratio `n/d`, 0 <= n <= d, 1 <= d <= 16 | The share of draws that take the second-nearest chord tone instead of the nearest. | `NEAREST_IN`, `NEAREST_OF` |
| `attempts` | 48 | 1 to 256 | Melodies tried for each target. | `MELODY_ATTEMPTS` |
| `keep` | 6 | 1 to 16, at most `attempts` | Melodies kept of those tried, repeats dropped. | `MELODY_KEEP` |
| `plain_shaping` | false | true or false | True leaves out the four shaping rules. | |
| `weights` | all 1 | each 0 to 20 | How many times each group of soft rules counts in a melody's score. | |
| `closure.final` | 0 | 0 to 20 | How many times the closure rules count for the target that ends the theme's last cadence. 0 leaves them out. Off by default; R-422. | new (R-478) |
| `closure.first` | 0 | 0 to 20 | The same for a target that ends an earlier cadence, such as an antecedent's weak cadence. A name left out of `closure` keeps its default. | new (R-478) |
| `rhythm_repeat_bars` | 0 | 0 to 16 | The most bars of 2 or more notes in a row that may share one rhythm in a theme's line; 0 sets no cap. | new (R-479) |
| `leaps` | 0 | 0 to 20 | How many times a melody's leaps count against it (`leap_weight`). 0 leaves them out. | new (R-481) |
| `register` | 0 | 0 to 20 | How many times an idea's drift from the register of the span before it counts against it (`register_weight`). 0 leaves it out. | new (R-481) |
| `cadence_long` | `false` | true or false | A target that ends a cadence ends on one note held from its last downbeat to its end (`MelodyFillParams.cadence_long`, [melody-fill.md](melody-fill.md)). Hard. Off by default. | new (R-712) |
| `waive` | `[]` | a list of `forbidden-leap`, `forbidden-outline`, `no-crossing`, `parallel-octaves`, `parallel-fifths`, `dissonance-against-bass`, `strong-beat-chord-tone`, no repeats | The hard rules the melody stage does not hold: a waived rule neither refuses a candidate nor scores it. Off by default; R-672. | new (R-672, API 4.3) |
| `reverse` | `[]` | a list of the groups of `weights` and `closure`, `leaps`, `register`, no repeats | The soft rule groups whose score counts what a candidate breaks in place of what it holds, so a setting can ask for the opposite. Off by default; R-672. | new (R-672, API 4.3) |

`weights` has ten keys. The score is the share of soft rules held: the sum of `weight * held` over the sum of `weight * total`. A weight of 0 leaves a group out; every weight 1 is the score before weights existed.

| Weight | The soft rules it counts |
| --- | --- |
| `melodic` | standard-counterpoint's melodic rules over the line. |
| `against_bass` | Its harmonic rules between the melody and the bass, and imperfect consonances on strong beats. |
| `tendency` | The resolution of tendency tones from chord to chord. |
| `climax` | The climax of an idea that carries the phrase's climax is above the phrase before it. |
| `contour` | The one climax, its place, recovered leaps and few repeated notes. |
| `ending_reduction`, `waves`, `central_range`, `monotony` | One shaping rule each (see [the shaping rules](melody-fill.md#the-shaping-rules)). |
| `seam` | The rules at the seam of a loop. |

`waive` and `reverse` are the overrides of the melody stage (R-672). A name that is no rule is refused at `/melody/waive/<i>` ("is not a rule the melody stage can waive: " and the names) or `/melody/reverse/<i>` ("is not a soft rule group: " and the names), a repeat at the same place, a value that is no list at `/melody/waive` or `/melody/reverse`. The range, the stable tone of the cadence, the non-chord tone types and the checks that are not named are never waived. A reversed group adds `total - held` to the score in place of `held`; with `reverse` empty the score is the same integers as before.

What a waived rule leaves out of the checker: `strong-beat-chord-tone` the chord tone asked of a strong beat (the range stays hard); `forbidden-leap` and `forbidden-outline` the melodic check, in the relaxed run check too; `dissonance-against-bass` the refusal of a chord tone that is dissonant over the bass, which is not scored either; `no-crossing`, `parallel-octaves` and `parallel-fifths` the harmonic check, so a waived parallel does not score. The checker keeps the weighted score of each group apart and adds a reversed group as `total - held` where the verdict is finished ([melody-fill.md](melody-fill.md#the-checker)).

```json
{
  "melody": {
    "range": {"low": 60, "high": 84},
    "nct_types": ["passing", "neighbor"],
    "density": 0,
    "climax_position": 667,
    "climax_rise": 4,
    "climax_slack": 150,
    "second_nearest_chord_tone": "1/3",
    "attempts": 48,
    "keep": 6,
    "plain_shaping": false,
    "weights": {"melodic": 1, "against_bass": 1, "tendency": 1, "climax": 1, "contour": 1,
                "ending_reduction": 1, "waves": 1, "central_range": 1, "monotony": 1, "seam": 1},
    "closure": {"final": 0, "first": 0},
    "rhythm_repeat_bars": 0,
    "leaps": 0,
    "register": 0,
    "waive": [],
    "reverse": []
  }
}
```

`start_cell` stays a parameter of the stage and is not a preference. The first melody attempt of a compose run uses `climax_position` and a retry uses its own position (`CLIMAX_RETRIES`, `compose_run.c`); the other melody settings apply to every attempt.

## The candidates group (R-673)

How a set of random motives is chosen, in `CandidatePreferences`. Off by default.

| Key | Default | Limit | What it shapes |
| --- | --- | --- | --- |
| `variety` | 0 | 0 to 20 | How much each random motive is chosen for being unlike the ones before it. 0 draws as it always did. With a style the choice mixes closeness to the style (counted 20 - `variety` times) and distance from the other motives (`variety` times). |
| `pool` | 4 | 2 to 8 | The candidates drawn for each motive that is chosen among (at least the 8 a style asks for, when there is one). Read only when `variety` is above 0. |

The distance of a candidate is the least, over the motives of the request's `avoid` list and those already chosen, of 1 - (rhythm similarity + contour similarity) / 2 (the library's `similarity_rhythm_score` and `similarity_contour_score`, the contour from the directions of the intervals); a tie keeps the first candidate drawn. A candidate equal to a motive of `avoid` is never kept. A value outside its limit is refused at `/candidates/variety` or `/candidates/pool`. The document's six groups are `melody`, `motif`, `development`, `candidates`, `critic` and `harmony`, in that order.

## The critic group (R-722)

The critic's settings, in `CriticSettings` ([critic.md](critic.md)). `rank` and the reversed evaluators change what a run chooses; with the defaults every output is what it was.

| Key | Default | Limit | What it shapes |
| --- | --- | --- | --- |
| `rank` | false | true or false | Choose a theme among several candidates by the critic's score (B5). |
| `candidates` | 4 | 2 to 8 | The candidates made for each theme kept. |
| `keep_rejected` | 8 | 0 to 16 | The rejected candidates a run reports. |
| `reverse` | none | evaluator codes | The evaluators whose penalty counts for a candidate (none twice). |
| `weights.<code>` | 1 | 0 to 20 | The weight of each evaluator: `rhythmic_monotony`, `contrast_distance`, `leap_load`, `open_ending`, `stray_syncopation`, `one_rhythm`, `repeated_notes`, `wide_range`. |

A value outside its limit is refused at `/critic/<key>`, an evaluator that is unknown at `/critic/weights/<code>` or `/critic/reverse/<i>`.

## The key and rule tables (R-672)

`composer_preferences_keys` lists every setting by its dotted key, for an application that edits settings one at a time ("melody.weights.melodic"). There is one row per leaf of the default document, in document order (a weighted list's entries and a list of words are leaves of their own, a list of words being one leaf), the two keys `melody.waive` and `melody.reverse` after `melody.register`, and `motif.plan`, the one setting the default document does not hold, after `motif.color_note`: 144 rows (the 12 of the critic group, then `harmony.mixture`). The color note settings are in [modes.md](modes.md#the-color-note), `harmony.mixture` in [modes.md](modes.md#modal-mixture). A row has the `key`, its `type` (`integer` from `min` to `max`, `boolean`, `ratio` with the denominator limit in `max`, `words` with the words it may hold, or `plan`) and whether it is `hard`.

A **hard** setting changes what a request may ask for or what a candidate may be: `melody.range.low`, `melody.range.high`, `melody.nct_types`, `melody.rhythm_repeat_bars`, `melody.waive`, `motif.leap.low`, `motif.leap.high`, `motif.leaps_per_motif`, `melody.cadence_long`, `motif.skips_per_motif`, `motif.contrast_fidelity_min`, `motif.plan` and every `development.fidelity_minimum.*` (R-671). Every other setting is **soft**: a preference a listener can move.

`composer_preferences_rules` lists the rules a setting can override, in enum order: the 13 soft groups (`melody.weights.<name>` for the ten weights, `melody.closure`, `melody.leaps`, `melody.register`) the 7 hard rules (key `melody.waive`) and the 8 evaluators of the critic (key `critic.weights.<code>`, soft). The vocabulary serves both tables as `preference_keys` and `rules` ([api.md](api.md)).

## The motif group

The settings of the segments motive generator are in [preferences-motif.md](preferences-motif.md).

## The development group

The least fidelity an idea of a role may have under each development setting before the melody stage refuses it ([grow-development.md](grow-development.md#ranking-and-refusing-by-fidelity)), stored in `GrowFidelityTable` and read through `GrowConfig.fidelity` and `ComposeConfig.fidelity` (NULL for the defaults; it must outlive the run), and the continuation settings and `basic_as_written` of a theme's development (`ComposerPreferences.continuation` and `.basic_as_written`, passed to the grow run by the theme request).

| Key | Default | Limit | What it shapes | Replaces |
| --- | --- | --- | --- | --- |
| `fidelity_minimum.<setting>.<role>` | the table below | a ratio `n/d` as text, 0 <= n <= d, 1 <= d <= 1000 | The score an idea of the role may not fall under. `<setting>` is `melody`, `balanced` or `theme`; `<role>` is `presentation`, `continuation` or `cadential`. | `FIDELITY_MINIMUM` |
| `continuation.every_segment` | `false` | true or false | Both fragments of a sentence's continuation are drawn from every segment of the motif, never both from the same one. Off by default; R-422. | new (R-462) |
| `continuation.fragment_lengths` | `segment` 1, `half_bar` 0, `beat` 0 | weights 0 to 20, sum above 0 | A fragment is its whole segment, half a bar of it, or a beat of it. A name left out keeps its default. Off by default; R-422. | new (R-463) |
| `continuation.diminish` | `false` | true or false | A fragment that is a whole segment is that segment in diminution 1:2. Off by default; R-422. | new (R-463) |
| `continuation.liquidate` | `false` | true or false | Fragment 2 is liquidated, and the cadential idea liquidates a step further. Off by default; R-422. | new (R-464) |
| `continuation.faster_harmony` | `false` | true or false | An 8-bar sentence's continuation changes chord every cell and cadences on the cadential idea; no effect where a bar is one cell. Off by default; R-422. | new (R-465) |
| `basic_as_written` | `false` | true or false | A motif as long as the basic idea, with chords of its own, is the basic idea as it is, over its own chords. Off by default; R-422. | new (R-466) |
| `contrast.distance` | `similar` 1, `distant` 0 | weights 0 to 20 | A contrasting idea strays from the basic idea's rhythm as today (`similar`) or takes a rhythm drawn from the motif settings (`distant`). All 0 reads as `similar` 1; a name left out keeps its default. | new (R-475) |
| `contrast.new_material` | 0 | 0 to 20 | The weight of a period's contrasting idea made of new material, a second motif from the motif settings, against the distance draw. Off by default; R-422. | new (R-477) |
| `functions.harmony` | `false` | true or false | An antecedent, a continuation and a consequent take the harmony of their ideas' formal functions: a predominant before the cadence's dominant, a chord a cell in the fragments, free of their motive's chords ([grow-harmony.md](grow-harmony.md)). Off by default. | new (R-711) |
| `functions.surface` | `false` | true or false | A sentence's fragments follow their formal function: fragment 1 is the segment of the motif with the most notes per beat that fits, fragment 2 the next one, exact, or the same one in sequence when only one fits; no random number ([grow-continuation.md](grow-continuation.md)). Off by default. | new (R-712) |
| `functions.contrast` | `false` | true or false | A contrasting idea of an antecedent, a compound basic idea or a consequent is drawn as a distant contrast that meets its function's contrast target (rhythm and contour by 1/2), or the structure is dropped; the idea node holds a `contrast` constraint and the theme response gives `contrasts` ([grow-contrast.md](grow-contrast.md#contrast-targets)). Off by default. | new (R-710) |

| Role | `melody` | `balanced` | `theme` |
| --- | --- | --- | --- |
| `presentation` | `"11/20"` | `"11/20"` | `"11/20"` |
| `continuation` | `"7/20"` | `"7/20"` | `"7/20"` |
| `cadential` | `"7/20"` | `"3/10"` | `"1/5"` |

The continuation settings are described in [grow-continuation.md](grow-continuation.md). A contrasting idea and an idea with no role are not settings: their minimum is 0. A setting or role not named here is refused (`/development/fidelity_minimum/contrast`). The theme's Development list shows the minimum of the table in use.

```json
{
  "development": {
    "fidelity_minimum": {
      "melody": {"presentation": "11/20", "continuation": "7/20", "cadential": "7/20"},
      "balanced": {"presentation": "11/20", "continuation": "7/20", "cadential": "3/10"},
      "theme": {"presentation": "11/20", "continuation": "7/20", "cadential": "1/5"}
    },
    "continuation": {
      "every_segment": false,
      "fragment_lengths": {"segment": 1, "half_bar": 0, "beat": 0},
      "diminish": false,
      "liquidate": false,
      "faster_harmony": false
    },
    "basic_as_written": false,
    "contrast": {
      "distance": {"similar": 1, "distant": 0},
      "new_material": 0
    }
  }
}
```

## In the API (1.19)

The random motive and theme requests take the document as an optional `preferences` member: a JSON object of the shape above, every member optional, read with `composer_preferences_read`. A member that is unknown, of the wrong type or outside its limit is refused with `URSATZ_ERR_REQUEST` and the reader's path under `/preferences`, for example `/preferences/melody/keep`. A request that has the member gets `"preferences"` in its response: the full document with the defaults filled in, so the response says what was used. A request without it has no such member and its response is what it was before.

| Request | Groups that apply |
|---|---|
| `composer_api_random_motive` | `motif`, for the segments strategy (the drawn strategy keeps its constants), and `candidates`. `melody` and `development` are accepted and echoed. |
| `composer_api_theme` | `melody` and `development` (the fidelity minimums, also shown in the `minimum` of each development item, from 1.17 the continuation settings and `basic_as_written`, and from 1.19 the L4 settings: `melody.closure`, `rhythm_repeat_bars`, `leaps`, `register` and `development.contrast`). `motif` shapes a theme's drawn rhythms and new material (1.19) and nothing else: a theme's motives come from the request. |

`composer_api_validate` reads the member for `theme` and `random_motive` and refuses a bad one with its path. The vocabulary serves the default document as `preferences` and the limits as numbers in `limits.preferences` (`weight_max`, `pickup_one_in_max`, `accented_one_in_max`, `long_nct_one_in_max`, `leaps_per_motif_max`, `syncopation_one_in_max`, `skips_per_motif_max`, `plan_chords_max`, `plan_beat_max`, `leap_min`, `leap_max`, `range_low_min`, `range_low_max`, `range_high_max`, `range_span_min`, `density_max`, `scale_max`, `climax_rise_min`, `climax_rise_max`, `ratio_denominator_max`, `fidelity_denominator_max`, `attempts_max`, `keep_max`, and from 1.19 `rhythm_repeat_bars_max`). API 1.19 adds the L4 settings (closure, the cap on bars with one rhythm, leaps, register, the contrast distance and new material; every one off by default, R-422). API 1.17 adds the L3 settings (syncopation, skips, a given chord plan, the continuation settings and `basic_as_written`; every one off by default, R-422). A given `motif.plan` that does not fit the meter, bars, shapes or endings allowed is refused by a random motive run as `URSATZ_ERR_REQUEST` at `/preferences/motif/plan` (`validate` leaves that meter-dependent refusal to the run; its syntax refusals come from the reader). The piece, fill, grow, regenerate and variations requests take no `preferences` member yet; a setting is promoted to them when it earns it (R-411), and a plan does not store the document.
