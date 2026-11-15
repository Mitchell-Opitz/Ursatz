# Keys claims

Claims about key regions, tonicizations, modulations and pivot chords in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Keys

`keys` is an object, not a claim:

| Field | Type | Meaning |
| --- | --- | --- |
| `regions` | array of key regions | The local keys, in time order, gap-free. Empty when no key was found. |
| `home_key` | object or null | The piece's home key, derived from the regions; `null` with no regions. |
| `opening_region` | integer or null | Index into `regions` of the first region. |
| `closing_region` | integer or null | Index into `regions` of the last region. |

The regions come from Ursatz's key-region detection (`key_regions_estimate`, in the
`common-practice-minimal` framework): windowed correlation of the weighted pitch classes against the
24 major and natural-minor keys, a path through the windows that charges for each key change, and
regions merged from neighbouring measures with the same key. The analyzer adds no key logic of its
own. A key signature the file notates is passed to the library as a boundary: the region that starts
there is restricted to the signature's major key and its relative minor.

A key region is a claim, plus:

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | The framework id, e.g. `"common-practice-minimal-v1"`. |
| `span`, `location` | | The region, barline to barline. `span.end` is exclusive. |
| `note_ids` | array of integers | The notes that start in the region. |
| `label` | string | The key, e.g. `"C major"` or `"Eb major"`. Flats are used when the tonic needs fewer flats than sharps (minimal-accidental spelling); the one tie, F#/Gb, reads in sharps. Every region label is spelled this way. |
| `uncertainty` | number | The library's uncertainty for the region, in [0, 1]. Higher is LESS certain: 0 when the chosen key beats every other by a full unit, 1 when another key fits as well or better. This is the opposite sense from a chord claim's `uncertainty`, which carries the library's confidence (see Chord claims). |
| `index` | integer | The region's position in `regions`. |
| `tonic` | string | The tonic's pitch class name, e.g. `"Eb"`. |
| `mode` | `"major"` or `"minor"` | The mode. Minor is the profile's minor (raised sixth and seventh weigh in); chord identification treats it as natural minor. |
| `measures` | object | `{"first", "last"}`: the measure numbers the region covers, inclusive. |
| `source` | `"profile"` or `"key_signature"` | `"key_signature"` when the region begins at a notated key signature and was restricted to its two keys; `"profile"` otherwise. |

`home_key` is `{"rule", "label", "region_indices", "duration"}`. The library offers no determination of
its own, so the rule is `"longest_total_duration"`: the key (tonic and mode) with the greatest total time over
all the regions that are in it, a tie going to the key that appears first. `duration` is that total, a rational
in whole notes, and `region_indices` lists those regions in time order. Totalling over a key's regions, not
taking the single longest region, keeps a key the detection splits into several regions from being outweighed
by one long stretch of another. The rule is recorded so a consumer can tell which applied should another be
added. The home key is not necessarily the opening or the closing key.

## Tonicizations, modulations and pivot chords

The library classes the change between each pair of neighbouring key regions, and the analyzer reports
what it establishes without adding a rule of its own. It reads the extent from the length of the region
the change goes INTO and whether the key then returns:

- shorter than four measures and followed by a return to the previous key: a tonicization;
- longer, and followed by a return: a weak modulation;
- not followed by a return: a modulation.

That is the extent from region length alone. The analyzer then confirms it with the cadences found
(R-044, `harmony_key_change_confirm`): a change into a new key is a modulation only when a cadence arrives in
the new region. A cadence inside a region the length rule calls a tonicization makes it a weak modulation; no
cadence inside makes any change a tonicization, however long. `extent` in the claims is the confirmed extent,
and `cadence_confirmed` says a cadence confirmed it. The change back out of an excursion (`is_return`)
carries the extent of the excursion it closes. The library's extended tonicization (it needs a chord count) is
not produced today, and would be listed under `tonicizations`.

`tonicizations` holds one claim per tonicization, in time order. The return out of a tonicization is part
of it and is not listed again. Beyond the claim fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The excursion: the stretch in the target key. |
| `label` | string | `"tonicization of G major"`. |
| `uncertainty` | null | The library supplies none. |
| `extent` | string | `"tonicization"` (or `"extended-tonicization"`), as confirmed by the cadences. |
| `cadence_confirmed` | boolean | Optional. Always `false` here: no cadence arrives in the target region. |
| `from_region`, `target_region` | integer | Indices into `keys.regions`: the key it leaves and the key it tonicizes. |
| `from_key`, `to_key` | string | Those regions' labels. |

`modulations` holds one claim per change the library gives a modulation extent, in time order, each a
lasting new key (a weak modulation goes right back to the home key). A return from an excursion that
was itself a modulation is listed, with `is_return` true; the return out of a tonicization is not.

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The passage in the new key. `location.start` is where it arrives. |
| `label` | string | `"C major to G major"`. |
| `uncertainty` | null | The library supplies none. |
| `extent` | `"weak-modulation"` or `"modulation"` | The library's extent, as confirmed by the cadences. |
| `cadence_confirmed` | boolean | Optional. `true` when a cadence arrives in the new region (a return carries the excursion's value). |
| `from_region`, `to_region` | integer | Indices into `keys.regions`. |
| `from_key`, `to_key` | string | Those regions' labels. |
| `type` | `"pivot"` or `"direct"` | The library's classification: `"pivot"` when it found a pivot chord, `"direct"` when it found none. It does not classify further (common-tone modulation, for one, is not distinguished). |
| `is_return` | boolean | The change goes back to the key before an excursion. |
| `pivot_chord_index` | integer or null | Index into `pivot_chords`; `null` for a direct modulation. |

`pivot_chords` holds one claim per pivot chord, in time order: the last segment before the new key
arrives, within two measures, whose chord is diatonic in both keys. The library's collection for a
minor key is its relative major's, so a chord needing the raised sixth or seventh does not count.
The chord is identified by the library's weighted identification in the OLD key (a minor old key is read in
its relative major, as the library does) and named without a Roman numeral.

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The chord's segment. |
| `label` | string | The root and quality, e.g. `"D minor"`. |
| `uncertainty` | number or null | The library's confidence in the chord, as for a chord claim. |
| `modulation_index` | integer | Index into `modulations`. |
| `chord_index` | integer | Index into `chords` of the same segment. That entry is read in the key of the region the segment starts in, which can differ from the old key if the pivot is the segment that starts at the boundary. |
| `root`, `quality`, `bass` | string | As for a chord claim. |
| `inversion` | integer or null | As for a chord claim. |
| `from_key`, `to_key` | string | The two keys the chord belongs to. |
