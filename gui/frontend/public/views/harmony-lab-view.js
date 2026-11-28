import { el as element } from "../core/dom.js";
import { commandJobResult, commandResult, queryResult } from "../core/api.js";
import Vocabulary from "../core/vocabulary.js";
import HarmonyLabModel from "../models/harmony-lab-model.js";
import ScoreWorkspace from "../score/score-workspace.js";
import { shell } from "./shell-context.js";

/* The Harmony Lab (R-769): a harmony plan heard before any melody. The palette of the key's mode and a second mode as chips, a bar timeline filled by
 * chips or typed chord text, spans and shapes, Hear (nothing is saved), variants, Save as a harmony item, Keep as my schema and Apply to a theme or a
 * piece section. The request, the chips and the commands are the model's (harmony-lab-model.js); the music is the Composer's. */

/* What draws the score and plays it: the score workspace, or a test double (setHarmonyLabScore). */
let mounter = (host, options) => ScoreWorkspace.mount(host, options);

export function setHarmonyLabScore(fn) {
  mounter = fn;
}

const lab = { state: null, chips: [], schemas: [], focused: 0, heard: null, saved: [], projectId: "", blobUrls: [], workspace: null };

function select(options, value, onChange, placeholder) {
  const node = element("select");
  if (placeholder !== undefined) {
    const blank = element("option", "", placeholder);
    blank.value = "";
    node.appendChild(blank);
  }
  for (const option of options) {
    const item = element("option", "", String(option.label));
    item.value = String(option.value);
    node.appendChild(item);
  }
  node.value = String(value === undefined || value === null ? "" : value);
  node.addEventListener("change", () => onChange(node.value));
  return node;
}

function labeled(label, control) {
  const row = element("label", "lab-field");
  row.appendChild(element("span", "lab-label", label));
  row.appendChild(control);
  return row;
}

function numberInput(value, onChange, attributes) {
  const node = element("input");
  node.type = "number";
  node.value = String(value);
  Object.assign(node, attributes || {});
  node.addEventListener("change", () => onChange(Number(node.value)));
  return node;
}

function say(text, isError) {
  const status = document.getElementById("lab-status");
  if (status) {
    status.textContent = text || "";
    status.classList.toggle("error", Boolean(isError));
  }
}

function rerender() {
  const host = document.getElementById("lab-page");
  if (host) {
    paint(host);
  }
}

function change(changes) {
  lab.state = { ...lab.state, ...changes };
  rerender();
}

/* The palette of the key and the second mode (knowledge.palette): the chips. */
async function loadPalette() {
  const answer = await queryResult("knowledge.palette", HarmonyLabModel.paletteParams(lab.state));
  lab.chips = answer.ok ? HarmonyLabModel.chipsFrom(answer.data) : [];
  if (!answer.ok) {
    say(HarmonyLabModel.errorText(answer.data, "The palette could not be read"), true);
  }
  rerender();
}

async function loadSchemas() {
  const answer = await queryResult("composer.progression", { list_schemas: true });
  lab.schemas = answer.ok && Array.isArray(answer.data.progression_schemas) ? answer.data.progression_schemas : [];
}

async function loadSaved() {
  const answer = await queryResult("item.list", { type: "harmony" });
  lab.saved = answer.ok && Array.isArray(answer.data.items) ? answer.data.items : [];
  const projects = await queryResult("project.list", {});
  const first = projects.ok && Array.isArray(projects.data.projects) ? projects.data.projects[0] : null;
  lab.projectId = first ? first.id : "";
}

/* Marks the field a refusal is about and says what it said. */
function refused(data, fallback) {
  const field = HarmonyLabModel.fieldForPath(data && data.path, lab.state);
  const where = field ? ` (${field.field}${field.bar ? ` bar ${field.bar}` : field.index !== undefined ? ` ${field.index + 1}` : ""})` : "";
  say(`${HarmonyLabModel.errorText(data, fallback)}${where}`, true);
}

function revokeBlobs() {
  if (lab.workspace) {
    lab.workspace.destroy();
    lab.workspace = null;
  }
  for (const url of lab.blobUrls) {
    URL.revokeObjectURL(url);
  }
  lab.blobUrls = [];
}

function mountScore(host, answer, title) {
  revokeBlobs();
  host.innerHTML = "";
  const xml = URL.createObjectURL(new Blob([answer.musicxml], { type: "application/xml" }));
  const playback = URL.createObjectURL(new Blob([JSON.stringify(answer.playback)], { type: "application/json" }));
  lab.blobUrls = [xml, playback];
  lab.workspace = mounter(host, { title, subtitle: "harmony plan", musicxmlUrl: xml, playbackUrl: playback });
}

async function hear() {
  say("Writing...");
  const answer = await queryResult("composer.progression", HarmonyLabModel.requestFromState(lab.state));
  if (!answer.ok) {
    refused(answer.data, "The plan could not be written");
    return;
  }
  say("");
  lab.heard = answer.data;
  rerender();
  mountScore(document.getElementById("lab-score"), answer.data, lab.state.name || "Harmony plan");
}

async function variants() {
  say("Writing variants...");
  const answer = await queryResult("composer.progression", HarmonyLabModel.requestFromState(lab.state, { suggest: 4 }));
  if (!answer.ok) {
    refused(answer.data, "The variants could not be written");
    return;
  }
  say("");
  lab.variants = Array.isArray(answer.data.alternatives) ? answer.data.alternatives : [];
  rerender();
}

/* The units of the page for a result's chord texts (a variant "used"): each bar's text read back as the page's units. */
function useVariant(alternative) {
  const request = { ...HarmonyLabModel.requestFromState(lab.state), palette: { ...HarmonyLabModel.requestFromState(lab.state).palette, chords: alternative.harmony.chords } };
  lab.state = { ...HarmonyLabModel.stateFromRequest(request, lab.state.item), name: lab.state.name };
  rerender();
}

async function save() {
  say("Saving...");
  const state = lab.state;
  let id = state.item ? state.item.id : "";
  if (!state.item) {
    const made = await commandResult("item.create", { params: { type: "harmony", name: state.name || "Harmony plan", project_id: lab.projectId, body: HarmonyLabModel.itemBody(state) } });
    if (!made.ok) {
      refused(made.data, "The plan could not be saved");
      return;
    }
    id = made.data.item.id;
  } else {
    const saved = await commandResult("item.save", { target: id, params: { body: HarmonyLabModel.itemBody(state), note: "Edited in the Harmony Lab" } });
    if (!saved.ok) {
      refused(saved.data, "The plan could not be saved");
      return;
    }
  }
  const composed = await commandJobResult("item.compose", { target: id, params: state.seed ? { seed: Number(state.seed) } : {} });
  if (!composed.ok) {
    refused(composed.data, "The plan could not be composed");
    return;
  }
  const item = composed.data && composed.data.item ? composed.data.item : { id, current_version: 0 };
  lab.state = { ...lab.state, item: { id, version: item.current_version || 0 } };
  await loadSaved();
  say("Saved.");
  rerender();
}

async function keepSchema(name, slug) {
  if (!lab.state.item) {
    say("Save the plan first.", true);
    return;
  }
  const answer = await commandResult("knowledge.keep-schema", { params: { item: lab.state.item.id, version: lab.state.item.version, name, slug, loop: true } });
  if (!answer.ok) {
    refused(answer.data, "The schema could not be kept");
    return;
  }
  say(`Kept as ${answer.data.entry.id}.`);
  await loadSchemas();
  rerender();
}

async function apply(targetId, section) {
  if (!targetId) {
    say("Choose a theme or a piece first.", true);
    return;
  }
  if (!lab.state.item) {
    say("Save the plan first.", true);
    return;
  }
  const answer = await commandResult("item.apply-harmony", { target: targetId, params: Object.assign({ harmony: lab.state.item.id, version: lab.state.item.version }, section ? { section } : {}) });
  say(answer.ok ? "Applied." : HarmonyLabModel.errorText(answer.data, "The plan could not be applied"), !answer.ok);
}

function chipsBar() {
  const bar = element("div", "lab-chips");
  for (const chip of lab.chips) {
    const button = element("button", `lab-chip lab-chip-${chip.kind}`, `${chip.name} ${chip.numeral}`);
    button.type = "button";
    button.title = `${chip.text} (${chip.word})`;
    button.addEventListener("click", () => {
      lab.state = HarmonyLabModel.applyChip(lab.state, lab.focused, chip);
      rerender();
    });
    bar.appendChild(button);
  }
  return bar;
}

function timeline() {
  const grid = element("div", "lab-timeline");
  const per = lab.state.chordsPerBar;
  lab.state.units.forEach((value, index) => {
    const cell = element("div", "lab-unit");
    if (index % per === 0) {
      cell.appendChild(element("span", "lab-bar", `Bar ${index / per + 1}`));
    }
    const input = element("input");
    input.type = "text";
    input.value = value;
    input.setAttribute("aria-label", `chord ${index + 1}`);
    input.addEventListener("focus", () => {
      lab.focused = index;
    });
    input.addEventListener("change", () => {
      const units = lab.state.units.slice();
      units[index] = input.value.trim();
      lab.state = { ...lab.state, units };
    });
    cell.appendChild(input);
    grid.appendChild(cell);
  });
  return grid;
}

function arcRow(label, key) {
  const row = element("div", "lab-row");
  const arc = lab.state[key];
  row.appendChild(labeled(label, select(HarmonyLabModel.SHAPES.map((shape) => ({ value: shape, label: shape })), arc.shape, (value) => change({ [key]: { ...arc, shape: value } }), "none")));
  if (arc.shape === "peak" || arc.shape === "step") {
    row.appendChild(labeled("at bar", numberInput(arc.at, (value) => change({ [key]: { ...arc, at: value } }), { min: 1, max: lab.state.bars })));
  }
  return row;
}

function spanRows() {
  const box = element("div", "lab-spans");
  lab.state.spans.forEach((span, index) => {
    const row = element("div", "lab-row lab-span");
    const update = (changes) => change({ spans: lab.state.spans.map((item, at) => (at === index ? { ...item, ...changes } : item)) });
    row.appendChild(labeled("from bar", numberInput(span.from, (value) => update({ from: value }), { min: 1, max: lab.state.bars })));
    row.appendChild(labeled("to bar", numberInput(span.to, (value) => update({ to: value }), { min: 1, max: lab.state.bars })));
    row.appendChild(labeled("chords", select(HarmonyLabModel.POOLS.map((pool) => ({ value: pool, label: pool })), span.chords, (value) => update({ chords: value }))));
    row.appendChild(labeled("schema", select(lab.schemas.map((schema) => ({ value: schema.id, label: schema.name })), span.schema, (value) => update({ schema: value }), "none")));
    const remove = element("button", "", "Remove");
    remove.type = "button";
    remove.addEventListener("click", () => change({ spans: lab.state.spans.filter((item, at) => at !== index) }));
    row.appendChild(remove);
    box.appendChild(row);
  });
  const add = element("button", "", "Add a span");
  add.type = "button";
  add.addEventListener("click", () => change({ spans: [...lab.state.spans, { from: 1, to: lab.state.bars, chords: "any", schema: "", loop: null }] }));
  box.appendChild(add);
  return box;
}

function measuresBlock() {
  const box = element("div", "lab-measures");
  if (!lab.heard) {
    return box;
  }
  for (const row of HarmonyLabModel.measureRows(lab.heard.measures)) {
    box.appendChild(element("span", "lab-measure", `bar ${row.bar}: tension ${row.tension}, brightness ${row.brightness}`));
  }
  return box;
}

async function pickTarget(type) {
  const answer = await queryResult("item.list", { type });
  return answer.ok && Array.isArray(answer.data.items) ? answer.data.items : [];
}

function applyBlock() {
  const box = element("div", "lab-apply");
  const type = select([{ value: "theme", label: "theme" }, { value: "piece", label: "piece" }], "theme", () => {});
  const target = select([], "", () => {}, "choose an item");
  const section = element("input");
  section.type = "text";
  section.placeholder = "section (a piece)";
  const fill = async () => {
    const items = await pickTarget(type.value);
    target.innerHTML = "";
    const blank = element("option", "", "choose an item");
    blank.value = "";
    target.appendChild(blank);
    for (const item of items) {
      const option = element("option", "", item.name || item.id);
      option.value = item.id;
      target.appendChild(option);
    }
  };
  type.addEventListener("change", fill);
  const go = element("button", "", "Apply");
  go.type = "button";
  go.addEventListener("click", () => apply(target.value, type.value === "piece" ? section.value.trim() : ""));
  box.append(labeled("Apply to", type), target, section, go);
  fill();
  return box;
}

/* A saved plan opened: its request as the page's fields. */
async function load(id) {
  const answer = await queryResult("item.get", { id });
  if (!answer.ok) {
    refused(answer.data, "The plan could not be opened");
    return;
  }
  const item = answer.data.item || {};
  lab.state = HarmonyLabModel.stateFromRequest(answer.data.body && answer.data.body.request, { id, version: item.current_version || 0 });
  lab.state.name = item.name || "";
  lab.heard = null;
  await loadPalette();
}

function savedBlock() {
  const row = element("div", "lab-row lab-saved");
  const choice = select(lab.saved.map((item) => ({ value: item.id, label: item.name || item.id })), lab.state.item ? lab.state.item.id : "", () => {}, "saved plans");
  const open = element("button", "", "Open");
  open.type = "button";
  open.addEventListener("click", () => choice.value && load(choice.value));
  row.append(labeled("Saved", choice), open);
  return row;
}

function paint(host) {
  const state = lab.state;
  host.innerHTML = "";
  host.appendChild(savedBlock());
  const keyRow = element("div", "lab-row");
  const materials = Vocabulary.materialChoices("materials");
  keyRow.appendChild(labeled("tonic", select(HarmonyLabModel.tonicChoices(state.material), state.tonic, (value) => change({ tonic: Number(value) }))));
  keyRow.appendChild(labeled("mode", select(materials, state.material, (value) => { lab.state = { ...lab.state, material: value }; loadPalette(); })));
  keyRow.appendChild(labeled("with", select(materials, state.withMaterial, (value) => { lab.state = { ...lab.state, withMaterial: value }; loadPalette(); }, "none")));
  keyRow.appendChild(labeled("meter", select(Vocabulary.list("meters"), state.meter, (value) => change(HarmonyLabModel.resize(state, { meter: value })))));
  keyRow.appendChild(labeled("bars", numberInput(state.bars, (value) => change(HarmonyLabModel.resize(state, { bars: Math.max(1, Math.min(64, value || 1)) })), { min: 1, max: 64 })));
  keyRow.appendChild(labeled("chords a bar", select(HarmonyLabModel.chordsPerBarChoices(state.meter).map((count) => ({ value: count, label: count })), state.chordsPerBar, (value) => change(HarmonyLabModel.resize(state, { chordsPerBar: Number(value) })))));
  keyRow.appendChild(labeled("cadence", select([{ value: "none", label: "none" }, { value: "PAC", label: "PAC" }, { value: "HC", label: "HC" }], state.cadence, (value) => change({ cadence: value }))));
  host.appendChild(keyRow);
  host.appendChild(element("h2", "", "Palette"));
  host.appendChild(chipsBar());
  host.appendChild(element("h2", "", "Bars"));
  host.appendChild(timeline());
  host.appendChild(measuresBlock());
  host.appendChild(element("h2", "", "Spans"));
  host.appendChild(spanRows());
  host.appendChild(element("h2", "", "Shapes"));
  host.appendChild(arcRow("brightness", "brightness"));
  host.appendChild(arcRow("tension", "tension"));
  const accompaniment = element("div", "lab-row");
  accompaniment.appendChild(labeled("accompaniment", select(Vocabulary.list("accompaniment_patterns").filter((pattern) => pattern.value !== "any"), state.accompaniment.pattern, (value) => change({ accompaniment: { ...state.accompaniment, pattern: value } }), "none")));
  accompaniment.appendChild(labeled("voicing", select(Vocabulary.list("voicings"), state.accompaniment.voicing, (value) => change({ accompaniment: { ...state.accompaniment, voicing: value } }), "none")));
  host.appendChild(accompaniment);
  const actions = element("div", "lab-actions");
  const name = element("input");
  name.type = "text";
  name.placeholder = "name";
  name.value = state.name;
  name.addEventListener("change", () => { lab.state = { ...lab.state, name: name.value }; });
  const seed = element("input");
  seed.type = "text";
  seed.placeholder = "seed";
  seed.value = state.seed;
  seed.addEventListener("change", () => { lab.state = { ...lab.state, seed: seed.value }; });
  const buttons = [["Hear", hear], ["Variants", variants], ["Save", save]].map(([label, action]) => {
    const button = element("button", "", label);
    button.type = "button";
    button.addEventListener("click", action);
    return button;
  });
  actions.append(name, seed, ...buttons);
  host.appendChild(actions);
  host.appendChild(element("div", "", ""));
  const keep = element("div", "lab-row");
  const schemaName = element("input");
  schemaName.type = "text";
  schemaName.placeholder = "schema name";
  const schemaSlug = element("input");
  schemaSlug.type = "text";
  schemaSlug.placeholder = "slug";
  const keepButton = element("button", "", "Keep as my schema");
  keepButton.type = "button";
  schemaName.addEventListener("change", () => { if (!schemaSlug.value) { schemaSlug.value = HarmonyLabModel.slugOf(schemaName.value); } });
  keepButton.addEventListener("click", () => keepSchema(schemaName.value, schemaSlug.value));
  keep.append(schemaName, schemaSlug, keepButton);
  host.appendChild(keep);
  host.appendChild(applyBlock());
  if (Array.isArray(lab.variants) && lab.variants.length > 0) {
    host.appendChild(element("h2", "", "Variants"));
    lab.variants.forEach((alternative, index) => {
      const row = element("div", "lab-row lab-variant");
      row.appendChild(element("span", "", `${index + 1}: ${alternative.harmony.chords.map((chord) => chord.text).join(" | ")}`));
      const use = element("button", "", "Use");
      use.type = "button";
      use.addEventListener("click", () => useVariant(alternative));
      row.appendChild(use);
      host.appendChild(row);
    });
  }
}

export async function renderHarmonyLabView() {
  const page = shell().contentEl();
  page.innerHTML = "";
  page.appendChild(element("h1", "view-title", "Harmony Lab"));
  const status = element("div", "upload-status");
  status.id = "lab-status";
  status.setAttribute("role", "status");
  page.appendChild(status);
  const host = element("div", "");
  host.id = "lab-page";
  page.appendChild(host);
  const score = element("div", "");
  score.id = "lab-score";
  page.appendChild(score);
  if (!lab.state) {
    lab.state = HarmonyLabModel.initialState();
  }
  paint(host);
  await Promise.all([loadPalette(), loadSchemas(), loadSaved()]);
  rerender();
}

/* The saved plans follow the workshop: the shell reloads the lab's list when a harmony item changes. */
export async function refreshHarmonyLab() {
  if (document.getElementById("lab-page")) {
    await loadSaved();
  }
}
