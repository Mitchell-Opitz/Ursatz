# Repository structure

The source directories of the Composer, the CLI snapshot tests and the file scope state check. Back to the [docs index](README.md).

| Directory | Holds |
| --- | --- |
| `include/ursatz_composer/` | The one public header, `composer_api.h`. |
| `src/include/ursatz_composer/` | The private headers. |
| (`library/plan/`) | The piece plan, its lanes, states, constraints and JSON: the library's plan group. |
| `src/util/` | Module `util`: private helpers the sources share: `util.h` (numbers, strings, reading a file), `position.h`, `shaping.h`, jobs, JSON; `composer.c` (the composition key). Not installed. |
| `src/structure/` | Module `structure`: plan queries (`plan_util.h`), request parsing, locks and dependents, fill targets, the form documents (`form_spec.c`, `form_builtin.c`) and the form record, the theme shape table (`theme_shape.c`). |
| `src/fill/` | Module `fill`: the fill loop, its operators, seeds, candidates, suggestions and records. |
| `src/render/` | Module `render`: rendering a plan to NoteEvents, making a work of it, MIDI and MusicXML. |
| `src/style/` | Module `style`: style models, blending and labels. |
| `src/harmony/` | Module `harmony`: the harmony operators, hearing cadences, the chord masks and the seam hearing. |
| `src/motive/` | Module `motive`: motives, their analysis and development, random motives (`motive_random*.c`: rhythm, layout, pitch, segments), the variant registry. |
| `src/melody/` | Module `melody`: the melody operator. |
| `src/texture/` | Module `texture`: the accompaniment and four-part voicing operators. |
| `src/counterpoint/` | Module `counterpoint`: the counter-melody operator: species 1 to 4 against a given line, its search and its check. |
| `src/material/` | Module `material`: the returns, variations and figures of a section, given material, connectives and the seam. |
| `src/grow/` | Module `grow`: growing a theme from a motive: the request (`grow_theme.c`), the ideas, the contrast, the draws and the apply, the structure operator and its checks. |
| `src/check/` | Module `check`: hearing a render through the library chain (readings), the critic (`analyzer_check.c`, form, key and return hearing), the style check and measure. |
| `src/piece/` | Module `piece`: the one piece builder `form_build_plan`, keys and cadences from a form, the compose run over its sections and the stages of `grow`. |
| `src/candidates/` | Module `candidates`: candidates and variations of a plan. |
| `src/explain/` | Module `explain`: the explanation report of a run. |
| `src/api/`, `src/preferences/` | Module `request`: the composer API behind `composer_api.h`, one file for each request kind (the grow toolbox in `api_style`, `api_seed`, `api_construction`, `api_grow`, `api_grow_json`; the piece entry in `api_piece*.c`), and the preferences document: its settings, reading with paths, and writing. |
| `forms/` | The built-in forms as JSON documents, embedded at build time (`cmake/EmbedForms.cmake`); [forms.md](forms.md). |
| `cli/` | The `ursatz-composer` command: a thin wrapper over the API. It includes `composer_api.h`, cJSON and the C standard library only, and the layer table (`cmake/layer-table.txt`) fails on any other include. |
| `tests/` | Tests and their fixtures. `tests/support/` is the `composer_test_support` library the tests share: file readers, the hearing wrappers, the check oracles the operators' rules define (`test_oracles.c`: the harmony, melody and figure lane checks, the cadence hearing) and the voice-leading hearing (`voice_hear.c`), none of which the library carries; `tests/snapshots/` holds the CLI snapshots. |
| `schema/` | `api/`, the JSON Schemas of the composer API requests and responses; `test_plan_schema` validates the snapshots' plan files and every fixture written back when python3 with `jsonschema` is installed. |

## CLI snapshots

`test_cli_snapshots` (`tests/cli_snapshots.cmake`) runs the seeded commands on the fixtures and compares each output with
`tests/snapshots/<case>.<ext>`. The values of `analyzer_version`, `composer_version` and `ursatz_commit` (the producer key) are ignored; files
ending `.json` are compared as JSON values (member order and indentation do not matter, `test_cli_snapshot_compare` proves
it); MIDI, MusicXML and text are compared byte for byte. Comparing runs only the CLI front end: the cases no API test
reads and one case per command the API tests cover (`COMPARED_CASES` in the script; the refusal cases always run); the
API tests compare the rest of each command's output against the same files. After a change that is meant to move output,
recapture and review the diff with the target that updates every golden (it runs every case):

```sh
cmake --build build --target update-goldens
```

See "Goldens" in the root `README.md`.

The GUI runs the CLI per request, so the cases below lock every shape it uses (fixtures in `tests/fixtures/gui/`). They must
not move while the CLI is moved onto the composer API:

| GUI shape | Cases |
| --- | --- |
| version | `version` |
| motive analyze (notes, rhythm form), degrees to notes, MIDI to motive, render | `motive-notes`, `motive-form-rhythm`, `motive-degrees`, `motive-midi`, `motive-render` |
| operation list, variants, recognize, random motive | `variants-list-ops`, `variants-basic`, `recognize`, `random-motive-style` |
| theme (JSON, plan, MIDI, MusicXML) | `theme-one-motive`, `theme-period-contrast`, `theme-chords`, `theme-fast-minor` |
| piece compose, regenerate, regenerate one lane, variations | `piece-theme-fill`, `piece-loop-and-ending`, `piece-progression-fill`, `piece-regenerate`, `piece-regenerate-lane`, `piece-variations` |
| refusals (stderr and exit status) | `error-motive-meter` (1), `error-theme-key` (1), `error-piece-type` (1), `error-theme-unsatisfiable` (2), `error-theme-write` (3) |

## File scope state

`tools/ci/check-statics.sh composer` fails when a file in `src/` declares a file-scope static variable that is not `const`, so the
library is safe to link in process. Pass state through the caller's context. CI runs it after the build.
