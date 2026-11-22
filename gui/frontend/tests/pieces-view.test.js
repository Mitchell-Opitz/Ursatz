/* The piece editor (views/pieces-view.js) on the fake page: its header and structure strip come from the body and the Composer's forms, a section
 * is added from the strip, Save as new version sends the body as a new version and Compose of a form with no saved request saves first, then
 * composes. Plain Node script, run through ctest (it reads the vocabulary and forms the build generates). */
import Vocabulary from "../public/core/vocabulary.js";
import PiecesModel from "../public/models/pieces-model.js";
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
/* The structure strip of a form whose request lists its sections (the game loop): one lane for each of its four parts (intro, body, turnaround and
   ending), the body with its first section; each lane has an Add section button, and the one of the body adds a section to the state. */
check(editor.state.form === "game-loop" && editor.state.sections.length === 1, "the piece starts on the game loop with one section");
check(editor.el.querySelectorAll(".piece-strip").length === 1 && editor.el.querySelectorAll(".piece-lane").length === 4, "the strip has one lane for each of the four parts");
const before = editor.state.sections.length;
const add = byText(editor.el, "button", "Add section");
check(add.length === 4, "each lane has an Add section button");
add[1].click();
check(editor.state.sections.length === before + 1, "Add section of the body adds a section to the form state");

/* A sized form (the ternary at 24 bars) has the Composer's sections of its size: one lane holding a block for each. */
const sized = buildPieceEditor({
  id: "mat-4",
  version: 1,
  title: "Ternary",
  body: { uses: [], request: { form: "ternary", bars: 24 } },
  usesStatus: [],
  items: [],
  statusEl,
  saved: () => {},
});
page.content.appendChild(sized.el);
const planned = PiecesModel.sectionsOf(PiecesModel.formOf(sized.state.form), sized.state.bars, sized.state.count);
check(planned.length === 3, "the ternary at 24 bars has three sections");
check(sized.el.querySelectorAll(".piece-lane").length === 1 && sized.el.querySelectorAll(".piece-block").length === planned.length, "the strip draws one block for each");
check(byText(sized.el, "button", "Add section").length === 0, "a sized form has no Add section button");

byText(editor.el, "button", "Save as new version")[0].click();
await page.settle();
const version = page.commands("item.save")[0];
check(version !== undefined && version.target === "mat-3" && version.params.body !== undefined && saved.length === 1, "Save as new version sends the body and tells the page it is done");

byText(editor.el, "button", "Compose")[0].click();
await page.settle();
check(page.commands("item.compose").length === 1 && saved.length === 2, "Compose composes the saved form and tells the page it is done");

finish();
