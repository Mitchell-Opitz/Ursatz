# Tools

The third call a program makes to the workshop: work on files the caller names, with no data folder. [Docs index](README.md).

## The call

```c
UrsatzStatus workshop_tools_open(Workshop **out, UrsatzError *);
UrsatzStatus workshop_tool(Workshop *, const char *name, const cJSON *params, const WorkshopBytes *bytes, WorkshopReply *, UrsatzError *);
```

A **tool** works on the files its params name (requests, plans, motive and score files, corpus folders). It never reads or writes the data folder or the journal, writes only into an output its params name, is not journaled and takes no actor. The reply is a `WorkshopReply` like a query's: `json` (a tree the caller owns) and, for a tool that makes a file, `bytes`. A NULL `params` is `{}`.

An unknown name is `URSATZ_ERR_REQUEST`, path `/name`, `unknown tool '<name>'`.

## The tools workshop

`workshop_tools_open` opens a workshop with no data folder: it builds the knowledge snapshot and nothing else (no lock, recovery, services, jobs, events, and no change of directory).

- It answers tools. `workshop_query`, `workshop_command` and `workshop_start` are refused with `URSATZ_ERR_REQUEST`, `this workshop has no data folder`, path `""`.
- `workshop_status` gives `data_dir` null.
- One process holds one workshop at a time: `workshop_tools_open` while another workshop is open is `URSATZ_ERR_CONFLICT`, `a workshop is already open in this process`.
- A workshop opened on a data folder answers tools too, started or not.
- `workshop_close` frees it.

## The tools

| Tool | Params | Reply |
|---|---|---|
| `composer` | `entry`, `request`, `kind` | `{json, plan_json, musicxml}`; MIDI as the bytes |
| `corpus-passages` | `dir`, `like` or `numerals`, `limit` | `{passages: [...]}` |
| `pitch-names` | `midi` | `{names: [...]}` |
| `analyze` | `path`, `input_format`, `meter`, `value_scale`, `pickup`, `layers`, `model_plan`, `text` | `{warnings, document, document_path, ...}` |
| `corpus` | `dir`, `out`, `resume` | `{pieces, analyzed, skipped, failed, out}` |
| `style-profile` | `dir`, `name`, `out` | `{path, name, pieces, stale}` |
| `versions` | none | the members of `workshop_status` except `data_dir` |

### composer

One Composer API entry, called with the workshop's knowledge snapshot and no job. `entry` is a string, required; `request` is an object, required except for `versions`, `vocabulary` and `variant_operations`; `kind` is a string and only `validate` reads it (it is required there).

The entries: `export`, `motive`, `motive_from_midi` (the bytes are the MIDI file; none, or an empty file, is `URSATZ_ERR_REQUEST`, path `/bytes`), `motive_render`, `variant_operations`, `variants`, `recognize`, `random_motive`, `theme`, `piece`, `piece_variations`, `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`, `progression`, `counterpoint`, `develop`, `grow`, `vocabulary`, `versions` and `validate`.

The reply's `json` member is `{"json": <text>, "plan_json": <text>, "musicxml": <text>}`, each a string exactly as the Composer wrote it, or null when the response has none. The MIDI file is the reply's bytes (`audio/midi`). `vocabulary` answers the cached text.

A refusal answers the Composer's status and error unchanged: the path points into the request (for example `/preferences/melody/keep`), never prefixed with `/params`. An unknown entry is `URSATZ_ERR_REQUEST`, path `/params/entry`.

### corpus-passages

The passages of a corpus output folder that look like a passage file or a progression. `dir` is the folder; exactly one of `like` (a MIDI or MusicXML file) and `numerals` (Roman numerals); `limit` is 1 to 100 (default 10). The array is what `analyzer_corpus_passages_to_json` writes, empty when nothing matches.

Failures: an unreadable `like` (or unreadable `numerals`) is `URSATZ_ERR_REQUEST`, path `/params/like` (`/params/numerals`); a corpus folder that does not read is `URSATZ_ERR_NOT_FOUND`, path `/params/dir`.

### pitch-names

`midi` is an array of whole numbers from 0 to 127, at most 4096. Each name is the sharp spelling of the pitch class and the octave: 60 is `C4`, 61 `C#4`, 127 `G9`. A value out of range is `URSATZ_ERR_REQUEST`, path `/params/midi/<i>`.

### versions

What `workshop_status` answers, without `data_dir`: `composer_version`, `api_version`, `producer_key`, `composition_key`, `plan_format_version`, `analyzer_version`, `analyzer_producer_key`.

### analyze

One file analyzed as the Analyzer's command line did, with the outputs as text in the answer (the tool writes no file). `path` is required and is used as given: the document's `source` names it as the command line's did. `input_format` is `auto`, `midi` or `musicxml`. The notation reading: `meter` (`3/4`), `value_scale` (`2`, `1/2`) and `pickup` (`1/4`). `layers` and `model_plan` are booleans. `text` is an object `{first_measure, last_measure, title}`, each member optional (0 and 0, the default, export the whole piece).

The reply: `warnings` (the importer's warning lines); without `text`, `document` (the analysis document's text) and `document_path` (where the command line wrote it: `<stem>.analysis.json` beside the input); with `layers`, `realization` and `reading` (texts); with `model_plan`, `plan` (the plan file's text, producer `ursatz-analyzer`); with `text`, `text` (the text export) and no document.

Failures: an import that fails is `URSATZ_ERR_FORMAT` with the importer's message, path `/params/path`; a notation reading that does not apply is `URSATZ_ERR_REQUEST`, `the pickup must be shorter than one measure of the meter` (path `/params/pickup`) or `a scaled time is too large` (`/params/value_scale`); an analysis that fails answers its status (`URSATZ_ERR_ALLOC` or `URSATZ_ERR_INTERNAL`), `the analysis failed`, path `""`; a text range with no measure is `URSATZ_ERR_REQUEST`, path `/params/text`; a model plan that cannot be made is `URSATZ_ERR_UNSATISFIABLE` with the reading's message.

### corpus

Analyzes every piece of the folder `dir` into `out` (default `<dir>/.ursatz-corpus`, the tool's one output); `resume` skips what is unchanged. The reply: `pieces`, one `{state, path, error?, import_message?, warnings}` per piece in run order (`state` is `analyzed`, `skipped` or `failed`), the counts `analyzed`, `skipped`, `failed`, and `out`. A run that fails answers the Analyzer's status and message.

### style-profile

The style model of a corpus output folder: `dir` (required), `name` (required, not empty) and `out` (default `<dir>/style-model.json`). The reply: `path`, `name`, `pieces` (how many pieces it counted) and `stale` (the analyzed pieces whose document another version wrote).
