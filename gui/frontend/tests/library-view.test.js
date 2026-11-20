/* The Library view (views/library.js) on the fake page: the table lists a piece by its filename (the bare id when it has none), shows a dash for an
 * empty composer, has the select, title, composer, source, tags and actions columns, merges a passed filter over the defaults into the query, and
 * its Edit editor sends the typed title and shows the backend's refusal of an empty one. Plain Node script, run through ctest. */
import { renderLibraryView } from "../public/views/library.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { openPage } from "./support/page.js";

const pieces = [
  { id: "piece-1788837459-41", filename: "sonata.mid", composer: "John Field", source: "", tags: [], analysis_status: "current" },
  { id: "piece-1788837459-42", filename: "", composer: "", source: "Op. 1", tags: ["etude"], analysis_status: "current" },
];
const groups = { ok: true, composers: [{ value: "John Field", count: 1 }], sources: [], tags: [{ value: "etude", count: 1 }], unassigned: { composer: 1, source: 1 }, untagged: 1, total: 2 };
const page = await openPage({
  "GET /api/analyzer/pieces": { ok: true, pieces },
  "GET /api/analyzer/groups": groups,
  "GET /api/subjects": { ok: true, subjects: [] },
  "GET /api/analyzer/pieces/piece-1788837459-41": { ok: true, subjects: [] },
  "PATCH /api/analyzer/pieces/piece-1788837459-41": (call) =>
    call.body.filename === "" ? { status: 400, body: { ok: false, error: "filename must not be empty", code: "request" } } : { ok: true, piece: { ...pieces[0], filename: call.body.filename } },
});
page.setShell({ showAnalysis() {}, setTagsActive() {}, refreshTags() {} });

await renderLibraryView();
await page.settle();

const headers = page.content.querySelectorAll("thead th").map((th) => th.textContent);
check(headers.length === 6 && headers[1] === "Title" && headers[5] === "Actions", "the Library has the select, title, composer, source, tags and actions columns");
const rows = page.content.querySelectorAll("tbody tr");
check(rows.length === 2, "one row per piece");
check(rows[0].querySelectorAll("td")[1].textContent === "sonata.mid", "the title column shows the original filename");
check(rows[1].querySelectorAll("td")[1].textContent === "piece-1788837459-42", "the title column falls back to the bare id when the filename is empty");
check(rows[0].querySelectorAll("td")[2].textContent === "John Field" && rows[1].querySelectorAll("td")[2].textContent === "—", "composer shows its value, or a dash when empty");
const actions = rows[0].querySelectorAll("button").map((b) => b.getAttribute("aria-label"));
check(actions.includes("Edit") && actions.includes("Re-run analysis") && !actions.includes("Rename"), "Edit replaces Rename among the row actions");
check(page.callsTo("GET", "/api/analyzer/pieces?").length === 0, "no filter asks for the whole list");

await renderLibraryView({ tag: "etude" });
await page.settle();
check(page.callsTo("GET", "/api/analyzer/pieces?tag=etude").length === 1, "a passed filter is part of the query");

/* The editor. */
page.content.querySelectorAll("tbody tr")[0].querySelector('button[aria-label="Edit"]').click();
await page.settle();
const editor = page.content.querySelector("tr.editor-row");
check(editor !== null, "Edit opens the editor in the row");
const title = editor.querySelector("input");
title.value = "";
byText(editor, "button", "Save")[0].click();
await page.settle();
check(page.callsTo("PATCH", "/api/analyzer/pieces/piece-1788837459-41").length === 1, "Save sends the piece");
check(editor.textContent.includes("filename must not be empty"), "the backend's refusal of an empty title is shown");
title.value = "Sonata in C";
byText(editor, "button", "Save")[0].click();
await page.settle();
check(page.content.querySelector("tr.editor-row") === null && page.content.textContent.includes("Sonata in C"), "a saved title replaces the editor with the row");

finish();
