/* The theme editor (views/themes-view.js) on the fake page: its Theme and Structure sections show the form from the body and the vocabulary, a
 * changed meter reaches the form state, Compose without a version note for a changed form is refused in the status line, and Compose of an
 * unchanged form sends the compose request. Plain Node script, run through ctest (it reads the vocabulary the build generates). */
import MaterialsModel from "../public/models/materials-model.js";
import ThemesModel from "../public/models/themes-model.js";
import { buildThemeEditor } from "../public/views/themes-view.js";
import { check, finish } from "./support/check.js";
import { openPage } from "./support/page.js";

const page = await openPage({
  "POST /api/materials/items/mat-1/compose": { ok: true, version: 2 },
  "GET /api/materials/items/mat-1": { ok: true, item: { id: "mat-1", type: "theme", name: "Sentence", current_version: 1 }, body: {} },
});
const state = ThemesModel.emptyState("mat-1");
const statusEl = page.document.createElement("div");
const nameInput = page.document.createElement("input");
nameInput.value = "Sentence";
const projectSelect = page.document.createElement("select");
const saved = [];
const editor = buildThemeEditor({
  id: "mat-1",
  item: { id: "mat-1", type: "theme", name: "Sentence", current_version: 1, project_id: "prj-1" },
  body: ThemesModel.bodyFromForm(state),
  usesStatus: [],
  items: [],
  status: MaterialsModel.motifTabStatus(statusEl),
  openTab: () => "theme",
  statusText: () => statusEl.textContent,
  nameInput,
  projectSelect,
  saved: () => saved.push(true),
});

const theme = editor.sections.theme;
const labels = theme.querySelectorAll("label").map((label) => label.textContent.trim().split(" ")[0]);
check(labels.join() === "Mode,Meter,Tempo", "the Theme section has mode, meter and tempo");
const meter = theme.querySelectorAll("select")[1];
check(meter.options.map((o) => o.value).includes("3/4") && meter.value === state.meter, "the meter select offers the vocabulary's meters and shows the form's");

const structure = editor.sections.structure;
check(structure.textContent.includes("Form") && structure.querySelectorAll("select").length >= 6, "the Structure section has the form's selects");

meter.value = "3/4";
meter.dispatchEvent({ type: "input" });
check(editor.state.meter === "3/4", "a changed meter reaches the form state");

await editor.compose({});
check(statusEl.textContent === "Give a version note." && statusEl.classList.contains("error"), "Compose of a changed form without a version note is refused");
check(page.callsTo("POST", "/api/materials/items/mat-1/compose").length === 0, "and sends nothing");

meter.value = state.meter;
meter.dispatchEvent({ type: "input" });
await editor.compose({});
await page.settle();
const compose = page.callsTo("POST", "/api/materials/items/mat-1/compose")[0];
check(compose !== undefined && saved.length === 1, "Compose of an unchanged form sends the compose request and tells the page it is done");

finish();
