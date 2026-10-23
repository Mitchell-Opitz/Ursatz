# Motive links

```sh
ursatz-analyzer --link-motives <corpus output dir> [--cache <file>] [--depth <1-6>] [--candidates <n>] [--format text|json]
```

Library: `include/ursatz_analyzer/motive_links.h`. Output schemas: `schema/motive-links.v1.schema.json` and, for the
cache, `schema/motive-link-cache.v1.schema.json`.

Linking finds the motive families of different pieces that are variants of one another: one family's prototype is a
chain of motive relations (a transposition, an augmentation, an inversion, a fragment, and so on; Ursatz's
`analysis/motive_relation.h`) from the other's. It reads only the analysis documents of a corpus run (see
[corpus.md](corpus.md)), never the scores, and only on request: analyzing a piece does not link it.

## Scope

A scope is a set of pieces in the order they were added. `motive_link_scope_add_document` adds one piece from its
document's JSON; `motive_link_scope_add_corpus` adds every analyzed piece of a corpus run, in index order, the index path
as the piece id. A piece id added twice is refused. A document whose motive families have no `features` (written before
analyzer 0.2.0) is `MOTIVE_LINKS_ERR_NO_FEATURES`: analyze the piece again. The CLI names the piece and says to run the
corpus again. The scope is the caller's choice (a corpus run, a collection, a subject), so one cache serves them all.

Each family is read from its prototype's `features` (see [analysis-document.md](analysis-document.md)) as a melodic
sketch, without chords, in the key region of the prototype's first note (`motive_features_sketch`). The sketch is then
moved to the key of C, keeping its mode, by the shortest way that puts its tonic on C. The relations keep a sketch's
scale, so two motives in different keys could never be made equal otherwise; once both are in C they differ only by the
relations. A link between a G major motive and a C major one therefore reads as the same chain as between two C major
ones, and the steps apply to the sketches `motive_link_scope_sketch` returns, whose pitches are the key-of-C pitches. A
step such as `transpose by=12` is a move between those, not between the written keys.

## Candidates, the gate and directions

Recognizing a pair that is not a link costs time: a depth-2 miss takes about 330 ms in the default build (Ursatz's
[motive-link.md](../external/ursatz/docs/modules/motive-link.md)). So each family takes only the families of other pieces
at nearness at least 1/2 (the mean of the four dimension similarities, `motive_link_nearness`), nearest first, ties by
piece order and then family index, and at most `--candidates` of them (default 4). The union of these as unordered pairs
is what gets compared, each pair once.

A pair is compared with `motive_link_pair`, the earlier family in scope order as the source, depth 2 (`--depth`) and no
admit filter. A chain is directional: a fragment of a motive is reached from the whole, not the whole from the fragment.
So when no chain runs from the source to the other family, the other way is tried; `reversed` says the chain found runs
from b to a. In the text output a link is printed in the direction of its chain, source first.

Linked families are grouped by union: if A links to B and B to C, the group is A, B, C even when A and C were not
compared. Groups list their members in scope order, the largest group first.

## The cache

One cache serves every scope. It keeps, for each pair of sketches compared, whether it linked, the chain, the distance and
the dimension similarities, keyed by the two sketches' fingerprints (`motive_sketch_fingerprint` of the key-of-C
sketches), so the same two motives found in other pieces, or in another scope, are not compared again. `--cache <file>`
names the file; the default is `<corpus output dir>/motive-link-cache.json`. The CLI always writes it back (to
`<file>.tmp`, then renamed over it). A file that does not exist is an empty cache; a file that is not a cache is an error
(exit 2).

A cache is valid only for the Ursatz commit, the analyzer version and the link options (depth, beam, nearness gate,
contour) it was made with. A run with others empties it, compares everything, and says so (`cache_reset` in the JSON).
`--candidates` is not part of the key: it changes which pairs are considered, not what a pair's result is.

Incremental runs: adding one piece to a scope and running again compares only the pairs with a family of that piece. A
new candidate can only push an old pair out of a family's top list, never into it, so every other pair is in the cache.
A pair pushed out stays in the cache and is used again if a later scope needs it.

## Output

Text, one line per link, then one per group, then the counts:

```
a.musicxml m.1 b.1 [1 3 5 4 3] -> b.musicxml m.1 b.1 [1 3 5 4 3]: augment ratio=2:1 (1/5)
a.musicxml m.1 b.1 [1 3 5 4 3] -> d.musicxml m.1 b.1 [1 6 4 5 6]: invert tonal=1 (1/3)
group of 3: a.musicxml #0, b.musicxml #0, d.musicxml #0
compared 3, cached 0 of 3 pairs
```

A link line is `<piece> m.<first measure> b.<beat> [<degrees>] -> <piece> ...: <steps joined by " then "> (<distance>)`.
The degrees are the prototype's against the tonic's major scale. A group member is `<piece> #<family>`, the family's
index in its piece's `motives`. The distance is the recognizer's `cost / (cost + 4)` over the chain's summed relation
costs, an exact fraction (a transposition, augmentation or diminution costs 1, a change of meter 3, every other 2).

`--format json` writes the same links with their dimension similarities (rhythm, intervals, contour, harmony; 1 means the
dimension was kept), the steps as text (`motive_relation_step_text`, parsed back with `motive_relation_step_parse`),
`reversed` and the counts. The same links and counts give the same bytes. The library gives the steps as
`MotiveRelationStep` values as well.

Exit codes: 0, 1 for a usage error, 2 for a corpus output that cannot be read, a document without features or a cache that
cannot be read or written.

## Measured time

`tests/links` (four pieces of one motive family each) on the default build: 3 pairs compared in about 0.05 s, the same run
from the cache in about 0.01 s (reading the four documents). A pair that is not a link and passes the gate costs up to
about 330 ms, so a corpus costs at most `families x candidates` of those on a first run, and nothing for the pairs it has
seen before.
