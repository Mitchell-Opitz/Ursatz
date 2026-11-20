/* The Subjects tool (views/subjects-view.js) on the fake page: the list shows each subject with its counts, New sends the typed subject and
 * draws the list again, and a row opens the detail. Plain Node script, run through ctest. */
import { renderSubjectsView } from "../public/views/subjects-view.js";
import { check, same, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const subjects = [{ id: "sub-1", name: "Hero", kind: "character", item_count: 2, piece_count: 1, related_count: 0 }];
let created = false;
const notified = [];
const page = await openPage({
  "GET /api/subjects": () => ({ ok: true, subjects: created ? [...subjects, { id: "sub-2", name: "Longing", kind: "idea", item_count: 0, piece_count: 0, related_count: 0 }] : subjects }),
  "POST /api/workshop/command": byCommand({
    "subject.create": () => {
      created = true;
      return { ok: true, subject: { id: "sub-2", name: "Longing", kind: "idea" } };
    },
  }),
  "GET /api/subjects/sub-1": { ok: true, subject: { ...subjects[0], note: "", items: [], pieces: [], related: [] } },
  "GET /api/materials/items": { ok: true, items: [] },
  "GET /api/analyzer/pieces": { ok: true, pieces: [] },
});
page.setShell({ notifySubjectsChanged: () => notified.push(true), leaveWorkspace() {}, setActiveTool() {}, showAnalysis() {}, openItem() {} });

await renderSubjectsView();
await page.settle();
const rows = page.content.querySelectorAll("table tr.clickable");
check(rows.length === 1 && same(rows[0].querySelectorAll("td").map((td) => td.textContent), ["Hero", "Characters", "2", "1", "0"]), "the list shows a subject with its kind and counts");

const [name] = page.content.querySelectorAll("input");
const kind = page.content.querySelector("select");
check(kind.options.map((o) => o.textContent).join() === "Characters,Places,Ideas,Notes", "the kind select has the four kinds");
name.value = "Longing";
kind.value = "idea";
byText(page.content, "button", "New")[0].click();
await page.settle();
const post = page.commands("subject.create")[0];
check(post && post.params.name === "Longing" && post.params.kind === "idea", "New sends the typed name and kind");
check(notified.length === 1, "and tells the shell the subjects changed");
check(page.content.querySelectorAll("table tr.clickable").length === 2, "the list is drawn again with the new subject");

page.content.querySelectorAll("table tr.clickable")[0].click();
await page.settle();
check(page.callsTo("GET", "/api/subjects/sub-1").length === 1 && page.content.textContent.includes("Hero"), "a row opens the subject's detail");

finish();
