# Command reference

Every command of the workshop, as Ursatz Text and as the call `ursatz apply` and the GUI send. Generated from `workshop/commands/*.json` (tool `commands`, `ursatz help commands --format markdown`): do not edit. [Docs index](README.md).

## The text form

- One command a line: `<command> [<kind>:<id>[@v<n>]] {<param> <value>}`.
- The target is what the command works on, written `<kind>:<id>`: item, piece, collection, style, links, project, subject or job.
- A command that changes an item that has versions names the version you read: `item:<id>@v<n>`.
- A value is one token: a word, a number, `true`, `false`, `null`, `n/d`, a `"quoted string"` or a `[list of words]`.
- An object param is written below the command, two spaces in, one member a line: `key value` (nested members go in two more).
- A command that takes a file ends with: `file <path>`.
- A line starting with `#` is a comment; blank lines are ignored; the header `ursatz-text 1 commands` is allowed first.
- Params in `[brackets]` are optional; `one|two` lists the words an enum takes.

## Collection

Collections: named sets of library pieces.

| Command | Target | Params | Does |
|---|---|---|---|
| `collection.create` | none | `name string [description string] [id string]` | Make a collection of library pieces |
| `collection.update` | `collection:<id>` | `[name string] [description string]` | Rename a collection or change its description |
| `collection.delete` | `collection:<id>` | none | Delete a collection (its pieces stay) |
| `collection.add-pieces` | `collection:<id>` | `[piece_id string] [piece_ids list]` | Add library pieces to a collection |
| `collection.remove-piece` | `collection:<id>` | `piece_id string` | Take a piece out of a collection |

## Item

Items: the motifs, themes and pieces of a project, their versions and compositions.

| Command | Target | Params | Does |
|---|---|---|---|
| `item.create` | none | `type motif\|theme\|piece name string project_id string body object [note string]` | Make a motif, theme or piece with its first version |
| `item.update` | `item:<id>@v<n>` | `[name string] [project_id string]` | Rename an item or move it to another project |
| `item.delete` | `item:<id>@v<n>` | `[force bool]` | Delete an item and its variants |
| `item.save` | `item:<id>@v<n>` | `body object [note string]` | Save the next version of an item |
| `item.reanalyze` | `item:<id>@v<n>` | none | Analyze a motif again and save the result as a version |
| `item.compose` | `item:<id>@v<n>` | `[seed int] [update_uses bool] [settings object] [overrides list] [off list] [taste string]` | Compose a theme or piece into a new version |
| `item.explore` | `item:<id>@v<n>` | `[distance int] [seed int] [settings object] [overrides list] [off list] [taste string]` | Compose a theme with some settings moved away from the taste |
| `item.regenerate` | `item:<id>@v<n>` | `version int section_id string [lane string] [seed int]` | Write one section of a composed piece again as the next version |
| `item.variations` | `item:<id>@v<n>` | `version int section_id string lane string count int [seed int]` | Make variations of a section of a composed piece |
| `item.choose` | `item:<id>@v<n>` | `version int set int index int` | Make one variation of a set the next version |
| `item.set-op-checks` | `item:<id>@v<n>` | `checks object` | Replace the marks of Try every operation |
| `item.save-variant` | `item:<id>@v<n>` | `variant object [name string] [flag approved\|rejected\|unflagged]` | Save one candidate of the variants run as a derived motif |
| `item.flag` | `item:<id>@v<n>` | `flag approved\|rejected\|unflagged [kind exact\|motive-form\|variant]` | Flag a derived motif as approved, rejected or unflagged |
| `item.promote` | `item:<id>@v<n>` | `name string project_id string [note string]` | Save a variant as a motif of its own |
| `item.link-form` | `item:<id>@v<n>` | `source object recognition object` | Record a motif as a form of another motif |
| `item.reroll` | `item:<id>@v<n>` | none | Run a derived motif's chain again with a new seed |
| `item.restore` | `item:<id>@v<n>` | `version int [note string]` | Make the next version a copy of an older one |
| `item.update-use` | `item:<id>@v<n>` | `source_id string` | Move an item's use of a source to the source's current version |

## Job

Jobs: the long work the workshop runs for a command.

| Command | Target | Params | Does |
|---|---|---|---|
| `job.cancel` | `job:<id>` | none | Ask a job to stop |

## Library

Library pieces: import, change, delete and analyze again the pieces a person uploaded.

| Command | Target | Params | Does |
|---|---|---|---|
| `library.import` | none | `[filename string] file <path>` | Import a MIDI or MusicXML file as a library piece |
| `library.update` | `piece:<id>` | `[filename string] [composer string] [source string] [tags list] [add_tags list] [remove_tags list] [notation object]` | Change a piece's file name, composer, source, tags or notation |
| `library.delete` | `piece:<id>` | none | Delete a library piece and its files |
| `library.reanalyze` | `piece:<id>` | none | Analyze a library piece again from its stored source |

## Links

Motive links: runs that find the motives pieces share.

| Command | Target | Params | Does |
|---|---|---|---|
| `links.run` | none | `scope object` | Find the motive links between pieces |
| `links.delete` | `links:<id>` | none | Delete a link run and its files |

## Project

Projects: the folders that hold items.

| Command | Target | Params | Does |
|---|---|---|---|
| `project.create` | none | `name string [description string]` | Make a project |
| `project.update` | `project:<id>` | `[name string] [description string]` | Rename a project or change its description |
| `project.delete` | `project:<id>` | none | Delete a project |

## Ratings

Ratings: listening sessions, ratings and picks, item ratings, critiques and verdicts, and keep as default.

| Command | Target | Params | Does |
|---|---|---|---|
| `ratings.session` | none | `what motifs\|themes [taste string] [style string] [note string]` | Open a listening session |
| `ratings.batch` | none | `session string round int batch string samples list` | Record a batch of samples in a session |
| `ratings.rate` | none | `session string round int batch string seed int rating int [note string]` | Rate a sample of a batch from 1 to 5 |
| `ratings.pick` | none | `session string round int seed int better string [note string]` | Say which batch of a round sounded better |
| `ratings.rate-version` | `item:<id>@v<n>` | `version int rating int [note string]` | Rate a version of an item from 1 to 5 |
| `ratings.critique` | `item:<id>@v<n>` | `version int [settings object] [note string]` | Judge a version of a motif or a theme with the critic |
| `ratings.verdict` | `item:<id>` | `observation string verdict agree\|disagree [note string]` | Give a verdict on an observation of the critic |
| `ratings.keep` | none | `taste string key string ratings list [value value] [note string]` | Keep a setting as the default of a taste profile, citing the ratings behind it |

## Section

Sections: add, remove, move, resize and set the sections of a piece version's request.

| Command | Target | Params | Does |
|---|---|---|---|
| `section.insert` | `item:<id>@v<n>` | `id string [after string] [before string] [bars int] [beats int] [role intro\|body\|turnaround\|ending] [letter string] [function transition\|coda] [cadence PAC\|IAC\|HC\|DC\|PC] [pickup n/d] [fill empty\|motif] [region object] [harmony object] [note string]` | Add a section to a piece's request after or before another |
| `section.delete` | `item:<id>@v<n>` | `id string [note string]` | Remove a section from a piece's request |
| `section.move` | `item:<id>@v<n>` | `id string [after string] [before string] [note string]` | Move a section after or before another |
| `section.resize` | `item:<id>@v<n>` | `id string [bars int] [beats int] [note string]` | Change the length of a section |
| `section.set` | `item:<id>@v<n>` | `id string [role intro\|body\|turnaround\|ending] [letter string] [function transition\|coda\|none] [cadence PAC\|IAC\|HC\|DC\|PC\|none] [pickup n/d] [fill empty\|motif] [region object] [harmony object] [note string]` | Change the role, letter, function, cadence, pickup, fill, region or harmony |

## Settings

Settings: the values, overrides and switches of a style, project or item, and the taste profile it uses.

| Command | Target | Params | Does |
|---|---|---|---|
| `settings.set` | none | `scope string key string value value` | Set one setting in a scope |
| `settings.unset` | none | `scope string key string` | Take one setting out of a scope |
| `settings.import` | none | `scope string document object [replace bool] [note string]` | Set the settings of a document in a scope |
| `settings.override` | none | `scope string rule string action off\|reverse\|reweight\|waive\|clear [weight int] [note string]` | Waive, reverse, reweight or switch off a rule in a project or item |
| `settings.switch` | none | `scope string off list` | Switch scopes off for a project or an item |
| `settings.use-taste` | none | `scope string [taste string]` | Name the taste profile a style, project or item uses |

## Style

Styles: style models built from library pieces or a brief.

| Command | Target | Params | Does |
|---|---|---|---|
| `style.build` | none | `[name string] [filter object] [collection_id string] [piece_ids list] [brief object]` | Build a style model from pieces, a collection, a filter or a brief |
| `style.rebuild` | `style:<id>` | none | Build a style again from its source |
| `style.delete` | `style:<id>` | none | Delete a style |

## Subject

Subjects: the characters, places, ideas and notes items and pieces are linked to.

| Command | Target | Params | Does |
|---|---|---|---|
| `subject.create` | none | `name string kind character\|place\|idea\|note [note string]` | Make a subject |
| `subject.update` | `subject:<id>` | `[name string] [kind character\|place\|idea\|note] [note string]` | Change a subject's name, kind or note |
| `subject.delete` | `subject:<id>` | none | Delete a subject |
| `subject.link-item` | `subject:<id>` | `item_id string` | Link a subject to an item |
| `subject.unlink-item` | `subject:<id>` | `item_id string` | Take a subject's link to an item away |
| `subject.link-piece` | `subject:<id>` | `piece_id string` | Link a subject to a library piece |
| `subject.unlink-piece` | `subject:<id>` | `piece_id string` | Take a subject's link to a library piece away |
| `subject.relate` | `subject:<id>` | `other_id string` | Relate a subject to another subject |
| `subject.unrelate` | `subject:<id>` | `other_id string` | Take a subject's relation to another subject away |
