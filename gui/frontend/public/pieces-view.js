/* The Pieces view's editor (P17, R-169, R-174): the header, the structure strip and the section editor of a piece,
 * mounted in the piece detail of materials-view.js. Pure parts are in pieces-model.js; the field builders
 * (themeField, themeSelect, themeInput, themeButton) come from themes-view.js, materialsElement,
 * sendMaterialsRequest and renderMaterialsDetail from materials-view.js. */

const PIECE_PICKUP_LIST = "piece-pickup-values";

/* One lane of the strip: its label, a spacer to the first section and a block for each section, wide by its beats. */
function pieceLane(part, entries, total, state, selected, onSelect, onAdd) {
  const lane = materialsElement("div", "piece-lane");
  lane.appendChild(materialsElement("div", "piece-lane-label", part.label));
  const track = materialsElement("div", "piece-lane-track");
  if (entries.length > 0) {
    const spacer = materialsElement("div", "piece-lane-spacer");
    spacer.style.flex = `0 0 ${(entries[0].start / total) * 100}%`;
    track.appendChild(spacer);
  }
  for (const entry of entries) {
    const label = `${entry.letter || entry.id} · ${PiecesModel.beatsLabel(entry.beats, state.meter)}`;
    const block = themeButton(label, () => onSelect(entry.id));
    block.classList.add("piece-block");
    block.classList.toggle("selected", entry.id === selected);
    block.title = label;
    block.style.flex = `0 0 ${(entry.beats / total) * 100}%`;
    track.appendChild(block);
  }
  const add = themeButton("Add section", () => onAdd(part.value));
  add.classList.add("piece-lane-add");
  lane.append(track, add);
  return lane;
}

/*
 * The piece editor. ctx: {id, body, usesStatus, items (all items), statusEl, saved(answer)}.
 * Returns {el, state, problems(), save(note)}.
 */
function buildPieceEditor(ctx) {
  const state = PiecesModel.stateFromBody(ctx.body, ctx.id);
  if (state.sections.length === 0) {
    state.sections = [PiecesModel.newSection(state, "body")];
  }
  const el = materialsElement("section", "panel materials-editor piece-editor");
  const problemsEl = materialsElement("div", "materials-field-error");
  let selected = state.sections[0].id;

  /* Header */
  el.appendChild(materialsElement("h2", "section-title", "Piece"));
  const header = materialsElement("div", "upload-controls");
  const key = themeField(header, "Key", themeInput("D or Bb", state.key));
  const mode = themeField(header, "Mode", themeSelect(PiecesModel.MODES, state.mode));
  const meter = themeField(header, "Meter", themeSelect(PiecesModel.METERS, state.meter));
  const tempo = themeField(header, "Tempo", themeInput("from the brief", state.tempo));
  function syncHeader() {
    state.key = key.value;
    state.mode = mode.value;
    state.meter = meter.value;
    state.tempo = tempo.value;
    drawStrip();
    drawSectionEditor();
    problems();
  }
  for (const control of [key, mode, meter, tempo]) {
    control.addEventListener("input", syncHeader);
  }
  el.appendChild(header);

  /* The strip */
  el.appendChild(materialsElement("h2", "section-title", "Structure"));
  const stripEl = materialsElement("div", "piece-strip");
  const editorEl = materialsElement("div", "piece-section-editor");
  el.append(stripEl, editorEl);

  function drawStrip() {
    stripEl.innerHTML = "";
    const timeline = PiecesModel.sectionTimeline(state);
    const total = Math.max(1, timeline.reduce((sum, entry) => sum + entry.beats, 0));
    for (const part of PiecesModel.PARTS) {
      stripEl.appendChild(
        pieceLane(
          part,
          timeline.filter((entry) => entry.part === part.value),
          total,
          state,
          selected,
          (id) => {
            selected = id;
            drawStrip();
            drawSectionEditor();
          },
          (partName) => {
            const next = PiecesModel.addSection(state, partName);
            state.sections = next.sections;
            selected = state.sections.find((section) => !timeline.some((entry) => entry.id === section.id)).id;
            redraw();
          },
        ),
      );
    }
  }

  function redraw() {
    drawStrip();
    drawSectionEditor();
    problems();
  }

  function drawSectionEditor() {
    editorEl.innerHTML = "";
    const index = state.sections.findIndex((section) => section.id === selected);
    if (index < 0) {
      editorEl.appendChild(materialsElement("div", "piece-list-status", "Select a section to edit it."));
      return;
    }
    const section = state.sections[index];
    const part = PiecesModel.PARTS.find((entry) => entry.value === section.part);
    editorEl.appendChild(materialsElement("h3", "section-title", `Section ${section.letter || section.id} (${part.label})`));
    const controls = materialsElement("div", "upload-controls");
    const letter = themeField(controls, "Letter", themeInput("A", section.letter));
    const per = PiecesModel.barBeats(state.meter);
    const bars = themeField(controls, "Bars", themeInput("0", String(Math.floor(section.beats / per))));
    const extra = themeField(controls, "Beats", themeInput("0", String(section.beats % per)));
    const lengthEl = materialsElement("span", "piece-list-status", PiecesModel.beatsLabel(section.beats, state.meter));
    const pickup = themeField(controls, "Pickup (beats)", themeInput("none", section.pickup));
    pickup.setAttribute("list", PIECE_PICKUP_LIST);
    const list = materialsElement("datalist");
    list.id = PIECE_PICKUP_LIST;
    for (const value of PiecesModel.PICKUPS) {
      const option = materialsElement("option");
      option.value = value;
      list.appendChild(option);
    }
    const cadence = themeField(
      controls,
      "Cadence",
      themeSelect(
        [{ value: "", label: `Default (${PiecesModel.defaultCadence(state, index)})` }, ...PiecesModel.CADENCES],
        section.cadence || "",
      ),
    );
    function syncSection() {
      section.letter = letter.value;
      const barCount = Number(bars.value);
      const extraCount = Number(extra.value);
      section.beats = (Number.isInteger(barCount) && barCount >= 0 ? barCount : 0) * per + (Number.isInteger(extraCount) && extraCount >= 0 ? extraCount : 0);
      lengthEl.textContent = PiecesModel.beatsLabel(section.beats, state.meter);
      section.pickup = pickup.value.trim() === "" ? "0" : pickup.value;
      section.cadence = cadence.value || null;
      drawStrip();
      problems();
    }
    for (const control of [letter, bars, extra, pickup, cadence]) {
      control.addEventListener("input", syncSection);
    }
    controls.append(lengthEl, list);
    const actions = materialsElement("div", "upload-controls");
    const left = themeButton("Move left", () => {
      state.sections = PiecesModel.moveSection(state, section.id, -1).sections;
      redraw();
    });
    const right = themeButton("Move right", () => {
      state.sections = PiecesModel.moveSection(state, section.id, 1).sections;
      redraw();
    });
    const remove = themeButton("Remove section", () => {
      state.sections = PiecesModel.removeSection(state, section.id).sections;
      selected = state.sections.length > 0 ? state.sections[Math.min(index, state.sections.length - 1)].id : "";
      redraw();
    });
    left.disabled = index === 0 || state.sections[index - 1].part !== section.part;
    right.disabled = index === state.sections.length - 1 || state.sections[index + 1].part !== section.part;
    actions.append(left, right, remove);
    editorEl.append(controls, actions);
  }

  el.appendChild(problemsEl);

  function problems() {
    const list = PiecesModel.formProblems(state);
    problemsEl.textContent = list.join(" ");
    return list;
  }

  /* Saves the form as a new version; the answer or null when the form has problems. */
  async function save(note) {
    if (problems().length > 0) {
      return null;
    }
    const answer = await sendMaterialsRequest(PiecesModel.saveRequest(state, note));
    if (!answer.ok) {
      ctx.statusEl.classList.add("error");
      ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "Could not save the piece");
    }
    return answer;
  }

  const saveRow = materialsElement("div", "upload-controls");
  const noteInput = themeInput("Version note (optional)", "");
  const saveButton = themeButton("Save as new version", async () => {
    const answer = await save(noteInput.value);
    if (answer && answer.ok) {
      ctx.saved(answer);
    }
  });
  saveRow.append(noteInput, saveButton);
  el.appendChild(saveRow);

  redraw();
  return { el, state, problems, save };
}
