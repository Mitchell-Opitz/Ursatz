# The Composer API

Everything an application asks of the Composer is a versioned in-process C API (R-219). The one public header is
`include/ursatz_composer/composer_api.h` (it includes only standard headers; every other Composer header is private in
`src/include/ursatz_composer/`, and Ursatz and the Analyzer are linked PRIVATE). A request is JSON text, a response is a `ComposerResponse`; no Composer type, no
cJSON type and no file path (except style models and corpus directories) crosses it. The CLI is a client of the same
functions: it reads its arguments into a request, calls the API and prints the response (R-267). Back to the [docs index](README.md).

This page covers what every entry shares, and indexes every entry: the core, export, the motive entries, variants, recognize, random motives, themes, pieces, the
plan workflow, the material commands (progression, counterpoint, develop, grow), the vocabularies and request validation.
Each entry has its request and response in a file under `docs/api/` (see [Entries](#entries)), an example in
[examples](api/examples.md), and its JSON Schemas under `schema/api/` (draft 2020-12). The CLI (`cli/`) includes no header but `composer_api.h` (`scripts/check-cli-api.sh`, run by CI).

A minimal client in C is in [Using the API from C](#using-the-api-from-c).

## Versioning

`COMPOSER_API_VERSION_MAJOR` and `COMPOSER_API_VERSION_MINOR`; `composer_api_version()` gives `"<major>.<minor>"`. This
is API 1.17 (1.0 was the first stable API, R-268). 1.17 adds to the settings document's `motif` group `syncopation_one_in`, `syncopation_types`, `syncopation_levels`, `skips_per_motif` and `plan`, to its `development` group `continuation` (`every_segment`, `fragment_lengths`, `diminish`, `liquidate`, `faster_harmony`) and `basic_as_written`, and to the vocabulary's `limits.preferences` `syncopation_one_in_max`, `skips_per_motif_max`, `plan_chords_max` and `plan_beat_max`; a given plan that does not fit the meter is refused by a random motive run at `/preferences/motif/plan`; every new member is off by default (R-422). 1.16 adds the section hearing, pedal and changes of piece rows and the forms entry's pedal. 1.15 adds to the settings document's `motif` group the five rhythm entries of `durations` (`quarter_beats`, `long_short`, `short_long`, `triplet`, `triplet_two_beats`), `compound_beats`, `endings`, `long_nct_one_in` and the four `long_nct_*` lists, and to the vocabulary's `limits.preferences` the `long_nct_one_in_max`; triplet values also read and write in the motive texts. 1.14 lets the piece entries compose, regenerate and vary a piece of any built-in form: the piece request takes `schema_version` 2 with `form`, `bars`, `count` and `modulate`, the request takes `musicxml_versions`, and the result names its versions (`versions`, `plans`, a variation's entry) by the form's version names (`piece` for a form with sizes) with the rows of its sections; a game loop request answers as before. 1.13 adds the `composer_api_forms` entry, which serves the built-in forms (their sections, sizes, cadences, keys and theme slots, and the roles of the game loop), and, in the vocabulary, `required` on each `theme_slots` entry and `beginning` and `ending` on the `piece_intos` entries that have one pairing. 1.12 adds nine optional listening settings to the `motif` group of the preferences document (`accented_one_in`, `accented_types`, `avoid_repeats`, `leaps_per_motif`, `leap_recovery`, `pickup_from_accent`, `contrast_fidelity_min`, `long_on_weak`, `final_long`; all off by default) and their two limits, `accented_one_in_max` and `leaps_per_motif_max`, to the vocabulary. 1.11 added the optional `preferences` member to the random motive and theme requests and responses, and the default `preferences` document and its limits to the vocabulary ([preferences.md](preferences.md)).

**Compatibility rule.** The MAJOR version changes on any removed or changed member or entry: an entry that goes, a request
or response member that goes, changes meaning or type, or a value a member took that is refused now. The MINOR version
changes on additions: a new entry, a new optional request member, a new response member, a new value. Within one major
version a client written against 1.0 keeps working with 1.n, if it ignores response members it does not know. The Composer
version (`composer_api_versions`, the project version) is separate: it follows the releases, not the API.

Every request may carry `"api_version": <major>`. A different major is refused with `COMPOSER_ERR_REQUEST` and the path
`/api_version`. A request without it is taken as the major of the library.

## Status

Every entry returns a `ComposerStatus`. The CLI maps it to its exit code.

| Status | Meaning | CLI exit |
|---|---|---|
| `COMPOSER_OK` | the response is filled | 0 |
| `COMPOSER_ERR_REQUEST` | the request is malformed or asks for something invalid | 1 |
| `COMPOSER_ERR_UNSATISFIABLE` | the request is valid but nothing satisfies it (a stuck run) | 2 |
| `COMPOSER_ERR_CANCELLED` | the job's `cancelled` callback returned true | none; the CLI has no cancel |
| `COMPOSER_ERR_ALLOC` | out of memory | 1 |
| `COMPOSER_ERR_INTERNAL` | a failure that is not the request's | 1 |

`COMPOSER_ERR_ALLOC` and `COMPOSER_ERR_INTERNAL` are 1 unless a command did otherwise before. A failure to write a file is
the CLI's own (the API writes none) and stays 3 where it was 3 (`export` has always given 1). `composer_status_name`
gives `"ok"`, `"request"`, `"unsatisfiable"`, `"cancelled"`, `"alloc"` and `"internal"`.

On any failure `ComposerResponse.error` holds `message` and `path`, the JSON Pointer of the request member at fault
(`"/tempo"`, `"/plan/form/children/1"`), or `""` when no member is at fault. Nothing else is set.

## Memory

An entry zeroes the `ComposerResponse` first and fills only the members it produces: `json`, `plan_json`, `midi` and
`midi_size`, `musicxml`. The caller owns them and frees them all with `composer_response_destroy`, which zeroes the
response; it is safe on a zeroed response and on one already destroyed. Strings are NUL-terminated; `midi` is `midi_size`
bytes. A request string is only read during the call.

## Jobs

`const ComposerJob *job` carries progress and cancel for the entries that take long. A NULL job, or a NULL member, means no
reports and no cancel.

- `progress(user, stage, done, total)` is called at the start of each stage and once per target of a fill loop. The stages
  are the compose stages (`"invent"`, `"structure"`, `"motives"`, `"harmony"`, `"bass"`, `"melody"`, `"texture"`,
  `"returns"`, `"seam"`, as `done` the stage's index of `total` = 9), `"fill"` (`done` targets attempted of the iteration
  limit; a fill loop that starts again, as the melody does from the next seed, begins at 0), `"grow"` (one candidate) and
  `"export"`.
- `cancelled(user)` is asked at the same points. When it returns true the run stops there, frees what it built and the
  entry returns `COMPOSER_ERR_CANCELLED`. The plan the run was filling is left as far as it got and is not returned.
- Both are called from the calling thread only.

In the library, `FillConfig`, `ComposeConfig` and `GrowConfig` carry `const ComposerJob *job` (NULL by default); a cancel is
`FillOutcome.cancelled`, `ComposeOutcome.cancelled` and `GrowOutcome.cancelled` with the module's existing failure value
(`FILL_ERR_OPERATOR_FAILED`, `PIECE_ERR_STUCK`, `GROW_ERR_STUCK`). `compose_run` tries no further round and
`compose_candidates` no further candidate after a cancel. With no job nothing changes: the same seed gives the same plan.

The engine reports through the engine job hook in `src/util/job.h`: `job_step(job, stage, done, total)` calls `progress` and
returns whether the job is cancelled, and `job_cancelled(job)` asks `cancelled` alone. `fill_loop.c`, `compose_run.c`,
`grow_run.c` and the API files use these two and nothing else; `api/api_internal.h` is for `src/api/` only
(`scripts/check-layers.sh` in CI).

## Threading

No entry keeps state between calls or shares state with another: one request per call, and a caller may run requests on
several threads at once. A job's callbacks run on the thread that called the entry.

## Entries

Every entry has its request and response in the file named here and its JSON Schemas under `schema/api/` (draft 2020-12).

| Entry | File |
|---|---|
| `composer_api_versions` | [vocabulary-and-validation.md](api/vocabulary-and-validation.md) |
| `composer_api_export` | [motives.md](api/motives.md) |
| `composer_api_motive`, `composer_api_motive_from_midi`, `composer_api_motive_render` | [motives.md](api/motives.md) |
| `composer_api_variant_operations`, `composer_api_variants`, `composer_api_recognize`, `composer_api_random_motive` | [motives.md](api/motives.md) |
| `composer_api_theme` | [theme-and-piece.md](api/theme-and-piece.md) |
| `composer_api_piece`, `composer_api_piece_variations` | [theme-and-piece.md](api/theme-and-piece.md) |
| `composer_api_plan`, `composer_api_fill`, `composer_api_lock`, `composer_api_regenerate`, `composer_api_variations`, `composer_api_harmonize` | [plan-workflow.md](api/plan-workflow.md) |
| `composer_api_progression`, `composer_api_counterpoint`, `composer_api_develop`, `composer_api_grow` | [material-commands.md](api/material-commands.md) |
| `composer_api_vocabulary`, `composer_api_forms`, `composer_api_validate` (for a theme, it builds the plan and refuses what Compose refuses before seeding) | [vocabulary-and-validation.md](api/vocabulary-and-validation.md) |
| Examples of every entry | [examples.md](api/examples.md) |

## Error paths

An error never comes with a document, except the stuck report of `composer_api_fill` and `composer_api_regenerate`. What
the path says:

| Error | Status | `path` |
|---|---|---|
| text that is not JSON, or not an object | `COMPOSER_ERR_REQUEST` | `""` |
| `"api_version"` of another major | `COMPOSER_ERR_REQUEST` | `/api_version` |
| a member of the wrong type or value | `COMPOSER_ERR_REQUEST` | the member: `/tempo`, `/features/1`, `/input/format`, `/given/0/at` |
| a plan that does not read | `COMPOSER_ERR_REQUEST` | `/plan` and the place in the plan: `/plan/form/children/1/span` |
| a section or node the plan does not have | `COMPOSER_ERR_REQUEST` | `/section`, `/node` |
| an input that is a motive but not a usable one (a pickup the bar cannot hold, notes sounding together) | `COMPOSER_ERR_UNSATISFIABLE` | `/input/text` |
| a file the request names that cannot be read (a style, a corpus) | `COMPOSER_ERR_UNSATISFIABLE` | `/style`, `/corpus`, `/like` |
| a valid request no seed can satisfy | `COMPOSER_ERR_UNSATISFIABLE` | `""` |
| the job's `cancelled` returned true | `COMPOSER_ERR_CANCELLED` | `""` |

`message` is for a person: it names the member by its request name (`tempo must be from 20 to 400`). A client that shows the
error next to a form field uses `path`.

## Job rules

The stage each entry reports at the start (`done` of `total`), besides the compose stages of the Jobs section:

| Entry | Stage |
|---|---|
| `composer_api_export` | `export` |
| `composer_api_motive`, `composer_api_motive_from_midi`, `composer_api_motive_render` | `motive` |
| `composer_api_variants` | `variants` (one step for each variant tried) |
| `composer_api_recognize` | `recognize` |
| `composer_api_random_motive` | `random-motive` |
| `composer_api_theme`, `composer_api_grow` | `grow` (one step for each seed tried) |
| `composer_api_piece`, `composer_api_fill`, `composer_api_regenerate` | `seed` (one step for each seed tried) |
| `composer_api_variations`, `composer_api_harmonize`, `composer_api_piece_variations` | `variations` |
| `composer_api_progression` | `progression` |
| `composer_api_counterpoint` | `counterpoint` |
| `composer_api_develop` | `develop` |

`composer_api_versions`, `composer_api_variant_operations`, `composer_api_vocabulary`, `composer_api_forms`, `composer_api_validate`,
`composer_api_plan` and `composer_api_lock` are quick: they report nothing and never return `COMPOSER_ERR_CANCELLED`. A
cancel is honored at the next report; the entry frees what it built and returns `COMPOSER_ERR_CANCELLED` with no document
and no plan. Progress and cancel run on the thread that called the entry: a client that shows progress from another thread
hands the value over itself.

## Using the API from C

Link `ursatz_composer` (add this repo with `add_subdirectory` and `target_link_libraries(app PRIVATE ursatz_composer)`) and
include the one header.

### Linking

An application that already has the `cjson`, `ursatz` and `ursatz_analyzer` targets adds those first and the Composer after
them; the Composer adds a target only when it is not there, so the tree keeps one copy of each (pin all four repositories to
the same commits, `scripts/check-pins.sh`):

```cmake
add_subdirectory(external/cjson)
add_subdirectory(external/ursatz)
add_subdirectory(external/ursatz-analyzer)
set(URSATZ_COMPOSER_BUILD_CLI OFF CACHE BOOL "")
add_subdirectory(external/ursatz-composer)
target_link_libraries(app PRIVATE ursatz_composer)
```

`URSATZ_COMPOSER_BUILD_CLI` (on only when the Composer is the top project) builds the `ursatz-composer` command; the tests that
run it are registered only when it is built. `tests/consumer/` is such an application, built by the `consumer_build` test.

This grows a theme from a motive and writes its MIDI file:

```c
#include <stdio.h>
#include <ursatz_composer/composer_api.h>

int main(void) {
    const char *request =
        "{\"schema\": \"ursatz-theme-request\", \"schema_version\": 2,"
        " \"into\": \"sentence\", \"key\": \"D\", \"mode\": \"minor\", \"meter\": \"4/4\", \"bars\": 8, \"cadence\": \"PAC\","
        " \"motives\": [{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:q 3:e 5:e 1':h\"}],"
        " \"slots\": {\"basic\": \"A\"}, \"seed\": 1, \"midi\": true}";

    ComposerResponse response;
    ComposerStatus status = composer_api_theme(request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s: %s (at '%s')\n", composer_status_name(status), response.error.message, response.error.path);
        composer_response_destroy(&response);
        return 1;
    }
    printf("%s\n", response.json);   /* the theme document */
    FILE *file = fopen("theme.mid", "wb");
    if (file) {
        fwrite(response.midi, 1, response.midi_size, file);
        fclose(file);
    }
    composer_response_destroy(&response);
    return 0;
}
```

## Tests

`tests/test_api_core.c`, `tests/test_api_motive.c` (each GUI motive shape from a request, the response equal to the CLI
snapshot), `tests/test_api_variants.c` (the operation list, variants, recognize and random motives against their
snapshots, the random draws for seeds 0, 1, 2 and 4294967295, refusals with their paths, cancel) and
`tests/test_api_theme.c` (every `theme-*` snapshot case from its request, with the document, the plan, the MIDI file and the MusicXML equal to the snapshots; cancel; two threads running two cases each; refusals with their paths),
`tests/test_api_piece.c` (every `piece-*` snapshot case from its request, with the document, the plans, the MIDI file and the MusicXML equal to the snapshots, the variations with their MIDI files through the export entry, the lanes of a regenerate, a rounded binary and variations composed, written again in a section and varied, cancel, refusals with their paths) and
`tests/test_api_plan_workflow.c` (the plan, fill, lock, regenerate, variations and harmonize cases of the CLI snapshots from their requests, the candidates, record and explanation, the report of a stuck plan, refusals with their paths, cancel),
`tests/test_api_material.c` (the progression, counterpoint and grow cases of the CLI snapshots from their requests, with the documents, the plans and the MIDI files equal to the snapshots; develop; the corpus seed; refusals with their paths; progress and cancel) and
`tests/test_api_forms.c` (the seven forms in registry order, the sizes, sections and slots of rounded binary, variations, layered and the game loop's roles) and `tests/test_api_vocabulary.c` (every vocabulary list non-empty and each value accepted by its reader, the default cadences of three piece structures, the parsed harmony, refusals with their paths) and
`tests/test_job_cancel.c`; `tests/validate_api_schema.cmake` validates every response the
tests write against its schema when python3 with `jsonschema` is present.
