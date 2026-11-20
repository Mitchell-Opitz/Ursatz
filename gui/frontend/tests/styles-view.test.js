/* The styles section and detail (views/styles.js) on the fake page: the list shows each style with its source and piece count, hides when there are
 * none, and Open draws the style's detail with the pieces it used (a piece that left the Library says so). Plain Node script, run through ctest. */
import { loadStylesSection } from "../public/views/styles.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { openPage } from "./support/page.js";

const styles = [{ id: "sty-1", name: "Field style", piece_count: 2, source: { kind: "collection", collection_id: "col-1" }, updated_at: "2026-11-02T10:00:00Z" }];
let list = styles;
const tools = [];
const page = await openPage({
  "GET /api/styles": () => ({ ok: true, styles: list }),
  "GET /api/corpus": { ok: true, corpora: [{ id: "col-1", name: "Set" }] },
  "GET /api/styles/sty-1": { ok: true, style: { ...styles[0], pieces: ["p1", "p-gone"], skipped: [{ id: "p3", reason: "no melody" }], statistics: {} } },
  "GET /api/analyzer/pieces": { ok: true, pieces: [{ id: "p1", filename: "nocturne.mid" }, { id: "p3", filename: "etude.mid" }] },
});
page.setShell({ leaveWorkspace() {}, setActiveTool: (id) => tools.push(id), setTagsActive() {}, showCollections() {} });
const section = page.document.createElement("section");
section.id = "styles-section";
page.content.appendChild(section);

await loadStylesSection();
await page.settle();
const rows = section.querySelectorAll("tbody tr");
check(rows.length === 1 && rows[0].textContent.includes("Field style") && rows[0].textContent.includes("Collection: Set") || rows[0].textContent.includes("Set"), "the list shows a style with its source, named after the collection");
check(!section.hidden, "the section is shown when there are styles");

section.querySelector('button[aria-label="Open Field style"]').click();
await page.settle();
check(page.document.getElementById("style-title").textContent === "Field style", "Open draws the style's title");
const detail = page.document.getElementById("style-body");
check(detail.textContent.includes("nocturne.mid") && detail.textContent.includes("p-gone (no longer in the Library)"), "the pieces used are listed, and one that left the Library says so");
check(detail.textContent.includes("etude.mid: no melody"), "a skipped piece is listed with its reason");
byText(detail, "button", "nocturne.mid")[0].click();
check(tools[tools.length - 1] === "analysis" && page.callsTo("GET", "/api/analyzer/pieces/p1").length === 1, "a piece opens in the Analysis view");

list = [];
page.content.innerHTML = '<section id="styles-section"></section>';
await loadStylesSection();
await page.settle();
check(page.document.getElementById("styles-section").hidden === true, "the section hides when there are no styles");

finish();
