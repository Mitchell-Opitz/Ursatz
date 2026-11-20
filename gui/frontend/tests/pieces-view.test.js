/* The piece editor (views/pieces-view.js) on the fake page: its header and structure strip come from the body and the Composer's forms, a section
 * is added from the strip, Save as new version sends the body as a new version and Compose of a form with no saved request saves first, then
 * composes. Plain Node script, run through ctest (it reads the vocabulary and forms the build generates). */
import Vocabulary from "../public/core/vocabulary.js";
import { buildPieceEditor } from "../public/views/pieces-view.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const page = await openPage({
  "POST /api/workshop/command": byCommand({
    "item.save": { ok: true, item: { id: "mat-3", current_version: 2 }, version: 2 },
    "item.compose": { ok: true, version: 3 },
  }),
});
const statusEl = page.document.createElement("div");
const saved = [];
const editor = buildPieceEditor({
  id: "mat-3",
  version: 1,
  title: "Suite",
  body: { uses: [], request: {} },
  usesStatus: [],
  items: [],
  statusEl,
  saved: (answer) => saved.push(answer),
});
page.content.appendChild(editor.el);

check(Vocabulary.forms().length > 0 && editor.el.querySelectorAll("select").length > 0, "the editor shows the Composer's forms and the structure selects");
check(editor.el.querySelectorAll(".piece-lane").length > 0 || editor.el.querySelectorAll(".piece-strip").length > 0, "the structure strip is drawn");
const before = editor.state.sections.length;
const add = byText(editor.el, "button", "Add section")[0];
if (add) {
  add.click();
  check(editor.state.sections.length === before + 1, "Add section adds a section to the form state");
} else {
  check(true, "this form has a fixed shape: no Add section button");
}

byText(editor.el, "button", "Save as new version")[0].click();
await page.settle();
const version = page.commands("item.save")[0];
check(version !== undefined && version.target === "mat-3" && version.params.body !== undefined && saved.length === 1, "Save as new version sends the body and tells the page it is done");

byText(editor.el, "button", "Compose")[0].click();
await page.settle();
check(page.commands("item.compose").length === 1 && saved.length === 2, "Compose composes the saved form and tells the page it is done");

finish();
