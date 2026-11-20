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
