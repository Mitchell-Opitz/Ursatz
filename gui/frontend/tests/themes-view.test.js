/* The theme editor (views/themes-view.js) on the fake page: its Theme and Structure sections show the form from the body and the vocabulary, a
 * changed meter reaches the form state, Compose without a version note for a changed form is refused in the status line, and Compose of an
 * unchanged form sends the compose request. Plain Node script, run through ctest (it reads the vocabulary the build generates). */
import MaterialsModel from "../public/models/materials-model.js";
import ThemesModel from "../public/models/themes-model.js";
import { buildThemeEditor } from "../public/views/themes-view.js";
import { check, finish } from "./support/check.js";
import { byCommand, openPage } from "./support/page.js";

const page = await openPage({
  "POST /api/workshop/command": byCommand({ "item.compose": { ok: true, version: 2 } }),
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

/* A block accompaniment takes a voicing from the vocabulary (R-731): the select is there, hidden until a block pattern is asked for, and its
 * choice reaches the form state. */
function selectLabelled(text) {
  for (const section of Object.values(editor.sections)) {
    for (const label of section.querySelectorAll("label")) {
      if (label.textContent.trim().startsWith(text)) {
        return label.querySelector("select");
      }
    }
  }
  return null;
}
const kind = selectLabelled("Accompaniment");
const pattern = selectLabelled("Pattern");
const voicing = selectLabelled("Voicing");
check(kind && pattern && voicing, "the accompaniment section has a Voicing select");
check(voicing.options.map((o) => o.value).join() === ["", ...ThemesModel.VOICINGS.map((entry) => entry.value)].join() && ThemesModel.VOICINGS.length >= 4, "it offers none and the vocabulary's voicings");
check(voicing.parentElement.hidden === true, "it is hidden while there is no accompaniment pattern");
kind.value = "pattern";
kind.dispatchEvent({ type: "input" });
pattern.value = "block";
pattern.dispatchEvent({ type: "input" });
check(voicing.parentElement.hidden === false, "it shows for a block pattern");
voicing.value = "omt.voicing.shell-dominant";
voicing.dispatchEvent({ type: "input" });
check(editor.state.accompaniment.voicing === "omt.voicing.shell-dominant" && ThemesModel.themeOfBody(ThemesModel.bodyFromForm(editor.state)).accompaniment.voicing === "omt.voicing.shell-dominant", "its choice reaches the form state and the body");
pattern.value = "alberti";
pattern.dispatchEvent({ type: "input" });
check(editor.state.accompaniment.voicing === "" && voicing.parentElement.hidden === true, "another pattern drops the voicing");
kind.value = "none";
kind.dispatchEvent({ type: "input" });

await editor.compose({});
check(statusEl.textContent === "Give a version note." && statusEl.classList.contains("error"), "Compose of a changed form without a version note is refused");
check(page.commands("item.compose").length === 0, "and sends nothing");

meter.value = state.meter;
meter.dispatchEvent({ type: "input" });
await editor.compose({});
await page.settle();
const compose = page.commands("item.compose")[0];
check(compose !== undefined && compose.target === "mat-1" && saved.length === 1, "Compose of an unchanged form sends the compose request and tells the page it is done");

finish();
