# recognize command

```sh
ursatz-composer recognize <source> <target> [--format ...] [--form ...] --meter <n/m> [--key <key>] [--pickup <beats>] [--harmony "<text>"] [--feature <label:first-last>]... [--role <melodic|accompaniment>] [--target-format <format>] [--target-pickup <beats>] [--target-harmony "<text>"] [--target-feature <label:first-last>]... [--basic <file> [--basic-format <format>] [--basic-pickup <beats>] [--basic-harmony "<text>"]] [--depth <n>] [--json]
```

Says how one motive derives from another: the chain of [variant registry](variant-registry.md) operations that turns the source into the target, how far apart they are, and what was kept and changed. It is the reverse of [variants](cli-variants.md): the chain it finds, applied to the source with the registry, gives a motive equal to the target.

Both motives are loaded as for the [motive command](cli-motive.md). The source options (`--format`, `--form`, `--pickup`, `--harmony`, `--feature`, `--role`) describe the source; the target is read with `--target-format`, `--target-pickup`, `--target-harmony` and `--target-feature`, and shares `--meter`, `--key` and the form. `--basic` names the basic motive, the tree root, read in the source's form, meter, key and role (with `--basic-format`, `--basic-pickup` and `--basic-harmony`); the target's remoteness is measured from it. `--depth` is the longest chain to look for, 1 to 4, 4 by default.

## Result

Text output names the chain, the distance (cost over cost plus 4, exact, 1 when no chain is found), the remoteness and a derivation sentence. Remoteness is the target's distance from the basic motive (R-142), not from the source: without `--basic` the source is the basic motive and the two are equal; with it, the target one step from the source can be two from the basic motive. `--json`:

```json
{
  "found": true,
  "chain": {"steps": [{"op": "transpose", "params": {"tonal": false, "by": 2}}]},
  "distance": 0.2,
  "remoteness": 0.2,
  "kept": ["rhythm", "intervals", "contour"],
  "changed": ["harmony"],
  "derivation": "Transposed +2 semitones. Kept the rhythm, the intervals and the contour; changed the harmony."
}
```

Exit status 0 whether or not a chain is found (`found` is false, `steps` is empty and the derivation says no chain of up to `--depth` steps turns the source into the target); exit 1 for bad input.

## How it searches

The search is the library's (`motive_variant_recognize`, see the Ursatz docs): a beam search over relations, so every chain of one or two steps is found and chains of three or four are found about 4 times in 5 (R-149). It only returns chains the registry can replay: the relation has a row, the row takes the source's form and every parameter is in the row's range (R-152). A contour source is searched by rank, as the Composer normalizes a contour after every step (R-148). A source or target with no chords of its own is compared without the implied chord; a harmony step that only adds that chord is left out of the replay.

## Timing

`tests/test_variant_recognize.c` makes every registry row on five fixtures (seeds 1 to 20) and random chains (seeds 1 to 50), recognizes each result, replays the chain and compares. 2043 variants, all recovered. Default (Debug) build: full run about 17 s, `--smoke` (seeds 1 to 3) about 2 s. The full run is labelled `slow`.
