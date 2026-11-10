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

`ComposerPreferences` holds `MelodyFillParams melody`. The motif and development groups are added with the stages that read them.

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
