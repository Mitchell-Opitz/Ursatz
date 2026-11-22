/* The motif editor of the motif detail page (views/motif-detail-view.js) on the fake page: its fields come from the body of a motif, its values are what
 * the fields hold, and Save as new version sends the body as a new version of the motif with the note typed; the motif page shows the tabs of a motif.
 * Plain Node script, run through ctest (it reads the vocabulary the build generates). */
import MaterialsModel from "../public/models/materials-model.js";
import MotifDetailView, { buildMotifEditor, buildMotifSaveControls } from "../public/views/motif-detail-view.js";
import { sendMaterialsRequest } from "../public/views/materials-common.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const page = await openPage({
  "POST /api/workshop/command": byCommand({ "item.save": { ok: true, item: { id: "mat-1", current_version: 2 }, version: 2 } }),
});
const store = {};
globalThis.localStorage = { getItem: (key) => (key in store ? store[key] : null), setItem: (key, value) => { store[key] = value; } };

const body = {
  uses: [],
  motif: { role: "melodic", form: "full", notes: "C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e", meter: "4/4", key: "C minor", pickup: "0", features: [], analysis: null, derived: null },
};

/* The editor shows the body: the role, the form and the notes of the motif, and gives them back as the values it holds. */
const statusEl = page.document.createElement("div");
const editor = buildMotifEditor(body, MaterialsModel.motifTabStatus(statusEl));
page.content.appendChild(editor.el);
const values = editor.values();
check(values.role === "melodic" && values.form === "full" && values.meter === "4/4", "the editor's values are the body's role, form and meter");
check(values.notes === body.motif.notes, "and its notes");
const notes = editor.el.querySelector("textarea");
check(notes.value === body.motif.notes, "the notes field shows the notes");

/* Save as new version sends the body (with the notes typed) as a new version of the motif, and the note with it. */
let answered = null;
const controls = buildMotifSaveControls(editor, statusEl, "Save as new version", true, async (typed, note) => {
  answered = await sendMaterialsRequest(MaterialsModel.buildSaveMotifRequest({ id: "mat-1", note, body: MaterialsModel.buildMotifBody(typed, body) }));
  return answered;
});
page.content.appendChild(controls);
notes.value = "C4:q Eb4:e G4:e C5:h";
controls.querySelector("input").value = "shorter";
byText(controls, "button", "Save as new version")[0].click();
await page.settle();
const saved = page.commands("item.save")[0];
check(saved !== undefined && saved.target === "mat-1", "Save as new version sends item.save for the motif");
check(saved.params.body.motif.notes === "C4:q Eb4:e G4:e C5:h" && saved.params.body.motif.role === "melodic", "the body carries the notes typed");
check(saved.params.note === "shorter", "and the version note");
check(answered && answered.ok === true, "the answer comes back to the page");

/* The page of a motif: the four tabs, the Motif tab open, built from the item. */
const item = { id: "mat-1", type: "motif", name: "Source", project_id: "prj-1", current_version: 1, versions: [{ number: 1, created_at: "2026-11-01T10:00:00Z", note: "" }] };
const detail = page.document.createElement("div");
const nameInput = page.document.createElement("input");
nameInput.value = "Source";
MotifDetailView.render(detail, {
  id: "mat-1",
  item,
  data: { item, body, used_by: [], subjects: [], variants: [] },
  nameInput,
  projectSelect: page.document.createElement("select"),
  analysisError: null,
  rerender: () => {},
});
const labels = detail.querySelectorAll(".analysis-tabs button").map((b) => b.textContent);
check(labels.join() === MaterialsModel.MOTIF_TABS.map((t) => t.label).join(), "the page has the motif tabs");

finish();
