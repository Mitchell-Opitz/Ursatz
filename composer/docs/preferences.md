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

The settings of the segments motive generator ([cli-random-motive.md](cli-random-motive.md#the-segments-strategy)), stored in `MotiveRandomPreferences` and read through `MotiveRandomOptions.preferences` (NULL for the defaults). The drawn strategy keeps its constants as the comparison baseline and reads none of them (R-304). A weighted list is an object of whole numbers 0 to 20 under the names below, with a sum above 0; a name left out keeps its default. A list whose weights sum to 0 where the meter leaves an entry out (a three-way shape list in a meter with no halves) draws its first entry.

| Key | Default | Limit | What it shapes | Replaces |
| --- | --- | --- | --- | --- |
| `bars` | `one` 1, `two` 1 | weighted list | A motive of one bar or of two. One bar only in a meter of an even number of beats, four or more. | `SEGMENT_BAR_ONE_BAR_WEIGHT`, `SEGMENT_BAR_TWO_BARS_WEIGHT` |
| `pickup_one_in` | 4 | 0 to 64 | A pickup (half a beat or a beat) in one motive of this many; 0 never draws one and draws no random number for it. | `SEGMENT_PICKUP_ONE_IN` |
| `shapes_one_bar` | `repeat` 3, `contrast` 2 | weighted list | The shape of a one-bar motive: a a' or a b. | `SHAPE_WEIGHTS_ONE_BAR` |
| `shapes_two_bars` | `repeat` 3, `contrast` 2, `repeat_contrast` 2 | weighted list | The shape of a two-bar motive; `repeat_contrast` (a a' b) only with an even meter. | `SHAPE_WEIGHTS_TWO_BARS` |
| `plans_two` | `I-V` 3, `I-IV` 1, `I-I` 1 | weighted list | The chords of a two-segment motive. | the weights of `PLANS_TWO` |
| `plans_three` | `I-V-I` 2, `I-IV-I` 1 | weighted list | The chords of a three-segment motive. | the weights of `PLANS_THREE` |
| `features` | `dotted` 1, `leap` 1, `repeat` 1 | weighted list | The distinctive feature of segment a. A feature that cannot fit its cell gives way to the next one with a weight. | `FEATURE_KINDS` (equal) |
| `leap.low` | 5 | 3 to 12 | The smallest leap, in semitones, a leap feature may be. | `SEGMENT_LEAP_LOW` |
| `leap.high` | 9 | `leap.low` to 12 | The largest. | `SEGMENT_LEAP_HIGH` |
| `rung_moves` | `step` 6, `skip` 2 | weighted list | A walk on the chord tones moves one rung or two. | the `moves` table of `next_rung` |
| `decorations` | `chord_tone` 1, `passing` 1, `neighbor` 1, `appoggiatura` 1, `escape` 1 | weighted list | What a note between two frame notes is: another chord tone or a non-chord tone of the type. | `DECORATION_KINDS` (equal) |
| `durations` | `half_beat` 2, `beat` 4, `two_beats` 1 | weighted list | The note lengths a cell is tiled with; a length that does not fit its place is left out. | `MOTIVE_RANDOM_DURATION_WEIGHTS` (the drawn strategy still reads it) |

With every default each draw makes the random-number calls it always made, so a seed gives the motives it always gave (the `random-motive*` snapshots are unchanged). The plan degrees (I-V, I-IV, I-I, I-V-I, I-IV-I) stay in `motive_random_segments.c`; the preferences set only how often each is drawn.

```json
{
  "motif": {
    "bars": {"one": 1, "two": 1},
    "pickup_one_in": 4,
    "shapes_one_bar": {"repeat": 3, "contrast": 2},
    "shapes_two_bars": {"repeat": 3, "contrast": 2, "repeat_contrast": 2},
    "plans_two": {"I-V": 3, "I-IV": 1, "I-I": 1},
    "plans_three": {"I-V-I": 2, "I-IV-I": 1},
    "features": {"dotted": 1, "leap": 1, "repeat": 1},
    "leap": {"low": 5, "high": 9},
    "rung_moves": {"step": 6, "skip": 2},
    "decorations": {"chord_tone": 1, "passing": 1, "neighbor": 1, "appoggiatura": 1, "escape": 1},
    "durations": {"half_beat": 2, "beat": 4, "two_beats": 1}
  }
}
```

## The development group

The least fidelity an idea of a role may have under each development setting before the melody stage refuses it ([grow-development.md](grow-development.md#ranking-and-refusing-by-fidelity)), stored in `GrowFidelityTable` and read through `GrowConfig.fidelity` and `ComposeConfig.fidelity` (NULL for the defaults; it must outlive the run).

| Key | Default | Limit | What it shapes | Replaces |
| --- | --- | --- | --- | --- |
| `fidelity_minimum.<setting>.<role>` | the table below | a ratio `n/d` as text, 0 <= n <= d, 1 <= d <= 1000 | The score an idea of the role may not fall under. `<setting>` is `melody`, `balanced` or `theme`; `<role>` is `presentation`, `continuation` or `cadential`. | `FIDELITY_MINIMUM` |

| Role | `melody` | `balanced` | `theme` |
| --- | --- | --- | --- |
| `presentation` | `"11/20"` | `"11/20"` | `"11/20"` |
| `continuation` | `"7/20"` | `"7/20"` | `"7/20"` |
| `cadential` | `"7/20"` | `"3/10"` | `"1/5"` |

A contrasting idea and an idea with no role are not settings: their minimum is 0. A setting or role not named here is refused (`/development/fidelity_minimum/contrast`). The theme's Development list shows the minimum of the table in use.

```json
{
  "development": {
    "fidelity_minimum": {
      "melody": {"presentation": "11/20", "continuation": "7/20", "cadential": "7/20"},
      "balanced": {"presentation": "11/20", "continuation": "7/20", "cadential": "3/10"},
      "theme": {"presentation": "11/20", "continuation": "7/20", "cadential": "1/5"}
    }
  }
}
```

## In the API (1.11)

The random motive and theme requests take the document as an optional `preferences` member: a JSON object of the shape above, every member optional, read with `composer_preferences_read`. A member that is unknown, of the wrong type or outside its limit is refused with `COMPOSER_ERR_REQUEST` and the reader's path under `/preferences`, for example `/preferences/melody/keep`. A request that has the member gets `"preferences"` in its response: the full document with the defaults filled in, so the response says what was used. A request without it has no such member and its response is what it was before.

| Request | Groups that apply |
|---|---|
| `composer_api_random_motive` | `motif`, for the segments strategy (the drawn strategy keeps its constants). `melody` and `development` are accepted and echoed. |
| `composer_api_theme` | `melody` and `development` (the fidelity minimums, also shown in the `minimum` of each development item). `motif` is accepted, echoed and has no effect: a theme's motives come from the request. |

`composer_api_validate` reads the member for `theme` and `random_motive` and refuses a bad one with its path. The vocabulary serves the default document as `preferences` and the limits as numbers in `limits.preferences` (`weight_max`, `pickup_one_in_max`, `leap_min`, `leap_max`, `range_low_min`, `range_low_max`, `range_high_max`, `range_span_min`, `density_max`, `scale_max`, `climax_rise_min`, `climax_rise_max`, `ratio_denominator_max`, `fidelity_denominator_max`, `attempts_max`, `keep_max`). The piece, fill, grow, regenerate and variations requests take no `preferences` member yet; a setting is promoted to them when it earns it (R-411), and a plan does not store the document.
