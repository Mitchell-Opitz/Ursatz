# lock and regenerate commands

```sh
ursatz-composer lock <plan.json> (--node <id> | --section <letter>) [--lane harmony|bass|melody|inner] --out <plan.json> [--format text|json]
ursatz-composer regenerate <plan.json> (--node <id> | --section <letter>) [--lane harmony|bass|melody|inner] [--seed <n>] --out <plan.json> [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format text|json]
```

Take a filled plan (from [`fill`](cli-fill.md)) apart. `lock` keeps material: it becomes Given and nothing composes over
it. `regenerate` writes one node again with a seed, and only what depends on it. See [regenerate.md](regenerate.md) for
what that means. [`variations` and `harmonize`](cli-variations.md) write several versions of a node at once.

`lock` and `regenerate` read the plan file into a request for `composer_api_lock` and `composer_api_regenerate` ([api.md](api.md)); the node is found, refused and described by the entry, and the files are written from the response.

## Addressing a node

`--node <id>` is the node's `composer.id` in the plan file. `--section <letter>` is the section with that form letter
(`A`, `B`, `A'`). One of them is required. The ids of a plan file run from 1 without gaps: a plan written after a section
was built again numbers its nodes again, so read the ids from the file you give.

`--lane` narrows the node to one lane of it and of what is under it: `harmony`, `bass`, `melody` or `inner` (all the inner
voices).

## lock

| Option | Meaning |
| --- | --- |
| `--out <plan.json>` | Where the plan is written (required). |
| `--format <text\|json>` | How to print the result (default: text). |

Prints what became Given. A node or span not written yet is refused (run `fill` first). Locking is for good: to unlock, go
back to an earlier plan file.

```
$ ursatz-composer lock piece.json --section A --out piece-locked.json
locked section A: 9 nodes and 23 lane spans now Given
wrote piece-locked.json
```

With `--format json` the output is the object `{"locked": {"nodes": 9, "spans": 23}, "plan": "piece-locked.json"}`: the
nodes and lane spans that became Given, and the path of the plan as given to `--out`.

## regenerate

| Option | Meaning |
| --- | --- |
| `--seed <n>` | Whole number from 0. The same plan, node and seed give the same output. Default: from the clock. The seed used is always printed. If a seed gets stuck the next seeds are tried, up to 16. |
| `--out <plan.json>` | Where the plan is written (required). |
| `--midi <file>`, `--musicxml <file>` | Also write the piece. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--format <text\|json>` | How to print the result (default: text). |

The output is `fill`'s, with what was reopened first: the sections reopened whole, the lane spans reopened one by one, the
seed, the form and cadences heard in each section written again, and the returns heard. With `--format json` the object has
a `reopened` member (`sections` and `spans` by lane) after `seed` and `realized`. A section the run kept, outside the scope of
the hearing ([regenerate.md](regenerate.md)), prints as `kept` (text) and as `"heard": null, "in_scope": false` (JSON).

```
$ ursatz-composer regenerate piece-locked.json --section B --seed 5 --out piece-2.json
reopened sections B
seed 5

sections (form and cadences heard)
  B   heard as hybrid, cadences half

returns heard 0 of 0 planned
wrote piece-2.json
```

A locked section is refused, and so is one with Given material among its phrases and ideas.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument. |
| 2 | The plan cannot be read, the node is not found, is Given or cannot be regenerated, a node to lock has parts not written, or no seed completes the plan. |
| 3 | An output file could not be written. |
