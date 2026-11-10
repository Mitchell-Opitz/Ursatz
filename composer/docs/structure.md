# Repository structure

The source directories of the Composer, the CLI snapshot tests and the file scope state check. Back to the [docs index](README.md).

| Directory | Holds |
| --- | --- |
| `include/ursatz_composer/` | The one public header, `composer_api.h`. |
| `src/include/ursatz_composer/` | The private headers. |
| `src/plan/` | The piece plan, its lanes, states, constraints and JSON. |
| `src/render/` | Rendering a plan to NoteEvents, MIDI and MusicXML. |
| `src/fill/` | The fill loop, its operators, seeds and records. |
| `src/harmony/` | The harmony operators and hearing cadences. |
| `src/motive/` | Motives, their analysis and development. |
| `src/melody/` | The melody operator. |
| `src/preferences/` | The preferences document: its settings, reading with paths, writing and the default check. |
| `src/form/` | Hearing the phrase form through standard-form. |
| `src/counterpoint/` | The counter-melody operator: species 1 to 4 against a given line, its search and its check. |
| `src/texture/` | The accompaniment and four-part voicing operators, and hearing the voice leading. |
| `src/piece/` | Form templates, the piece plan and the compose run over its sections. |
| `src/grow/` | Growing a theme from a motive: the plan, the structure operator, its checks and the stages. |
| `src/check/` | Hearing a render through the Analyzer, the Analyzer check loop and the explanation report. |
| `src/style/` | Style models, blending, style checks and seeding from a corpus. |
| `src/api/` | The composer API behind `composer_api.h`: one file for each request kind. |
| `src/util/` | Private helpers the sources share: `util.h` (numbers, strings, reading a file) and `plan_util.h` (spans, lanes, subtrees, name tables). Not installed. |
| `cli/` | The `ursatz-composer` command: a thin wrapper over the API. It includes `composer_api.h`, cJSON and the C standard library only, and `scripts/check-cli-api.sh` (CI runs it) fails on any other include. |
| `tests/` | Tests and their fixtures. `tests/support/` is the `composer_test_support` library the tests share; `tests/snapshots/` holds the CLI snapshots. |
| `schema/` | `plan.v2.schema.json`, the JSON Schema of the plan file, and `api/`, the JSON Schemas of the composer API requests and responses; `test_plan_schema` validates the snapshots' plan files and every fixture written back when python3 with `jsonschema` is installed. |

## CLI snapshots

`test_cli_snapshots` (`tests/cli_snapshots.cmake`) runs the seeded commands on the fixtures and compares each output with
`tests/snapshots/<case>.<ext>`. The values of `analyzer_version`, `composer_version` and `ursatz_commit` are ignored; files
ending `.json` are compared as JSON values (member order and indentation do not matter, `test_cli_snapshot_compare` proves
it); MIDI, MusicXML and text are compared byte for byte. After a change that is meant to move output, recapture and review
the diff:

```sh
cmake -DCOMPOSER=build/cli/ursatz-composer -DWORK=build/tests/cli_snapshots -DSNAPSHOTS=tests/snapshots \
      -DFIXTURES=tests/fixtures -DUPDATE=1 -P tests/cli_snapshots.cmake
```

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

`scripts/check-statics.sh` fails when a file in `src/` declares a file-scope static variable that is not `const`, so the
library is safe to link in process. Pass state through the caller's context. CI runs it after the build.
