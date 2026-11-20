/* The Motive links tool (views/motive-links-view.js) on the fake page: the scope pickers come from the Library's groups and the collections, the
 * earlier runs are listed, and Run sends the chosen scope and draws the run's progress line. Plain Node script, run through ctest. */
import { renderMotiveLinksView } from "../public/views/motive-links-view.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const runs = [{ id: "run-1", status: "done", scope: { kind: "all" }, created_at: "2026-11-02T10:00:00Z" }];
const page = await openPage({
  "GET /api/corpus": { ok: true, corpora: [{ id: "col-1", name: "Set" }] },
  "GET /api/analyzer/groups": { ok: true, composers: [{ value: "Field", count: 2 }], sources: [{ value: "Op. 1", count: 1 }], tags: [{ value: "slow", count: 1 }], unassigned: { composer: 0, source: 0 }, untagged: 0, total: 2 },
  "GET /api/subjects": { ok: true, subjects: [] },
  "GET /api/materials/projects": { ok: true, projects: [] },
  "GET /api/links/runs": { ok: true, runs },
  "POST /api/workshop/command": byCommand({
    "links.run": { ok: true, run: { id: "run-2", status: "done", scope: { kind: "collection", collection_id: "col-1" }, created_at: "2026-11-03T10:00:00Z", result: { pieces: 2, families: [], links: [] } } },
  }),
});
page.setShell({ showAnalysis() {} });

await renderMotiveLinksView();
await page.settle();

check(page.callsTo("GET", "/api/analyzer/groups").length === 1, "the pickers read the Library's groups");
const kind = page.content.querySelector("select");
check(kind.options.map((o) => o.textContent).join() === "All Library pieces,Collection,Composer / Source / Tag,Subject,Project", "the scope kinds are offered");
check(page.content.textContent.includes("Earlier runs") && page.content.querySelectorAll("li a.clickable").length === 1, "the earlier runs are listed");

kind.value = "collection";
kind.dispatchEvent({ type: "change" });
const pickers = page.content.querySelectorAll("select");
check(pickers.length === 2 && pickers[1].options.map((o) => o.textContent).join() === "Choose a collection,Set", "the Collection scope offers the collections");
byText(page.content, "button", "Run")[0].click();
await page.settle();
check(page.commands("links.run").length === 0 && page.content.textContent.includes("Choose what to compare first."), "Run without a chosen scope is refused in the page");

pickers[1].value = "col-1";
pickers[1].dispatchEvent({ type: "change" });
byText(page.content, "button", "Run")[0].click();
await page.settle();
const post = page.commands("links.run")[0];
check(post !== undefined && JSON.stringify(post.params.scope).includes("col-1"), "Run sends the chosen collection");
check(page.callsTo("GET", "/api/links/runs").length >= 2, "and reads the runs again afterwards");

finish();
