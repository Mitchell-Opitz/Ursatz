# Preferences

The composer's settings as one document (R-411, R-413): the numbers that shape what it makes, so that a listening session can change one and compose again with no code change and no rebuild. Back to the [docs index](README.md).

Header: `<ursatz_composer/preferences.h>`. Source: `src/preferences/preferences.c`.

Every member is optional and its default is the value the Composer used before the document existed. With no preferences every output is byte-identical to what it was (R-413). A member that is unknown, of the wrong type or outside its limit is refused with its JSON path, for example `/preferences/melody/keep`; the functions give the path relative to the object (`/melody/keep`). A value is a whole number unless the table says otherwise; `null` is a wrong type.

## Functions

| Function | Does |
| --- | --- |
| `composer_preferences_default(out)` | Writes the defaults. |
| `composer_preferences_read(object, out, path, path_size, message, message_size)` | Starts from the defaults, applies every member present, and returns false at the first bad one with its `path` and a `message` naming the limit. |
| `composer_preferences_write(preferences)` | The full document, every member, as a `cJSON` object the caller deletes. Ratios are `"n/d"` text and `nct_types` is in the order passing, neighbor, appoggiatura, suspension, anticipation. |
| `composer_preferences_is_default(preferences)` | True when every setting is the default. |

`ComposerPreferences` holds `MelodyFillParams melody`, `MotiveRandomPreferences motif` and `GrowFidelityTable development`. The API carries the document as the `preferences` member of the random motive and theme requests (below).

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
                "ending_reduction": 1, "waves": 1, "central_range": 1, "monotony": 1, "seam": 1}
  }
}
```

`start_cell` stays a parameter of the stage and is not a preference. The first melody attempt of a compose run uses `climax_position` and a retry uses its own position (`CLIMAX_RETRIES`, `compose_run.c`); the other melody settings apply to every attempt.

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
    "basic_as_written": false
  }
}
```

## In the API (1.17)

The random motive and theme requests take the document as an optional `preferences` member: a JSON object of the shape above, every member optional, read with `composer_preferences_read`. A member that is unknown, of the wrong type or outside its limit is refused with `COMPOSER_ERR_REQUEST` and the reader's path under `/preferences`, for example `/preferences/melody/keep`. A request that has the member gets `"preferences"` in its response: the full document with the defaults filled in, so the response says what was used. A request without it has no such member and its response is what it was before.

| Request | Groups that apply |
|---|---|
| `composer_api_random_motive` | `motif`, for the segments strategy (the drawn strategy keeps its constants). `melody` and `development` are accepted and echoed. |
| `composer_api_theme` | `melody` and `development` (the fidelity minimums, also shown in the `minimum` of each development item, and, from 1.17, the continuation settings and `basic_as_written`). `motif` is accepted, echoed and has no effect: a theme's motives come from the request. |

`composer_api_validate` reads the member for `theme` and `random_motive` and refuses a bad one with its path. The vocabulary serves the default document as `preferences` and the limits as numbers in `limits.preferences` (`weight_max`, `pickup_one_in_max`, `accented_one_in_max`, `long_nct_one_in_max`, `leaps_per_motif_max`, `syncopation_one_in_max`, `skips_per_motif_max`, `plan_chords_max`, `plan_beat_max`, `leap_min`, `leap_max`, `range_low_min`, `range_low_max`, `range_high_max`, `range_span_min`, `density_max`, `scale_max`, `climax_rise_min`, `climax_rise_max`, `ratio_denominator_max`, `fidelity_denominator_max`, `attempts_max`, `keep_max`). API 1.17 adds the L3 settings (syncopation, skips, a given chord plan, the continuation settings and `basic_as_written`; every one off by default, R-422). A given `motif.plan` that does not fit the meter, bars, shapes or endings allowed is refused by a random motive run as `COMPOSER_ERR_REQUEST` at `/preferences/motif/plan` (`validate` leaves that meter-dependent refusal to the run; its syntax refusals come from the reader). The piece, fill, grow, regenerate and variations requests take no `preferences` member yet; a setting is promoted to them when it earns it (R-411), and a plan does not store the document.
