/* The shell (views/shell.js) on the fake page: boot reads the vocabulary, fills the tool list, mounts the Tags and Subjects panels and opens the
 * Library; choosing another tool draws it in the content area. Plain Node script, run through ctest (it reads the vocabulary the build generates). */
import { boot } from "../public/views/shell.js";
import { check, finish } from "./support/check.js";
import { generated, readJson } from "./support/fixtures.js";
import { openPage } from "./support/page.js";

const page = await openPage(
  {
    "GET /api/composer/vocabulary": readJson(generated("vocabulary.json")),
    "GET /api/composer/forms": readJson(generated("forms.json")),
    "GET /api/analyzer/pieces": { ok: true, pieces: [{ id: "p1", filename: "nocturne.mid", composer: "Field", source: "", tags: ["slow"], analysis_status: "current" }] },
    "GET /api/analyzer/groups": { ok: true, composers: [{ value: "Field", count: 1 }], sources: [], tags: [{ value: "slow", count: 1 }], unassigned: { composer: 0, source: 1 }, untagged: 0, total: 1 },
    "GET /api/subjects/groups": { ok: true, groups: [{ kind: "character", subjects: [{ id: "s1", name: "Hero", link_count: 2 }] }] },
    "GET /api/materials/projects": { ok: true, projects: [{ id: "prj-1", name: "General", item_count: 0 }] },
    "GET /api/corpus": { ok: true, corpora: [] },
    "GET /api/subjects": { ok: true, subjects: [] },
  },
  { shell: false },
);
await boot();
await page.settle(12);

const tools = page.document.getElementById("tool-list").children.map((li) => li.textContent);
check(tools.join(",") === "Library,Analysis,Collections,Motifs,Themes,Pieces,Harmony Lab,Subjects,Motive links", "boot lists the nine tools");
check(page.document.getElementById("tool-list").children[0].classList.contains("active"), "the Library is the open tool");
check(page.content.textContent.includes("nocturne.mid"), "the Library table lists the pieces");
check(page.document.getElementById("tags-panel").textContent.includes("Field"), "the Tags panel lists the composers");
check(page.document.getElementById("subjects-panel").textContent.includes("Hero"), "the Subjects panel lists the subjects");

page.document.getElementById("tool-list").children[2].click();
await page.settle(8);
check(page.document.getElementById("tool-list").children[2].classList.contains("active") && page.content.textContent.includes("Collections"), "choosing Collections draws that tool");
check(page.callsTo("GET", "/api/corpus").length >= 1, "and asks the backend for the collections");

finish();
