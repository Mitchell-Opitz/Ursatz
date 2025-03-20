/* Covers the pure Library metadata helpers (public/piece-metadata.js).
 * Plain Node script: `node frontend/tests/piece-metadata.test.js`. */
const M = require("../public/piece-metadata.js");

let failures = 0;
function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

check(same(M.parseTagInput(" Etude, nocturne ,ETUDE,,"), ["etude", "nocturne"]), "tags trimmed, lowercased, deduplicated, empties dropped");
check(same(M.parseTagInput(""), []), "empty tag text gives no tags");
check(same(M.parseTagInput(undefined), []), "missing tag text gives no tags");

check(M.filterQuery({}) === "", "empty filter gives an empty query");
check(M.filterQuery({ composer: "John Field", tag: "a&b" }) === "composer=John%20Field&tag=a%26b", "values are encoded");
check(
  M.filterQuery({ source: "Op. 1", unassigned: ["composer", "source"], untagged: true }) ===
    "source=Op.%201&unassigned=composer&unassigned=source&untagged=1",
  "unassigned may repeat and untagged is 1",
);

const pieces = [
  { id: "p1", filename: "b.mid", composer: "Field", source: "Op 1", tags: ["etude"], uploaded_at: "2026-01-02" },
  { id: "p2", filename: "A.mid", composer: "", source: "", tags: [], uploaded_at: "2026-01-03" },
  { id: "p3", filename: "", composer: "chopin", source: "Book", tags: ["waltz"], uploaded_at: "2026-01-01" },
];
const ids = (list) => list.map((p) => p.id);
check(same(ids(M.sortPieces(pieces, "added")), ["p2", "p1", "p3"]), "added sorts newest first");
check(same(ids(M.sortPieces(pieces, "title")), ["p2", "p1", "p3"]), "title sorts case-insensitively, id when no filename");
check(same(ids(M.sortPieces(pieces, "composer")), ["p3", "p1", "p2"]), "composer sorts case-insensitively with unassigned last");
check(same(ids(pieces), ["p1", "p2", "p3"]), "sorting does not change the input");

check(same(ids(M.searchPieces(pieces, "A.MID")), ["p2"]), "search matches the title");
check(same(ids(M.searchPieces(pieces, "chop")), ["p3"]), "search matches the composer");
check(same(ids(M.searchPieces(pieces, "op 1")), ["p1"]), "search matches the source");
check(same(ids(M.searchPieces(pieces, "wal")), ["p3"]), "search matches a tag");
check(M.searchPieces(pieces, "  ").length === 3, "blank search keeps every piece");

check(same(M.bulkPatch("set-composer", "Field"), { composer: "Field" }), "set-composer body");
check(same(M.bulkPatch("set-source", "Op 1"), { source: "Op 1" }), "set-source body");
check(same(M.bulkPatch("add-tag", ["a"]), { add_tags: ["a"] }), "add-tag body");
check(same(M.bulkPatch("remove-tag", "a"), { remove_tags: ["a"] }), "remove-tag body wraps a single tag");
check(M.bulkPatch("nope", "x") === null, "unknown action gives null");

process.exit(failures === 0 ? 0 : 1);
