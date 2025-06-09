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

const summaryPieces = [
  { id: "a", composer: "Field", tags: ["etude", "slow"] },
  { id: "b", composer: "field", tags: ["etude"] },
  { id: "c", composer: "Clementi", tags: ["slow", "zeta"] },
  { id: "d", composer: "Mozart", tags: ["alpha", "etude"] },
  { id: "e", composer: "Bach", tags: [] },
  { id: "f", composer: "", tags: [] },
];
const summary = M.collectionSummary(summaryPieces);
check(summary.composers.length === 3, "summary keeps the top 3 composers");
check(summary.composers[0].value === "Field" && summary.composers[0].count === 2, "composers are counted case-insensitively");
check(same(summary.composers.slice(1).map((c) => c.value), ["Bach", "Clementi"]), "composer ties are ordered by name");
check(same(summary.tags.map((t) => [t.value, t.count]), [["etude", 3], ["slow", 2], ["alpha", 1]]), "top 3 tags by count, ties by name");
check(same(M.collectionSummary([]), { composers: [], tags: [] }), "an empty collection has an empty summary");
check(same(M.collectionSummary([{ id: "x", composer: "" }]).composers, []), "unassigned composers are not counted");

check(M.pieceCountText(1) === "1 piece" && M.pieceCountText(5) === "5 pieces", "piece count text");
check(M.summaryLine("Tags", summary.tags) === "Tags: etude, slow, alpha", "summary line lists values");
check(M.summaryLine("Tags", []) === "", "summary line is empty without values");

const members = [
  { id: "m1", filename: "a.mid", composer: "Field", source: "Op 1", tags: ["etude"] },
  { id: "m2", filename: "b.mid", composer: "", source: "", tags: [] },
  { id: "m3", filename: "c.mid", composer: "Mozart", source: "Op 1", tags: ["etude", "slow"] },
];
check(M.matchPieces(members, { composer: "field" }).length === 1, "client filter matches composer case-insensitively");
check(M.matchPieces(members, { tag: "ETUDE" }).length === 2, "client filter matches a tag");
check(M.matchPieces(members, { unassigned: ["composer"] })[0].id === "m2", "client filter unassigned composer");
check(M.matchPieces(members, { untagged: true })[0].id === "m2", "client filter untagged");
check(M.matchPieces(members, { source: "op 1", search: "mozart" }).length === 1, "client filter combines fields and search");
check(M.matchPieces(members, {}).length === 3, "empty client filter keeps all");
const groups = M.groupValues(members);
check(same(groups.tags.map((t) => [t.value, t.count]), [["etude", 2], ["slow", 1]]), "member groups count tags");
check(groups.unassigned.composer === 1 && groups.untagged === 1 && groups.total === 3, "member groups count unassigned and untagged");

check(same(M.pickerCandidates(members, ["m2"]).map((p) => p.id), ["m1", "m3"]), "picker excludes members");
check(same(M.addPiecesBody(["a", "b"]), { piece_ids: ["a", "b"] }), "add body uses piece_ids");
check(M.addResultText(5, 2) === "Added 5, 2 were already in it", "result line with already");
check(M.addResultText(3, 1) === "Added 3, 1 was already in it", "result line with one already");
check(M.addResultText(4, 0) === "Added 4", "result line without already");

check(M.collectionNameFromFilter({ composer: "John Field" }) === "John Field", "name from composer");
check(M.collectionNameFromFilter({ tag: "whimsical" }) === "Tag: whimsical", "name from tag");
check(
  M.collectionNameFromFilter({ composer: "John Field", source: "Op 1", tag: "etude" }) ===
    "John Field · Source: Op 1 · Tag: etude",
  "name combines parts with a middle dot",
);
check(M.collectionNameFromFilter({ unassigned: ["composer"], untagged: true }) === "No composer · Untagged", "name for unassigned and untagged");
check(M.collectionNameFromFilter({ search: " waltz " }) === "Search: waltz", "name from search alone");
check(M.collectionNameFromFilter({}) === "Selected pieces", "name fallback");
check(M.collectionNameFromFilter({ composer: "x".repeat(200) }).length === 120, "name is cut at 120 characters");

/* Style names and requests. */
check(M.defaultStyleName({ kind: "filter", filter: { composer: "John Field" } }) === "Composer: John Field", "style name from composer");
check(M.defaultStyleName({ kind: "filter", filter: { source: "Op 1" } }) === "Source: Op 1", "style name from source");
check(M.defaultStyleName({ kind: "filter", filter: { tag: "Whimsical" } }) === "Tag: whimsical", "style name lowercases the tag");
check(
  M.defaultStyleName({ kind: "filter", filter: { composer: "John Field", source: "Op 1", tag: "etude" } }) ===
    "Composer: John Field · Source: Op 1 · Tag: etude",
  "style name combines filters with a middle dot",
);
check(M.defaultStyleName({ kind: "collection", name: "Nocturnes" }) === "Collection: Nocturnes", "style name from a collection");
check(M.defaultStyleName({ kind: "pieces", piece_ids: ["p1"] }) === "Selected pieces", "style name for chosen pieces");
check(M.defaultStyleName({ kind: "filter", filter: { composer: "x".repeat(200) } }).length === 120, "style name is cut at 120 characters");

check(
  same(M.styleSourceForFilter({ composer: "John Field", tag: "etude", unassigned: [], search: "" }, ["p1"]), {
    kind: "filter",
    filter: { composer: "John Field", tag: "etude" },
  }),
  "a plain filter builds from the backend filter",
);
check(same(M.styleSourceForFilter({ untagged: true }, ["p1", "p2"]), { kind: "pieces", piece_ids: ["p1", "p2"] }), "untagged builds from the shown ids");
check(same(M.styleSourceForFilter({ unassigned: ["composer"] }, ["p1"]), { kind: "pieces", piece_ids: ["p1"] }), "unassigned builds from the shown ids");
check(same(M.styleSourceForFilter({ composer: "Chopin", search: "waltz" }, ["p3"]), { kind: "pieces", piece_ids: ["p3"] }), "a search builds from the shown ids");
check(same(M.styleSourceForFilter({}, ["p1"]), { kind: "pieces", piece_ids: ["p1"] }), "no filter builds from the shown ids");

check(
  same(M.styleRequestBody({ kind: "filter", filter: { tag: "etude" } }, " My style "), { name: "My style", filter: { tag: "etude" } }),
  "request body for a filter",
);
check(same(M.styleRequestBody({ kind: "collection", collection_id: "col_1" }, ""), { collection_id: "col_1" }), "a blank name is left out");
check(same(M.styleRequestBody({ kind: "pieces", piece_ids: ["p1"] }, "S"), { name: "S", piece_ids: ["p1"] }), "request body for ids");

check(M.styleBuildText(12, 0) === "Built from 12 pieces", "build text without skipped");
check(M.styleBuildText(1, 2) === "Built from 1 piece, 2 skipped", "build text with skipped");
check(M.styleBuildText(3, 0, "Rebuilt") === "Rebuilt from 3 pieces", "rebuild text");
check(M.styleSourceText({ kind: "filter", filter: { tag: "whimsical" } }) === "Tag: whimsical", "source text for a filter");
check(M.styleSourceText({ kind: "collection", collection_id: "c1" }, { c1: "Nocturnes" }) === "Collection: Nocturnes", "source text for a collection");
check(M.styleSourceText({ kind: "collection", collection_id: "gone" }, {}) === "Collection (deleted)", "source text for a deleted collection");
check(M.styleSourceText({ kind: "pieces", piece_ids: ["a", "b"] }) === "Selected pieces (2)", "source text for ids");

process.exit(failures === 0 ? 0 : 1);
