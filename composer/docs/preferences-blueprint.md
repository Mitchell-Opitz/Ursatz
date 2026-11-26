# The blueprint preferences (P69c, R-750)

The `blueprint` group of the settings document: the weights the blueprint generator (`blueprint-generator.md`, from B3) and the blueprint motif strategy (B4) draw from. Code: `BlueprintPreferences` in `ursatz_composer/motive_blueprint.h`, read and written by `composer/src/preferences/preferences_blueprint.c`. The group is the seventh of the document and every object is closed: an unknown member is refused at its path (`/blueprint/jobs/fragment/walk`). Nothing reads the group until B3; with the defaults every output is what it was. The accompaniment is not a preference: a preset carries it (R-754).

| Key | Default | Limit | What it shapes |
| --- | --- | --- | --- |
| `sounds.ninth_downbeat` | 3 | 0 to 20 | A ninth (root + 2) on a bar's downbeat, against none. |
| `sounds.sus4_dominant` | 2 | 0 to 20 | A suspended fourth on the first half of a dominant bar. |
| `sounds.appoggiatura` | 3 | 0 to 20 | An appoggiatura on a bar's downbeat. |
| `sounds.major_seventh` | 1 | 0 to 20 | A major seventh on the longest note of a tonic or subdominant bar. |
| `sounds.leading_tone` | 2 | 0 to 20 | The leading tone into the final tonic. |
| `sounds.long_weak` | 2 | 0 to 20 | The extra weight of a rhythm cell holding a note of a beat or longer off the downbeat. |
| `jobs.<function>.<job>` | below | 0 to 20 | The weight of a rhythm job for a unit of an idea with that formal function. |
| `climax.m6`, `climax.m7` | 1, 2 | 0 to 20 | The weight of the sentence's climax in bar 5 (m6) or bar 6 (m7). Both 0 takes the last bar. |
| `range` | 16 | 12 to 24 | The semitones the melody spans below the climax. |
| `candidates` | 3 | 1 to 8 | The themes drawn for one request, of which the critic keeps one (B6). |

`jobs` holds four functions, each with the five jobs `running`, `long`, `syncopated`, `variant` and `contrast`:

| Function | running | long | syncopated | variant | contrast |
| --- | --- | --- | --- | --- | --- |
| `basic` | 2 | 3 | 1 | 2 | 1 |
| `fragment` | 3 | 0 | 2 | 1 | 1 |
| `contrasting` | 1 | 1 | 1 | 0 | 3 |
| `cadential` | 2 | 2 | 1 | 1 | 0 |

A weight of 0 never draws that job or sound. The 30 setting keys (`blueprint.sounds.ninth_downbeat` to `blueprint.candidates`) are soft and sit after the harmony keys in the key table ([preferences.md](preferences.md#the-key-and-rule-tables-r-672)). A value outside its limit is refused at its path (`/blueprint/range`), and the vocabulary's preference limits give `blueprint_range_min` 12, `blueprint_range_max` 24 and `blueprint_candidates_max` 8.
