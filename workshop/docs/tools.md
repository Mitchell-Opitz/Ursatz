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
| `commands` | `format`, `name` | `{format, text}` |
| `composer` | `entry`, `request`, `kind` | `{json, plan_json, musicxml}`; MIDI as the bytes |
| `corpus-passages` | `dir`, `like` or `numerals`, `limit` | `{passages: [...]}` |
| `pitch-names` | `midi` | `{names: [...]}` |
| `analyze` | `path`, `input_format`, `meter`, `value_scale`, `pickup`, `layers`, `model_plan`, `text` | `{warnings, document, document_path, ...}` |
| `corpus` | `dir`, `out`, `resume` | `{pieces, analyzed, skipped, failed, out}` |
| `style-profile` | `dir`, `name`, `out` | `{path, name, pieces, stale}` |
| `text` | `path` or `text`, `view`, `measures`, `section` | `{kind, text}` or `{kind, json}` |
| `versions` | none | the members of `workshop_status` except `data_dir` |

### composer

One Composer API entry, called with the workshop's knowledge snapshot and no job. `entry` is a string, required; `request` is an object, required except for `versions`, `vocabulary` and `variant_operations`; `kind` is a string and only `validate` reads it (it is required there).

The entries: `export`, `motive`, `motive_from_midi` (the bytes are the MIDI file; none, or an empty file, is `URSATZ_ERR_REQUEST`, path `/bytes`), `motive_render`, `variant_operations`, `variants`, `recognize`, `random_motive`, `theme`, `piece`, `piece_variations`, `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`, `progression`, `counterpoint`, `develop`, `grow`, `vocabulary`, `versions` and `validate`.

The reply's `json` member is `{"json": <text>, "plan_json": <text>, "musicxml": <text>}`, each a string exactly as the Composer wrote it, or null when the response has none. The MIDI file is the reply's bytes (`audio/midi`). `vocabulary` answers the cached text.

A refusal answers the Composer's status and error unchanged: the path points into the request (for example `/preferences/melody/keep`), never prefixed with `/params`. A refusal that carries an answer (a plan that cannot be completed lists what is stuck) fills the reply's `json` as well. An unknown entry is `URSATZ_ERR_REQUEST`, path `/params/entry`.

### corpus-passages

The passages of a corpus output folder that look like a passage file or a progression. `dir` is the folder; exactly one of `like` (a MIDI or MusicXML file) and `numerals` (Roman numerals); `limit` is 1 to 100 (default 10). The array is what `analyzer_corpus_passages_to_json` writes, empty when nothing matches.

Failures: an unreadable `like` (or unreadable `numerals`) is `URSATZ_ERR_REQUEST`, path `/params/like` (`/params/numerals`); a corpus folder that does not read is `URSATZ_ERR_NOT_FOUND`, path `/params/dir`.

### pitch-names

`midi` is an array of whole numbers from 0 to 127, at most 4096. Each name is the sharp spelling of the pitch class and the octave: 60 is `C4`, 61 `C#4`, 127 `G9`. A value out of range is `URSATZ_ERR_REQUEST`, path `/params/midi/<i>`.

### versions

What `workshop_status` answers, without `data_dir`: `composer_version`, `api_version`, `producer_key`, `composition_key`, `plan_format_version`, `analyzer_version`, `analyzer_producer_key`.

### analyze

One file analyzed as the Analyzer's command line did, with the outputs as text in the answer (the tool writes no file). `path` is required and is used as given: the document's `source` names it as the command line's did. `input_format` is `auto`, `midi` or `musicxml`. The notation reading: `meter` (`3/4`), `value_scale` (`2`, `1/2`) and `pickup` (`1/4`). `layers` and `model_plan` are booleans. `text` is an object `{first_measure, last_measure, title}`, each member optional (0 and 0, the default, export the whole piece).

The reply: `warnings` (the importer's warning lines); without `text`, `document` (the analysis document's text) and `document_path` (where the command line wrote it: `<stem>.analysis.json` beside the input); with `layers`, `realization` and `reading` (texts); with `model_plan`, `plan` (the plan file's text, producer `ursatz-analyzer`); with `text`, `text` (the text export framed as the reading view of the file: `ursatz-text 1 view reading <path>`, the export's own lines, then `# leaves out:` and `# expand:`) and no document.

Failures: an import that fails is `URSATZ_ERR_FORMAT` with the importer's message, path `/params/path`; a notation reading that does not apply is `URSATZ_ERR_REQUEST`, `the pickup must be shorter than one measure of the meter` (path `/params/pickup`) or `a scaled time is too large` (`/params/value_scale`); an analysis that fails answers its status (`URSATZ_ERR_ALLOC` or `URSATZ_ERR_INTERNAL`), `the analysis failed`, path `""`; a text range with no measure is `URSATZ_ERR_REQUEST`, path `/params/text`; a model plan that cannot be made is `URSATZ_ERR_UNSATISFIABLE` with the reading's message.

### corpus

Analyzes every piece of the folder `dir` into `out` (default `<dir>/.ursatz-corpus`, the tool's one output); `resume` skips what is unchanged. The reply: `pieces`, one `{state, path, error?, import_message?, warnings}` per piece in run order (`state` is `analyzed`, `skipped` or `failed`), the counts `analyzed`, `skipped`, `failed`, and `out`. A run that fails answers the Analyzer's status and message.

### style-profile

The style model of a corpus output folder: `dir` (required), `name` (required, not empty) and `out` (default `<dir>/style-model.json`). The reply: `path`, `name`, `pieces` (how many pieces it counted) and `stale` (the analyzed pieces whose document another version wrote).

### text

A file or a text of any layer kind as Ursatz Text, a view of it, or its JSON ([text](../../library/text/README.md)). Params: exactly one of `path` (a file) and `text`; `view` (default `full`: `summary`, `structure`, `harmony`, `line:<name>`, `part:<name>`, `reading`, `full` or `json`); `measures` (`{first, last}`, either may be absent) and `section` (the label or id of a section).

The input is JSON when its first non-space character is `{`, its kind from `format` or `schema` (`plan`, `reading`, `realization`, `knowledge`, else `json`); otherwise it is Ursatz Text and its kind is the header's (a plan, reading or realization text is read through its codec first). The reply is `{"kind": "<kind>", "text": "<the view>"}`, or for `json` `{"kind", "json": "<the codec's JSON text>"}`. `full` prints the layer; the other views read the one layer the file is.

Failures: both or neither of `path` and `text` is `URSATZ_ERR_REQUEST`, path `/params`; a file that does not read is `URSATZ_ERR_NOT_FOUND`, path `/params/path`; a text that does not parse is `URSATZ_ERR_REQUEST`, path `/params/text` (`/params/path` for a file), `line <n>: <message>`; a layer a codec refuses is refused at the same path with the codec's `<path>: <message>`; a view the layer cannot give is `URSATZ_ERR_REQUEST`, path `/params/view` (`harmony needs a plan or a reading`, `unknown view '<v>'`); an unknown section is refused at `/params/section` and a measure range outside the piece at `/params/measures`.

### commands

The command reference generated from the schemas in `workshop/commands/` ([command reference](command-reference.md)). `format` is `sheet` (the rules of the text form and a line a command, at most 120 lines; the default), `markdown` (the page `command-reference.md`), `grammar` (an EBNF of the text form: the generic productions and one a command), `json` (every command's schema as one array) or `help` (with `name`: the usage line, the summary, each param with its type, limits and help, the reply, the job, the base version and the example of one command). The reply is `{format, text}`. An unknown `name` is `URSATZ_ERR_NOT_FOUND`, path `/params/name`; an unknown `format` is `URSATZ_ERR_REQUEST`, path `/params/format`.
