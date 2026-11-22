/* The materials tools (views/materials-view.js) on the fake page: the project picker, the list of one type with its rows, New for a theme (a
 * request, then the list again) and New for a motif (the new motif page). Plain Node script, run through ctest (it reads the vocabulary the build
 * generates). */
import { renderMaterialsView } from "../public/views/materials-view.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const projects = [{ id: "prj-1", name: "Work", item_count: 1, description: "", created_at: "2026-11-01T00:00:00Z", updated_at: "2026-11-01T00:00:00Z" }];
const themes = [{ id: "mat-1", type: "theme", name: "Sentence", project_id: "prj-1", current_version: 2, updated_at: "2026-11-02T10:00:00Z", variant_of: null }];
let made = false;
const page = await openPage({
  "GET /api/materials/projects": { ok: true, projects },
  "GET /api/materials/items": () => ({ ok: true, items: made ? [...themes, { ...themes[0], id: "mat-2", name: "Period", updated_at: "2026-11-03T10:00:00Z" }] : themes }),
  "POST /api/workshop/command": byCommand({
    "item.create": () => {
      made = true;
      return { ok: true, item: { id: "mat-2", type: "theme", name: "Period", project_id: "prj-1", current_version: 1 } };
    },
  }),
  "POST /api/materials/items/uses": { ok: true, uses: [] },
});
page.setShell({ activeToolId: () => "themes", openItem() {} });

await renderMaterialsView("theme");
await page.settle();

const select = page.document.getElementById("materials-project-select");
check(select.options.map((o) => o.textContent).includes("Work"), "the project picker lists the projects");
const rows = page.content.querySelectorAll("table tr").slice(1);
check(rows.length === 1 && rows[0].textContent.includes("Sentence") && rows[0].textContent.includes("Work"), "the list shows a theme with its project");
check(page.callsTo("GET", "/api/materials/items?type=theme").length === 1, "the list is asked for by type");

const name = page.content.querySelector("input");
name.value = "Period";
byText(page.content, "button", "New")[0].click();
await page.settle();
const post = page.commands("item.create")[0];
check(post && post.params.type === "theme" && post.params.name === "Period" && post.params.project_id === "prj-1", "New sends the typed name and the project");
check(page.content.querySelectorAll("table tr").length === 3, "the list is drawn again with the new theme");

/* A motif opens its page instead of making the item at once. */
page.setShell({ activeToolId: () => "motifs", openItem() {} });
await renderMaterialsView("motif");
await page.settle();
page.content.querySelector("input").value = "Hook";
const createsBefore = page.commands("item.create").length;
byText(page.content, "button", "New")[0].click();
await page.settle();
check(page.content.querySelector("h1.view-title").textContent === "New motif", "New for a motif opens the page titled New motif");
check(page.content.querySelector(".materials-editor") !== null, "the new motif page holds the motif editor");
check(page.commands("item.create").length === createsBefore, "New for a motif sends no item.create: the item is made when its page is saved");

finish();
