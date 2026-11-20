/* The motif detail page (P18 B3): a header, a tab bar (Motif, Variants, Used by, Other) and one pane per tab. The
 * panes are built once and shown or hidden, so the editor keeps what was typed when another tab is open. The
 * Variants pane mounts the first time it is shown. The pieces (buildMotifEditor, buildMotifScore, buildAnalysisPanel,
 * buildUsedByTable, buildUsesSection, buildVersionsList, buildSubjectsField, VariantTreeView, MotifCompareView)
 * are page globals from materials-view.js and the other view files. */

import Vocabulary from "../core/vocabulary.js";
import { el as element } from "../core/dom.js";
import MaterialsModel from "../models/materials-model.js";
import MotifCompareView from "./motif-compare-view.js";
import VariantTreeView from "./variant-tree-view.js";
import ScoreWorkspace from "../score/score-workspace.js";
import { buildMaterialExport, buildUsedByTable, buildUsesSection, buildVersionsList, materialsState, sendMaterialsRequest } from "./materials-common.js";
import { buildSubjectsField } from "./subjects-view.js";
import { shell } from "./shell-context.js";

/* ctx: {id, item, data (the item answer), analysisError, statusEl, allItems, headerEl, rerender(reason)}.
 * Appends the tab bar and panes to content, inside a `motifs-section` wrapper (up to 1200px wide). */
function render(content, ctx) {
  const { id, item, data } = ctx;
  const bar = element("div", "analysis-tabs");
  bar.setAttribute("role", "tablist");
  const panes = {};
  const buttons = {};
  const mounted = {};
  const builders = {
    motif: (pane) => buildMotifPane(pane, ctx),
    variants: (pane) => buildVariantsPane(pane, ctx),
    "used-by": (pane) => pane.appendChild(buildUsedByTable(data.used_by, item.current_version)),
    other: (pane) => buildOtherPane(pane, ctx),
  };

  let current = null;
  function show(tabId) {
    MaterialsModel.writeMotifTab(tabId);
    if (current === "variants" && tabId !== "variants" && ctx.variants) {
      ctx.variants.leave();
    }
    for (const tab of MaterialsModel.MOTIF_TABS) {
      const active = tab.id === tabId;
      panes[tab.id].hidden = !active;
      buttons[tab.id].classList.toggle("active", active);
      buttons[tab.id].setAttribute("aria-selected", active ? "true" : "false");
    }
    if (!mounted[tabId]) {
      mounted[tabId] = true;
      builders[tabId](panes[tabId]);
    } else if (tabId === "variants" && current !== "variants" && ctx.variants) {
      ctx.variants.enter();
    }
    current = tabId;
  }

  for (const tab of MaterialsModel.MOTIF_TABS) {
    const button = element("button", "", tab.label);
    button.type = "button";
    button.setAttribute("role", "tab");
    button.addEventListener("click", () => show(tab.id));
    buttons[tab.id] = button;
    bar.appendChild(button);
    panes[tab.id] = element("div", "materials-tab-pane");
    panes[tab.id].setAttribute("role", "tabpanel");
  }
  const wrapper = element("div", "motifs-section");
  wrapper.appendChild(buildMaterialExport(id, item.current_version, item.name));
  wrapper.appendChild(bar);
  for (const tab of MaterialsModel.MOTIF_TABS) {
    wrapper.appendChild(panes[tab.id]);
  }
  content.appendChild(wrapper);
  show(MaterialsModel.readMotifTab());
}

/* The score, the editor, the analysis and the save row. */
function buildMotifPane(pane, ctx) {
  const { id, item, data } = ctx;
  const status = MaterialsModel.motifTabStatus(buildStatusArea(pane));
  ctx.motifStatus = status;
  const score = buildMotifScore(id, item, data.body);
  if (score) {
    pane.appendChild(score);
  }
  const editor = buildMotifEditor(data.body, status);
  pane.appendChild(editor.el);
  const subjectsField = buildSubjectsField("items", id, data.subjects);
  const subjectsRow = element("div", "materials-subjects-row");
  subjectsRow.append(element("h2", "section-title", "Subjects"), subjectsField.el);
  pane.appendChild(subjectsRow);
  pane.appendChild(
    buildAnalysisPanel(data.body, ctx.analysisError, {
      status,
      send: () => sendMaterialsRequest(MaterialsModel.buildReanalyzeRequest(id)),
      done: (reason) => ctx.rerender(reason),
    }),
  );
  pane.appendChild(buildSaveRow(editor, ctx));
  const warnings = MaterialsModel.toneWarningText(data.body);
  if (warnings !== "") {
    status.show(warnings, true);
  }
}

/* The status area at the top of a tab: saves, errors and progress of every section of the tab show here. */
function buildStatusArea(pane) {
  const area = element("div", "motif-tab-status");
  area.setAttribute("role", "status");
  pane.appendChild(area);
  return area;
}

/* The one Save of the page: name and project go through the item update, an editor body that differs from the
 * current version writes a new version with the note. */
function buildSaveRow(editor, ctx) {
  const { id, item, data } = ctx;
  const status = ctx.motifStatus;
  const row = element("div", "upload-controls button-row");
  const note = element("input");
  note.type = "text";
  note.placeholder = "What changed? (required)";
  note.size = 40;
  const save = element("button", "button-primary", "Save");
  save.type = "button";
  row.append(note, save);

  save.addEventListener("click", async () => {
    status.clear();
    const values = editor.values();
    const fields = {};
    const typedName = ctx.nameInput.value.trim();
    if (typedName !== "" && typedName !== item.name) {
      fields.name = typedName;
    }
    if (ctx.projectSelect.value !== item.project_id) {
      fields.project_id = ctx.projectSelect.value;
    }
    const bodyChanged = MaterialsModel.motifBodyChanged(values, data.body);
    const decision = MaterialsModel.motifSaveDecision(
      { name: "name" in fields, project: "project_id" in fields, body: bodyChanged },
      note.value,
    );
    if (decision.kind !== "save") {
      status.show(decision.message, true);
      return;
    }
    save.disabled = true;
    status.show("Saving...");
    let reason;
    if (bodyChanged) {
      const saved = await sendMaterialsRequest(
        MaterialsModel.buildSaveMotifRequest({ id, note: note.value, body: MaterialsModel.buildMotifBody(values, data.body) }),
      );
      if (!saved.ok) {
        save.disabled = false;
        const text = MaterialsModel.errorText(saved.data, "Could not save the motif");
        status.show(text, true);
        editor.showError(text, MaterialsModel.fieldForPath(saved.data.path));
        return;
      }
      reason = saved.data.item.analysis_error;
    }
    if (Object.keys(fields).length > 0) {
      const renamed = await sendMaterialsRequest(MaterialsModel.buildUpdateItemRequest(id, fields));
      if (!renamed.ok) {
        save.disabled = false;
        status.show(MaterialsModel.errorText(renamed.data, "Could not save"), true);
        return;
      }
    }
    ctx.rerender(reason);
  });
  return row;
}

function buildVariantsPane(pane, ctx) {
  const { id, item, data } = ctx;
  const variants = VariantTreeView.mount(pane, {
    id,
    name: item.name,
    body: data.body,
    projectId: item.project_id,
    projects: materialsState.projects,
    /* A promoted motif opens on its Motif tab, where subjects can be added. */
    open: (motifId) => {
      MaterialsModel.writeMotifTab("motif");
      shell().openItem("motif", motifId);
    },
    send: sendMaterialsRequest,
  });
  ctx.variants = variants;
}

/* Versions, Derived from and Compare. */
function buildOtherPane(pane, ctx) {
  const { id, item, data, statusEl } = ctx;
  const versionEl = element("div");
  pane.appendChild(buildVersionsList(item, versionEl, statusEl, null));
  pane.appendChild(versionEl);
  pane.appendChild(
    buildUsesSection({
      id,
      type: "motif",
      body: data.body,
      statuses: data.uses_status,
      allItems: ctx.allItems,
      statusEl,
    }),
  );
  MotifCompareView.mount(pane, {
    id,
    version: item.current_version,
    projectId: item.project_id,
    projects: materialsState.projects,
    items: ctx.allItems,
    send: sendMaterialsRequest,
    linked: () => {
      if (ctx.variants) {
        ctx.variants.refresh();
      }
    },
  });
}

/* The motif form. Returns {el, values(), fill, showError(text, field)}.
 * body is the version body being edited, or null for a new motif. */
export function buildMotifEditor(body, tabStatus) {
  const initial = MaterialsModel.motifFormValues(body);
  const el = element("section", "panel materials-editor");
  /* On the motif tab the MIDI upload writes to the tab's status area (tabStatus, a
   * MaterialsModel.motifTabStatus writer). The new-motif page has none and keeps a status line at the end of the
   * panel. */
  let status = tabStatus;
  if (!status) {
    const line = element("div", "upload-status");
    line.setAttribute("role", "status");
    el.appendChild(line);
    status = MaterialsModel.motifTabStatus(line);
  }
  /* The fields sit in a two-column grid: the label in the first, the control with its hints and error under it in
   * the second (R-191). Rows that belong to no field (the MIDI upload) take the control column. */
  const grid = element("div", "motif-form-grid");
  el.appendChild(grid);
  const errors = {};

  function field(labelText, control, key) {
    control.id = `motif-field-${key}`;
    const label = element("label", "motif-form-label", labelText);
    label.htmlFor = control.id;
    const cell = element("div", "motif-form-control");
    cell.appendChild(control);
    errors[key] = element("div", "materials-field-error");
    cell.appendChild(errors[key]);
    grid.append(label, cell);
    return control;
  }

  /* A hint under the control of the field with this key, above its error. */
  function hintUnder(key, text) {
    const hintEl = element("div", "materials-hint", text);
    errors[key].before(hintEl);
    return hintEl;
  }

  function controlRow(row) {
    row.classList.add("motif-form-full");
    grid.appendChild(row);
  }

  const role = element("select");
  for (const option of MaterialsModel.motifRoles()) {
    const o = element("option", "", option.label);
    o.value = option.value;
    role.appendChild(o);
  }
  role.value = initial.role;
  field("Role", role, "role");

  const form = element("select");
  for (const option of MaterialsModel.motifForms()) {
    const o = element("option", "", option.label);
    o.value = option.value;
    form.appendChild(o);
  }
  form.value = initial.form;
  field("Form", form, "form");
  const hint = hintUnder("form", MaterialsModel.formHint(initial.form));

  const notes = element("textarea", "materials-notes");
  notes.rows = 4;
  notes.spellcheck = false;
  notes.classList.add("motif-form-wide");
  notes.value = initial.notes;
  field("Notes", notes, "notes");
  hintUnder("notes", MaterialsModel.TONIC_HINT);
  form.addEventListener("change", () => {
    hint.textContent = MaterialsModel.formHint(form.value);
  });

  /* One chip per note with its tone label (R-195); the row is hidden when the analysis has no tones. */
  const toneChips = element("div", "materials-chips motif-tone-chips");
  field("Tones", toneChips, "tones");
  let toneOverrides = { ...initial.toneOverrides };
  let openTone = null;
  function drawToneChips() {
    toneChips.innerHTML = "";
    const chips = MaterialsModel.toneChips(body);
    toneChips.parentElement.hidden = chips.length === 0;
    toneChips.parentElement.previousSibling.hidden = chips.length === 0;
    for (const chip of chips) {
      const pending = toneOverrides[String(chip.note)];
      const pendingLabel = pending ? MaterialsModel.toneRoleLabel(pending) : null;
      const el = element("span", "materials-chip motif-tone-chip");
      const button = element("button", "motif-tone-button", pendingLabel ? `${chip.note} ${pendingLabel}` : chip.text);
      button.type = "button";
      button.setAttribute("aria-label", `Note ${chip.note} tone label`);
      el.classList.toggle("motif-tone-unresolved", chip.unresolved && !pending);
      el.classList.toggle("motif-tone-overridden", chip.overridden || Boolean(pending));
      el.title = pending ? "Set by hand" : chip.unresolved ? "Does not resolve" : chip.overridden ? "Set by hand" : "";
      button.addEventListener("click", () => {
        openTone = openTone === chip.note ? null : chip.note;
        drawToneChips();
      });
      el.appendChild(button);
      if (openTone === chip.note) {
        const select = element("select");
        select.setAttribute("aria-label", `Note ${chip.note} label`);
        for (const option of MaterialsModel.TONE_ROLE_OPTIONS) {
          const o = element("option", "", option.label);
          o.value = option.value;
          select.appendChild(o);
        }
        select.value = pending || "";
        select.addEventListener("change", () => {
          toneOverrides = MaterialsModel.withToneOverride(toneOverrides, chip.note, select.value);
          openTone = null;
          drawToneChips();
        });
        el.appendChild(select);
      }
      toneChips.appendChild(el);
    }
  }
  drawToneChips();

  const importRow = element("div", "upload-controls");
  const importButton = element("button", "button-secondary", "Upload MIDI");
  importButton.type = "button";
  const importFile = element("input");
  importFile.type = "file";
  importFile.accept = ".mid,.midi";
  importFile.hidden = true;
  importRow.append(importButton, importFile);
  controlRow(importRow);
  importButton.addEventListener("click", () => importFile.click());
  importFile.addEventListener("change", async () => {
    const file = importFile.files && importFile.files[0];
    importFile.value = "";
    if (!file) {
      return;
    }
    status.show("Reading the file...");
    const answer = await sendMaterialsRequest(MaterialsModel.buildImportMidiRequest(file));
    if (!answer.ok) {
      status.show(MaterialsModel.errorText(answer.data, "Could not read the MIDI file"), true);
      return;
    }
    fill(MaterialsModel.importToFormValues(answer.data));
    status.show(MaterialsModel.importNotice(answer.data));
  });

  /* Sets the form, notes, meter, material and pickup (and the role when given) from values. */
  function fill(filled) {
    if (filled.role) {
      role.value = filled.role;
    }
    form.value = filled.form;
    hint.textContent = MaterialsModel.formHint(filled.form);
    notes.value = filled.notes;
    meter.value = filled.meter;
    material.value = filled.material;
    pickup.value = filled.pickup;
  }

  const meter = element("input");
  meter.type = "text";
  meter.value = initial.meter;
  meter.size = 6;
  field("Meter", meter, "meter");

  const material = element("select");
  for (const option of MaterialsModel.MOTIF_MATERIALS) {
    const o = element("option", "", String(option.label));
    o.value = option.value;
    material.appendChild(o);
  }
  material.value = initial.material;
  field("Mode", material, "material");

  const pickup = element("input");
  pickup.type = "text";
  pickup.value = initial.pickup;
  pickup.size = 6;
  field("Pickup (beats)", pickup, "pickup");

  const harmony = element("input");
  harmony.type = "text";
  harmony.value = initial.harmony;
  harmony.size = 30;
  harmony.maxLength = 200;
  harmony.classList.add("motif-form-wide");
  harmony.placeholder = MaterialsModel.harmonyPlaceholder(body);
  field("Base harmony", harmony, "harmony");
  hintUnder("harmony", MaterialsModel.HARMONY_HINT);

  const segmentsTitle = element("h2", "section-title", "Segments");
  const proposedTag = element("span", "materials-tag", "Proposed");
  segmentsTitle.append(" ", proposedTag);
  el.appendChild(segmentsTitle);
  el.appendChild(element("div", "materials-hint", MaterialsModel.SEGMENTS_HINT));
  const featureTable = element("table", "library-table materials-features");
  el.appendChild(featureTable);
  errors.features = element("div", "materials-field-error");
  el.appendChild(errors.features);
  let featureRows = initial.features.map((feature) => ({ ...feature }));
  /* True while the segments are the Composer's proposal; any edit of the rows makes them the motif's own. */
  let segmentsProposed = initial.segmentsProposed;

  function setProposed(value) {
    segmentsProposed = value;
    proposedTag.hidden = !segmentsProposed;
  }
  setProposed(segmentsProposed);

  function drawFeatures() {
    featureTable.innerHTML = "";
    if (featureRows.length > 0) {
      const head = element("tr");
      for (const label of ["Segment", "First note", "Last note", ""]) {
        head.appendChild(element("th", "", label));
      }
      featureTable.appendChild(head);
    }
    featureRows.forEach((row, index) => {
      const tr = element("tr");
      for (const prop of ["label", "first", "last"]) {
        const input = element("input");
        input.type = "text";
        input.size = prop === "label" ? 8 : 4;
        input.value = String(row[prop]);
        input.setAttribute("aria-label", `Segment ${index + 1} ${prop}`);
        input.addEventListener("input", () => {
          row[prop] = prop === "label" ? input.value : input.value.trim() === "" ? NaN : Number(input.value);
          setProposed(false);
        });
        const cell = element("td");
        cell.appendChild(input);
        tr.appendChild(cell);
      }
      const remove = element("button", "", "Remove");
      remove.type = "button";
      remove.addEventListener("click", () => {
        featureRows.splice(index, 1);
        setProposed(false);
        drawFeatures();
      });
      const last = element("td");
      last.appendChild(remove);
      tr.appendChild(last);
      featureTable.appendChild(tr);
    });
  }
  drawFeatures();
  const addFeature = element("button", "button-secondary", "Add segment");
  addFeature.type = "button";
  addFeature.addEventListener("click", () => {
    featureRows.push(MaterialsModel.newFeatureRow(featureRows));
    setProposed(false);
    drawFeatures();
  });
  el.appendChild(addFeature);
  /* Clears the rows and marks them proposed: the save then takes the Composer's proposal. */
  const useProposal = element("button", "button-secondary", "Use proposal");
  useProposal.type = "button";
  useProposal.addEventListener("click", () => {
    featureRows = [];
    setProposed(true);
    drawFeatures();
  });
  el.appendChild(useProposal);

  function values() {
    return {
      role: role.value,
      form: form.value,
      notes: notes.value,
      meter: meter.value,
      material: material.value,
      pickup: pickup.value,
      harmony: harmony.value,
      toneOverrides: { ...toneOverrides },
      features: featureRows.map((row) => ({ ...row })),
      segmentsProposed,
    };
  }

  function showError(text, fieldName) {
    for (const holder of Object.values(errors)) {
      holder.textContent = "";
    }
    if (text) {
      errors[fieldName || "notes"].textContent = text;
    }
  }

  /* The Composer reads the values while they are edited (R-276): its refusal shows under the field its path names. */
  const validator = Vocabulary.createValidator({
    build: () => MaterialsModel.validateRequest(values(), body),
    field: MaterialsModel.fieldForPath,
    show(result) {
      if (!result.ok && result.complete) {
        showError(result.message, result.field);
      } else {
        showError("");
      }
    },
  });
  for (const type of ["input", "change", "click"]) {
    el.addEventListener(type, () => validator.check());
  }
  validator.check();

  return { el, values, fill, showError };
}

/* The Save controls of a motif editor: an optional version note and a
 * button. onSave(values, note) resolves to the answer; its error text shows
 * beside the note list and the input stays. */
export function buildMotifSaveControls(editor, statusEl, label, withNote, onSave) {
  const controls = element("div", "upload-controls");
  const note = element("input");
  note.type = "text";
  note.placeholder = "Version note (optional)";
  const save = element("button", "", label);
  save.type = "button";
  save.addEventListener("click", async () => {
    const values = editor.values();
    save.disabled = true;
    statusEl.textContent = "";
    statusEl.classList.remove("error");
    const answer = await onSave(values, note.value);
    save.disabled = false;
    if (answer && !answer.ok) {
      editor.showError(MaterialsModel.errorText(answer.data, "Could not save the motif"), MaterialsModel.fieldForPath(answer.data.path));
    }
  });
  if (withNote) {
    controls.appendChild(note);
  }
  controls.appendChild(save);
  return controls;
}

/* The score and playback of a motif version in the score workspace, sounded in its stored key. Nothing here is saved. */
function buildMotifScore(id, item, body) {
  const motif = (body && body.motif) || {};
  const section = element("section", "panel materials-score");
  if (typeof motif.degrees !== "string" || motif.degrees === "") {
    return null;
  }
  const mountEl = element("div", "materials-score-mount");
  let workspace = null;
  function mountScore() {
    if (workspace) {
      workspace.destroy();
    }
    mountEl.innerHTML = "";
    const urls = MaterialsModel.versionScoreUrls(id, item.current_version);
    workspace = ScoreWorkspace.mount(mountEl, {
      title: item.name,
      subtitle: `${motif.form || "full"} motif · v${item.current_version} · ${MaterialsModel.motifKeyText(motif) || "C major"}`,
      musicxmlUrl: urls.musicxmlUrl,
      playbackUrl: urls.playbackUrl,
    });
  }
  section.appendChild(element("h2", "section-title", "Score"));
  section.appendChild(mountEl);
  mountScore();
  return section;
}

/* The Composer's analysis of a motif version body, or why there is none. */
function buildAnalysisPanel(body, analysisError, onReanalyzed) {
  const info = MaterialsModel.analysisRows(body && body.motif, analysisError);
  const section = element("section", "panel materials-analysis");
  section.appendChild(element("h2", "section-title", "Analysis"));
  const statusRow = element("div", "upload-controls");
  statusRow.appendChild(element("div", "piece-list-status", info.message));
  const again = element("button", "button-secondary", "Re-analyze");
  again.type = "button";
  again.addEventListener("click", async () => {
    again.disabled = true;
    onReanalyzed.status.show("Re-analyzing...");
    const answer = await onReanalyzed.send();
    if (answer.ok) {
      onReanalyzed.done(answer.data.item.analysis_error);
      return;
    }
    again.disabled = false;
    onReanalyzed.status.show(MaterialsModel.errorText(answer.data, "Could not re-analyze"), true);
  });
  statusRow.appendChild(again);
  section.appendChild(statusRow);
  if (!info.analyzed) {
    return section;
  }
  const list = element("dl", "materials-analysis-list");
  for (const row of info.rows) {
    list.appendChild(element("dt", "", row.label));
    list.appendChild(element("dd", "", row.text));
  }
  section.appendChild(list);
  return section;
}

export default { render };
