/* Covers the Collections view's data decisions through the pure helpers in
: card text, the add-pieces picker, the add body
 * and result line, and the name given to Save as collection. Plain Node
 * script: `node frontend/tests/collections-view.test.js`. */
import M from "../public/models/piece-metadata.js";
import { renderCorpusView } from "../public/views/collections.js";
import { check, same, finish } from "./support/check.js";
import { openPage } from "./support/page.js";

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

/* The view: the cards with their summaries, a collection's members listed and filtered by the backend, and the picker's exclusion of members. */
const members = [library[0], library[1]];
const page = await openPage({
  "GET /api/corpus": { ok: true, corpora: [{ id: "col-1", name: "Set", description: "", piece_count: 2, updated_at: "2026-11-01T00:00:00Z" }] },
  "GET /api/corpus/col-1": { ok: true, id: "col-1", name: "Set", description: "", pieces: ["p1", "p2"] },
  "GET /api/analyzer/groups": { ok: true, composers: [{ value: "John Field", count: 2 }], sources: [{ value: "Op 1", count: 1 }], tags: [{ value: "slow", count: 1 }, { value: "whimsical", count: 2 }], unassigned: { composer: 0, source: 1 }, untagged: 0, total: 2 },
  "GET /api/analyzer/pieces": (call) => ({ ok: true, pieces: call.url.includes("tag=slow") ? [members[1]] : members }),
  "GET /api/styles": { ok: true, styles: [] },
});
await renderCorpusView();
await page.settle();
const card = page.content.querySelector(".collection-card");
check(card !== null && card.textContent.includes("2 pieces") && card.textContent.includes("Composers: John Field") && card.textContent.includes("Tags: whimsical, slow"), "a card shows the count, composers and tags from the collection's groups");
check(page.callsTo("GET", "/api/analyzer/groups?collection=col-1").length >= 1, "the summary asks the groups route for the collection");

card.click();
await page.settle();
check(page.content.querySelectorAll("#corpus-piece-table-container tbody tr").length === 2, "the detail lists the members");
const tagSelect = page.document.getElementById("corpus-filter-tag");
check(same(tagSelect.options.map((o) => o.textContent), ["Any", "Untagged (0)", "slow (1)", "whimsical (2)"]), "the filter selects come from the collection's groups");
tagSelect.value = "slow";
tagSelect.dispatchEvent({ type: "change" });
await page.settle();
check(page.callsTo("GET", "/api/analyzer/pieces?collection=col-1&tag=slow").length === 1, "a filter is a query on the collection");
check(page.content.querySelectorAll("#corpus-piece-table-container tbody tr").length === 1 && page.document.getElementById("corpus-count").textContent === "1 of 2 pieces", "the table shows the members the backend kept");

finish();
