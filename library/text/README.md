# Text: Ursatz Text

`library/text` (target `ursatz_text`) is the compact, lossless text form of the library's objects (R-662, `spec/target-architecture.md` 8.3). A plan, a reading, a realization (with its scoring assignments), a knowledge file or a knowledge entry prints as lines a person can read and edit, and reads back to what the layer's own codec writes. Text is the form Claude and Michelle read and write; JSON stays the form on disk.

## The grammar (version 1)

A document is the line `ursatz-text 1 <kind>` and then the members of one JSON object, one line each. Indent is two spaces a level (a tab or an odd indent is refused). A line whose first non-space character is `#` is a comment; blank lines are ignored.

```
ursatz-text 1 plan
# a comment
format ursatz-plan
format_version 6
metadata producer ursatz-composer producer_version 5.0.0
```

The kinds are `json` (any object), `plan`, `reading`, `realization`, `knowledge`, `knowledge-entry`, `commands` (B6) and `view` (the views, below).

### Values

- `null`, `true`, `false`.
- A number, as `cJSON_PrintUnformatted` prints it: `3`, `-0.25`, `1e+20`.
- `n/d` for an object of exactly `num` and `den` (integers, `den` above 0): `3/4` for `{"num": 3, "den": 4}`. `{"num": 1, "den": 0}` and `{"den": 1, "num": 2}` stay objects.
- `a..b` for an object of exactly `start` and `end` that are both such rationals: `0/1..3/4`.
- A string: bare when it matches `[A-Za-z_][A-Za-z0-9_.:#@'+/-]*`, is not `null`, `true` or `false` and holds no `..` (`omt.material.major`, `F#4`); otherwise quoted as JSON (`"12"`, `"1/2"`, `"two words"`, `""`).
- `{}` is an empty object, `[]` an empty array, `[1 2 x]` an array of scalars.

### Members

A member is a key and what it holds. A bare key matches `[A-Za-z_$][A-Za-z0-9_$-]*` and is not a keyword; any other key is quoted (`"a b" 1`).

```
key value                one value: key 3, key "two words", key [1 2 3], key {}
key k1 v1 k2 v2          an object whose leading scalar members sit on the line
  k3 ...                 and its other members below it, two spaces in
key                      an object or an array below it
  k1 ...
key | c1 c2 a.b          a table: an array of objects with the same members, a row a line
  1 x 2/3 4
```

A member takes one value or pairs of key and value; three units after a key are refused ("a member takes one value or pairs of key and value"). A repeated member in one object is refused.

An array of two or more objects with the same members in the same order, each member a scalar, a scalar array or a one-level object of them, prints as a table: the header `key | c1 c2 a.b` names a column per member (`a.b` is member `b` of the object member `a`; column names have no dot), and each row is one object. A row with another cell count than the header is refused.

```
chords | id start end numeral
  c1 0/1 1/1 I
  c2 1/1 2/1 V
```

Any other array prints as items, one `- ` line each:

```
tags
  - first
  - k 1 l 2
    m
      n 3
  -
    - 1
    - 2
```

An item is `- value`, `- k1 v1 ...` (an object: its leading scalar members on the line, its other members below it, indented two more than the `-`) or `-` alone (an object or an array below it). An array of arrays prints as `-` with the inner items below it.

### Lossless

For a plan, reading or realization `x`, the codec's JSON of `parse(print(x))` is byte-identical to the codec's JSON of `x`. For a knowledge file or entry and for `json`, the trees compare equal (`cJSON_Compare` with case) and keep their member order. Scoring assignments have no codec of their own: the realization codec writes them inside a movement, so they travel in the realization's text. Test `text_tree` prints and reads back every JSON object of the repository's knowledge, fixtures, schemas and forms (396 files); test `text_layers` does the same through the codecs for every plan, reading and realization fixture.

### Views

A view is a printed summary of a layer, not a copy: it starts `ursatz-text 1 view <name> <address>` and ends with `# leaves out: ...` and `# expand: ...` lines, so a reader knows what is missing and how to ask for it. The views come with the next section of the phase (P65 B2).

## API

Headers `text/text.h`, `text/text_tree.h` and `text/text_layers.h`. Every function that answers a string answers one the caller frees; every failure fills a `TextError` (`line`, 1-based, 0 for the document as a whole, and `message`).

| Function | What it does |
|---|---|
| `text_kind_name`, `text_kind_find` | the kind names above |
| `text_tree_print(object, kind)` | the document of any JSON object |
| `text_tree_parse(text, &kind, &error)` | the object of a document, and its kind |
| `text_scalar_print(value)` | one scalar token or an inline array of them (the views and the command sheet print values with it) |
| `text_lines_split(text, &lines, &error)` | the lines of a text without comments and blank lines, each with its indent and number |
| `text_units_parse(line, number, &error)` | the units of one line as an array |
| `text_block_parse(&lines, &pos, indent, &error)` | the members or items at an indent (B6's command parser reads object params with it) |
| `text_from_json(json, kind, &error)` | a document of a kind from JSON text; "this is not a <kind>" when the JSON is not that kind |
| `text_to_json(knowledge, text, &kind, &error)` | the JSON of a document: a plan, a reading or a realization through its codec (a refusal reads "<path>: <message>"), anything else as the tree |
| `text_plan_producer(document, &producer)` | the `PlanProducer` a plan document's `metadata` names |

The kinds `plan`, `reading` and `realization` need the knowledge snapshot the codecs read with. `knowledge` and `knowledge-entry` have no snapshot check: `tools/ci/check-knowledge.py` checks entries.

## Where it sits

`text` is a library group above `plan` and `reading` (layer table row `uses text model theory realization plan reading cjson common`). Fixtures are in [tests/fixtures](tests/fixtures/README.md).
