/* Covers the Collections view's data decisions through the pure helpers in
 * public/piece-metadata.js: card text, the add-pieces picker, the add body
 * and result line, and the name given to Save as collection. Plain Node
 * script: `node frontend/tests/collections-view.test.js`. */
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

const library = [
  { id: "p1", filename: "nocturne.mid", composer: "John Field", source: "", tags: ["whimsical"] },
  { id: "p2", filename: "sonata.mid", composer: "John Field", source: "Op 1", tags: ["whimsical", "slow"] },
  { id: "p3", filename: "etude.mid", composer: "Chopin", source: "", tags: [] },
];

/* Card text, as buildCollectionCard assembles it. */
const summary = M.collectionSummary(library.slice(0, 2));
const lines = [
  M.pieceCountText(2),
  M.summaryLine("Composers", summary.composers),
  M.summaryLine("Tags", summary.tags),
];
check(same(lines, ["2 pieces", "Composers: John Field", "Tags: whimsical, slow"]), "card shows count, composers and tags");
check(M.pieceCountText(0) === "0 pieces", "an empty collection says 0 pieces");

/* The picker. */
const candidates = M.pickerCandidates(library, ["p2"]);
check(same(candidates.map((piece) => piece.id), ["p1", "p3"]), "the picker excludes pieces already in the collection");
check(M.pickerCandidates(library, library.map((piece) => piece.id)).length === 0, "the picker is empty when every piece is a member");
check(same(M.searchPieces(candidates, "chopin").map((piece) => piece.id), ["p3"]), "the picker search narrows the candidates");
check(same(M.addPiecesBody(candidates.map((piece) => piece.id)), { piece_ids: ["p1", "p3"] }), "Add all matching sends every candidate in one body");
check(M.addResultText(5, 2) === "Added 5, 2 were already in it", "the result line reports added and already");

/* Detail filters over members. */
check(same(M.matchPieces(library, { tag: "slow" }).map((piece) => piece.id), ["p2"]), "member filter by tag");
check(same(M.matchPieces(library, { untagged: true }).map((piece) => piece.id), ["p3"]), "member filter untagged");

/* Save as collection. */
check(M.collectionNameFromFilter({ composer: "John Field" }) === "John Field", "Save as collection names a composer filter");
check(M.collectionNameFromFilter({ tag: "whimsical" }) === "Tag: whimsical", "Save as collection names a tag filter");
check(
  M.collectionNameFromFilter({ composer: "John Field", tag: "whimsical" }) === "John Field · Tag: whimsical",
  "Save as collection combines the parts",
);

process.exit(failures === 0 ? 0 : 1);
