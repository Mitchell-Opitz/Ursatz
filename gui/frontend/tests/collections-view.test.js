/* Covers the Collections view's data decisions through the pure helpers in
: card text, the add-pieces picker, the add body
 * and result line, and the name given to Save as collection. Plain Node
 * script: `node frontend/tests/collections-view.test.js`. */
import M from "../public/models/piece-metadata.js";
import { check, same, finish } from "./support/check.js";

const library = [
  { id: "p1", filename: "nocturne.mid", composer: "John Field", source: "", tags: ["whimsical"] },
  { id: "p2", filename: "sonata.mid", composer: "John Field", source: "Op 1", tags: ["whimsical", "slow"] },
  { id: "p3", filename: "etude.mid", composer: "Chopin", source: "", tags: [] },
];

/* Card text, as buildCollectionCard assembles it. */
const summary = M.collectionSummary({
  composers: [{ value: "John Field", count: 2 }],
  tags: [{ value: "slow", count: 1 }, { value: "whimsical", count: 2 }],
});
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

/* Detail filters over members are the backend's: the same query as the Library's, limited to the collection. */
check(M.filterQuery({ tag: "slow", untagged: false }, "col-1") === "collection=col-1&tag=slow", "member filter by tag is a query on the collection");
check(M.filterQuery({ untagged: true }, "col-1") === "collection=col-1&untagged=1", "member filter untagged is a query on the collection");

/* Save as collection. */
check(M.collectionNameFromFilter({ composer: "John Field" }) === "John Field", "Save as collection names a composer filter");
check(M.collectionNameFromFilter({ tag: "whimsical" }) === "Tag: whimsical", "Save as collection names a tag filter");
check(
  M.collectionNameFromFilter({ composer: "John Field", tag: "whimsical" }) === "John Field · Tag: whimsical",
  "Save as collection combines the parts",
);

finish();
