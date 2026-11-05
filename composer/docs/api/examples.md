# Examples

Examples of Composer API requests and responses, from runs of the API. Back to the [docs index](../README.md).

The examples come from runs of this API (the MIDI entry and the piece variations are written from `tests/test_api_motive.c` and `tests/test_api_piece.c`); a long value is shortened with `…`. A plan object is `{…}`: the plan file of the
command line, read as JSON (docs/plan-format.md).

## Versions

```
composer_api_versions(&out)
json: {"composer_version":"0.4.1","api_version":"1.1","analyzer_version":"0.6.0","ursatz_commit":"d2b85ce…","plan_format_version":2}
```

## Export

```
request: {"plan": {…}, "midi": true, "tempo": 120}
status:  COMPOSER_OK    midi, midi_size filled
json:    {"skipped":[{"span":3,"lane":"melody","reason":"source material"}, …]}

request: {"plan": {…}, "plan_file": true}
status:  COMPOSER_OK    plan_json filled with the plan file text, nothing rendered
```

## Motive, from MIDI and render

```
request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major"}
status:  COMPOSER_OK
json:    {"motive": {"meter": {"beats_per_bar": 4, "beat_unit": 4, …}, "key": {"tonic_pitch_class": 2, "mode": "major"},
          "notes": [{"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}}, …]},
          "analysis": {"notes_text": "D4:q E4:q F4:q G4:q", "degrees_text": "1:q 2:q #2:q 4:q", …}}

composer_api_motive_from_midi(bytes, size, "{\"key\": \"D minor\"}", NULL, &out)    the same document for a MIDI file

request: the motive request above plus {"render_key": "G minor", "tempo": 90, "musicxml": true}
json:    the document with "rendered": {"key": "G minor", "notes": [{"midi_pitch", "offset", "duration"}, …]}
         musicxml filled
```

## Variants, recognize and random motives

```
composer_api_variant_operations(&out)
json: {"operations": [{"name": "retrograde", "label": …, "category": …, "params": [], "forms": ["full"], …}, …]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major", "count": 2, "seed": 1}
json:    {"seed": 1, "source": {"motive": {…}, "analysis": {…}}, "variants": [{"seed": 1, "chain": {"steps": […]}, "kept": […],
          "changed": […], "derivation": "…", "motive": {…}}, …]}

request: {"source": {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}},
          "target": {"input": {"format": "notes", "text": "G4:q F4:q E4:q D4:q"}}, "meter": "4/4", "key": "D major"}
json:    {"found": true, "chain": {"steps": [{"op": "retrograde", "params": {}}]}, "distance": 0.3333, "remoteness": 0.3333,
          "kept": ["rhythm", "harmony"], "changed": ["intervals", "contour"],
          "derivation": "Put in retrograde. Kept the rhythm and the harmony; changed the intervals and the contour."}

request: {"mode": "major", "meter": "4/4", "seed": 7}
json:    {"seed": 7, "motives": [{"motive": {…}, "analysis": {…}}]}
```

## Theme and piece

```
request: a theme request v2 (theme-request.md) with "seed": 1, "suggest": 1, "midi": true
status:  COMPOSER_OK    midi, midi_size and plan_json filled
json:    {"key": "D minor", "meter": "4/4", "bars": 8, "into": "sentence", "pairing": "presentation + continuation",
          "cadence": "PAC", "themes": [{"seed": 1, "structure": […], "chords": […], "score": …}], …}

request: {"request": {"schema": "ursatz-piece-request", "schema_version": 1, "key": "D", "mode": "minor", "meter": "4/4",
          "sections": […]}, "seed": 1, "midi": true}
json:    {"schema": "ursatz-piece", "seed": 1, "key": "D minor", "loop": {"start": "2/1", "end": "16/1"},
          "versions": {"loop": {…}, "ending": {…}}, "plans": {"loop": {…}, "ending": {…}}}

request: {"request": {…}, "from": {…the loop plan…}, "variations": {"section": "a", "lanes": ["bass"], "count": 3}, "seed": 1}
json:    {"schema": "ursatz-piece-variations", "section": "a", "variations": [{"index": 1, "seed": 1, "loop": {…}, …}], "kept": 3, …}
```

## The plan workflow

```
request: {"form": "binary", "key": "C", "mode": "major", "meter": "4/4", "seed": 1}      composer_api_plan
json:    {"form": "binary", "bars": 16, "key": "C major", "sections": [{"letter": "A", "bars": 8, …}, {"letter": "B", …}], …}
         plan_json: the plan

request: {"plan": {…}, "seed": 1}                                                            composer_api_fill
json:    {"seed": 1, "realized": false, "sections": [{"letter": "A", "heard": "heard as sentence", "heard_as_requested": true,
          "cadences": ["half", "perfect-authentic"]}, …], "returns": {"planned": 0, "heard": 0, "pairs": []}}
         plan_json: the realized plan

request: {"plan": {…}, "section": "A"}                                                       composer_api_lock
json:    {"locked": {"nodes": 9, "spans": 19}, "what": "section A"}
         plan_json: the plan with the section Given

request: {"plan": {…}, "section": "A", "seed": 2}                                            composer_api_regenerate
json:    {"seed": 2, "realized": false, "reopened": {"sections": ["A"], "spans": {…}}, "sections": […], …}

request: {"plan": {…}, "section": "A", "count": 2, "seed": 1}                                composer_api_variations
json:    {"variations": [{"index": 1, "seed": 1, "plan": {…}, "sections": […]}, …], "kept": 2, "requested": 2, "seeds_tried": …}
```

`composer_api_harmonize` takes the request of `composer_api_variations` with a `section` whose melody is Given.

## The material commands

```
request: {"key": "C major", "meter": "4/4", "bars": 4, "cadence": "PAC", "seed": 1}         composer_api_progression
json:    {"key": "C major", "meter": "4/4", "bars": 4, "cadence": "PAC", "seed": 1, "heard": "perfect-authentic",
          "chords": [{"bar": 1, "beat": 1, "chord": "I", "function": "tonic", "bass": "C3"}, …]}

request: {"input": {"format": "notes", "text": "D4:w F4:w E4:w D4:w …"}, "species": 1, "key": "D", "mode": "minor",
          "meter": "4/4", "position": "above", "seed": 1}                                    composer_api_counterpoint
json:    {"species": 1, "position": "above", "key": "D minor", "meter": "4/4",
          "results": [{"seed": 1, "line": […], "counter_melody": […], "groups": […]}]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major",
          "op": "invert", "seed": 0, "suggest": 2}                                           composer_api_develop
json:    {"source": "D4:q E4:q F4:q G4:q", "operation": "invert", "seed": 0, "candidates": [{"index": 1,
          "label": "invert about the first note", "score_numerator": 5, "score_denominator": 8, "text": "D4:q C4:q B3:q A3:q"}, …]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D", "mode": "major",
          "into": "sentence", "bars": 8, "cadence": "PAC", "seed": 1}                        composer_api_grow
json:    {"key": "D major", "meter": "4/4", "bars": 8, "into": "sentence", "pairing": "presentation + continuation",
          "cadence": "PAC", "themes": [{"seed": 1, "structure": […], …}], …}
```

## Vocabularies and validation

```
composer_api_vocabulary(&out)
json: {"meters": […], "modes": […], "keys": […], …, "limits": {"motive_notes": 64, "motive_chords": 8, "motive_features": 8,
       "variant_chain": 4, "variant_functions": 4, "random_motive_count": 16, "tempo_min": 20, …}}

composer_api_validate("random_motive", "{\"mode\": \"random\", \"meter\": \"random\", \"seed\": 5}", &out)
json: {"request": {"seed": 5, "mode": "major", "meter": "4/4"}}
```
