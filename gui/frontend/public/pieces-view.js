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
 * A picker of one composed theme version: a name search, the theme, its composed versions and a line with its form.
 * options: {items(), onChange({item_id, version, beats} | null) for a pick the user made, onInfo({item_id, version,
 * beats}) for every theme version now shown}. Returns {el, setValue({item_id, version})}.
 */
function buildPieceThemePicker(options) {
  const el = materialsElement("div", "theme-motif-picker");
  const controls = materialsElement("div", "upload-controls");
  const query = themeInput("Search themes by name");
  const choice = materialsElement("select", "materials-use-choice");
  const versionSelect = materialsElement("select");
  const infoEl = materialsElement("div", "piece-list-status");
  let bodies = {};
  let loading = 0;

  function fillChoices(keep) {
    const found = MaterialsModel.pickerItems(options.items(), materialsState.projects, { query: query.value, type: "theme" });
    choice.innerHTML = "";
    const blank = materialsElement("option", "", found.length === 0 ? "No themes" : "Choose a theme");
    blank.value = "";
    choice.appendChild(blank);
    for (const item of found) {
      const option = materialsElement("option", "", item.label);
      option.value = item.id;
      choice.appendChild(option);
    }
    choice.value = keep && found.some((item) => item.id === keep) ? keep : "";
  }

  function show(text, isError) {
    infoEl.textContent = text;
    infoEl.classList.toggle("error", Boolean(isError));
  }

  /* Shows the picked version: its form and length. announce: the pick came from the user. */
  function showVersion(itemId, number, announce) {
    const body = bodies[number];
    const beats = PiecesModel.themeLengthBeats(body);
    const value = { item_id: itemId, version: Number(number), beats };
    show(PiecesModel.themeFormText(body));
    options.onInfo(value);
    if (announce) {
      options.onChange(value);
    }
  }

  /* Loads the composed versions of a theme and picks `wanted` (else the newest). */
  async function loadVersions(itemId, wanted, announce) {
    const ticket = ++loading;
    versionSelect.innerHTML = "";
    bodies = {};
    const answer = await sendMaterialsRequest(MaterialsModel.buildItemGetRequest(itemId));
    if (ticket !== loading) {
      return;
    }
    if (!answer.ok) {
      show(MaterialsModel.errorText(answer.data, "Could not load the theme"), true);
      return;
    }
    for (const row of MaterialsModel.versionRows(answer.data.item)) {
      const got = await sendMaterialsRequest(MaterialsModel.buildVersionGetRequest(itemId, row.number));
      if (ticket !== loading) {
        return;
      }
      if (got.ok && PiecesModel.themeComposed(got.data.body)) {
        bodies[row.number] = got.data.body;
        const option = materialsElement("option", "", row.current ? `v${row.number} (current)` : `v${row.number}`);
        option.value = String(row.number);
        versionSelect.appendChild(option);
      }
    }
    if (versionSelect.options.length === 0) {
      show("This theme has no composed version. Compose it first.", true);
      if (announce) {
        options.onChange(null);
      }
      return;
    }
    versionSelect.value = bodies[wanted] ? String(wanted) : versionSelect.options[0].value;
    showVersion(itemId, versionSelect.value, announce);
  }

  query.addEventListener("input", () => fillChoices(choice.value));
  choice.addEventListener("change", () => {
    if (!choice.value) {
      versionSelect.innerHTML = "";
      show("");
      options.onChange(null);
      return;
    }
    loadVersions(choice.value, 0, true);
  });
  versionSelect.addEventListener("change", () => showVersion(choice.value, versionSelect.value, true));
  controls.append(query, choice, versionSelect);
  el.append(controls, infoEl);
  fillChoices("");

  /* Shows a stored value without announcing it. */
  async function setValue(value) {
    if (!value || !value.item_id) {
      return;
    }
    if (!options.items().some((item) => item.id === value.item_id)) {
      show(`Theme ${value.item_id} not found.`, true);
      return;
    }
    query.value = "";
    fillChoices(value.item_id);
    await loadVersions(value.item_id, value.version, false);
  }
  return { el, setValue };
}

/* The fill a new kind starts with. */
function pieceNewFill(kind) {
  switch (kind) {
    case "motif":
      return { kind, item_id: "", version: 0, into: "sentence", hybrid: "presentation+continuation" };
    case "theme":
      return { kind, item_id: "", version: 0 };
    case "progression":
      return { kind, numerals: "", cells: "" };
    default:
      return PiecesModel.emptyFill();
  }
}

/* A table of header cells and rows of text. */
function pieceTable(headers, rows) {
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of headers) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const row of rows) {
    const tr = materialsElement("tr");
    for (const cell of row) {
      tr.appendChild(materialsElement("td", "", cell === null || cell === undefined ? "" : String(cell)));
    }
    table.appendChild(tr);
  }
  return table;
}

/* One version of a piece result: its section rows, seam rules, score, Analyzer check and style distance. */
function pieceVersionTables(view, heading, check) {
  const box = materialsElement("div", "piece-version");
  box.appendChild(materialsElement("h3", "section-title", heading));
  box.appendChild(
    pieceTable(
      ["Section", "Part", "Letter", "Start", "Length", "Cadence", "Heard", "Fill", "Source"],
      view.rows.map((row) => [row.id, row.part, row.letter, row.start, row.length, row.cadence, row.heard, row.fill, row.source]),
    ),
  );
  box.appendChild(
    themeFacts([
      ["Score", view.score ? view.score.label : ""],
      ["Seam", view.seam ? view.seam.label : ""],
      ...(view.seam ? view.seam.rules.map((rule) => [rule.label, rule.held ? "held" : "not held"]) : []),
      ["Analyzer check", view.check ? view.check.label : check ? "" : "not run"],
      ...view.distance.map((entry) => [`Style distance, ${entry.label}`, entry.value]),
    ]),
  );
  return box;
}

/*
 * Regenerate and variations of the sections of a composed version. ctx: {id, version, statusEl, saved(answer)}; state
 * is the form state of the version's body. A regenerate or a chosen variation becomes the next version (ctx.saved).
 */
function buildPieceActions(ctx, state) {
  const box = materialsElement("div", "piece-actions");
  box.appendChild(materialsElement("h3", "section-title", "Regenerate and variations"));
  const results = materialsElement("div", "composer-variation-grid");
  const buttons = [];

  function setBusy(busy, text) {
    for (const button of buttons) {
      button.disabled = busy;
    }
    ctx.statusEl.style.whiteSpace = "pre-wrap";
    ctx.statusEl.classList.remove("error");
    ctx.statusEl.textContent = busy ? text : "";
  }

  function fail(answer, fallback) {
    ctx.statusEl.classList.add("error");
    ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, fallback);
  }

  async function run(built, busyText, fallback, onOk) {
    if (built.error) {
      ctx.statusEl.classList.add("error");
      ctx.statusEl.textContent = built.error;
      return;
    }
    setBusy(true, busyText);
    const answer = await sendMaterialsRequest(built.request);
    setBusy(false);
    if (!answer.ok) {
      fail(answer, fallback);
      return;
    }
    onOk(answer);
  }

  function showVariations(section, data) {
    results.innerHTML = "";
    const cards = PiecesModel.variationCards(data);
    if (cards.length === 0) {
      results.appendChild(materialsElement("div", "piece-list-status", "No different version was found."));
      return;
    }
    for (const card of cards) {
      const el = materialsElement("div", "composer-variation");
      el.appendChild(materialsElement("div", "", `Section ${section.letter || section.id} ${card.title}`));
      el.appendChild(materialsElement("div", "claim-row-where", [card.score && `score ${card.score}`, card.seam, card.heard && `heard ${card.heard}`].filter(Boolean).join(", ")));
      const scoreStatus = materialsElement("div", "piece-list-status");
      const score = materialsElement("div", "analysis-score-container");
      const choose = themeButton("Choose", () =>
        run({ request: PiecesModel.chooseRequest(ctx.id, ctx.version, data.set, card.index) }, "Choosing...", "Could not choose the variation", ctx.saved),
      );
      buttons.push(choose);
      el.append(scoreStatus, score, choose);
      results.appendChild(el);
      drawMusicXmlScore(PiecesModel.variationFileUrl(ctx.id, ctx.version, data.set, `variation-${card.index}.musicxml`), score, scoreStatus, 0.5);
    }
  }

  for (const section of state.sections) {
    const lanes = PiecesModel.lanesFor(section);
    if (lanes.length === 0) {
      continue;
    }
    const whole = lanes.length === PiecesModel.LANES.length;
    const row = materialsElement("div", "upload-controls piece-action-row");
    row.appendChild(materialsElement("strong", "", `${section.letter || section.id} (${PiecesModel.beatsLabel(section.beats, state.meter)})`));
    const regenLane = themeSelect([...(whole ? [{ value: "", label: "All lanes" }] : []), ...lanes], whole ? "" : lanes[0]);
    const regenSeed = themeInput("seed", "");
    const regenerate = themeButton("Regenerate", () =>
      run(
        PiecesModel.regenerateRequest(ctx.id, ctx.version, { section_id: section.id, lane: regenLane.value, seed: regenSeed.value }),
        "Regenerating, this can take a while...",
        "The section could not be regenerated",
        ctx.saved,
      ),
    );
    const varLane = themeSelect(whole ? [...lanes, "all"] : lanes, lanes[0]);
    const varCount = themeInput("count", "3");
    const varSeed = themeInput("seed", "");
    const variations = themeButton("Variations", () =>
      run(
        PiecesModel.variationsRequest(ctx.id, ctx.version, { section_id: section.id, lane: varLane.value, count: varCount.value, seed: varSeed.value }),
        "Making variations, this can take a while...",
        "The variations could not be made",
        (answer) => showVariations(section, answer.data),
      ),
    );
    buttons.push(regenerate, variations);
    row.append(regenLane, regenSeed, regenerate, varLane, varCount, varSeed, variations);
    box.appendChild(row);
  }
  box.appendChild(results);
  return box;
}

/*
 * The result of a piece version: the Composer's summary as tables for the loop and the ending version, the scores
 * with playback and the MIDI downloads. ctx: {id, version, piece (the version body's piece), nameOf, title}, and
 * statusEl and saved(answer) on the current version, which adds regenerate and variations. Returns null when the
 * version has no result.
 */
function buildPieceResult(ctx) {
  const result = ctx.piece && ctx.piece.result;
  if (!result || typeof result !== "object") {
    return null;
  }
  const state = PiecesModel.stateFromBody({ uses: [], piece: ctx.piece }, ctx.id);
  const sourceNames = {};
  for (const section of state.sections) {
    if (section.fill.kind === "theme") {
      sourceNames[section.id] = `${ctx.nameOf(section.fill.item_id)} v${section.fill.version}`;
    }
  }
  const view = PiecesModel.resultView(result, { sourceNames });
  const section = materialsElement("section", "panel materials-analysis piece-result");
  section.appendChild(materialsElement("h2", "section-title", "Result"));
  section.appendChild(
    themeFacts([
      ["Seed", view.seed === null ? "" : String(view.seed)],
      ["Key", view.key],
      ["Loop", view.loop ? `${view.loop} (${view.loopLength})` : ""],
      ["Tempo", view.brief.tempo],
      ["Mode", view.brief.mode],
      ["Note density", view.brief.density],
      ["Read without", view.brief.without],
      ["Brief", view.pieces],
    ]),
  );
  if (view.versions.loop) {
    section.appendChild(pieceVersionTables(view.versions.loop, "Loop version", ctx.piece.check === true));
  }
  if (view.versions.ending) {
    section.appendChild(pieceVersionTables(view.versions.ending, "Ending version", ctx.piece.check === true));
  }
  const downloads = materialsElement("div", "upload-controls");
  for (const [label, name] of [
    ["Download loop MIDI", "loop.mid"],
    ["Download ending MIDI", "ending.mid"],
  ]) {
    const link = materialsElement("a", "back-link clickable", label);
    link.href = PiecesModel.versionFileUrl(ctx.id, ctx.version, name);
    link.download = name;
    downloads.appendChild(link);
  }
  section.appendChild(downloads);

  /* The scores: the loop version, a tab for the ending version */
  const tabs = materialsElement("div", "upload-controls");
  const scoreHost = materialsElement("div", "materials-score-mount");
  function showScore(which) {
    const looping = which === "loop" && view.playLoop;
    themeMountScore(scoreHost, ctx.id, ctx.version, ctx.title, {
      midi: `${which}.mid`,
      musicxml: `${which}.musicxml`,
      subtitle: `piece · v${ctx.version} · ${which} version`,
      loop: looping ? { start: view.playLoop.start, end: view.playLoop.end } : null,
      ending: looping ? { midi: "ending.mid", musicxml: "ending.musicxml", bodyEnd: view.playLoop.bodyEnd } : null,
    });
    for (const button of tabs.children) {
      button.classList.toggle("selected", button.dataset.which === which);
    }
  }
  for (const [which, label] of [
    ["loop", "Loop score"],
    ["ending", "Ending score"],
  ]) {
    const button = themeButton(label, () => showScore(which));
    button.dataset.which = which;
    tabs.appendChild(button);
  }
  section.append(tabs, scoreHost);
  showScore("loop");
  if (ctx.statusEl && ctx.saved) {
    section.appendChild(buildPieceActions(ctx, state));
  }
  return section;
}

/* Shows an older version read only: its structure, then its result. */
function pieceVersionView(viewEl, id, number, body, nameOf, title) {
  viewEl.innerHTML = "";
  viewEl.appendChild(materialsElement("h2", "section-title", `Version ${number} (read only)`));
  const piece = body && body.piece ? body.piece : {};
  const state = PiecesModel.stateFromBody(body, id);
  viewEl.appendChild(
    themeFacts([
      ["Key", `${state.key} ${state.mode}, ${state.meter}`],
      ...state.sections.map((section, index) => [
        `${PiecesModel.PARTS.find((entry) => entry.value === section.part).label} ${section.letter || section.id}`,
        `${PiecesModel.beatsLabel(section.beats, state.meter)}, cadence ${PiecesModel.cadenceOf(state, index)}, ${section.fill.kind}${
          section.fill.item_id ? ` ${nameOf(section.fill.item_id)} v${section.fill.version}` : ""
        }`,
      ]),
      ["Brief", ThemesModel.briefLabel(ThemesModel.briefFromPicks(state.picks))],
    ]),
  );
  const result = buildPieceResult({ id, version: number, piece, nameOf, title });
  viewEl.appendChild(result || materialsElement("div", "piece-list-status", "This version was not composed."));
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
  const nameOf = (itemId) => {
    const found = ctx.items.find((item) => item.id === itemId);
    return found ? found.name : itemId;
  };
  const problemsEl = materialsElement("div", "materials-field-error");
  let selected = state.sections[0].id;
  let poolRefresh = () => {};
  let savedJson = JSON.stringify(PiecesModel.bodyFromState(state));
  const themeLengths = {};
  const structureOptions = {
    themeBeats: (fill) => (Number.isFinite(themeLengths[PiecesModel.fillKey(fill)]) ? themeLengths[PiecesModel.fillKey(fill)] : null),
  };

  /* Sources (motifs and themes) with a newer version */
  const notices = materialsElement("div");
  for (const use of PiecesModel.updatedUses({ uses_status: ctx.usesStatus })) {
    const notice = materialsElement("div", "piece-list-status");
    notice.appendChild(materialsElement("span", "", `${use.label} `));
    notice.appendChild(themeButton("Update and compose", () => compose({ update_uses: true })));
    notice.appendChild(themeButton("Keep", () => notice.remove()));
    notices.appendChild(notice);
  }
  el.appendChild(notices);

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
    let drawInto = () => {};
    function syncSection() {
      section.letter = letter.value;
      const barCount = Number(bars.value);
      const extraCount = Number(extra.value);
      section.beats = (Number.isInteger(barCount) && barCount >= 0 ? barCount : 0) * per + (Number.isInteger(extraCount) && extraCount >= 0 ? extraCount : 0);
      lengthEl.textContent = PiecesModel.beatsLabel(section.beats, state.meter);
      section.pickup = pickup.value.trim() === "" ? "0" : pickup.value;
      section.cadence = cadence.value || null;
      drawStrip();
      drawInto();
      problems();
    }
    /* Sets the length from outside the length inputs (a theme fill takes its theme's). */
    function applyBeats(beats) {
      bars.value = String(Math.floor(beats / per));
      extra.value = String(beats % per);
      syncSection();
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

    /* The fill */
    const fillBox = materialsElement("div", "piece-fill");
    fillBox.appendChild(materialsElement("h3", "section-title", "Fill"));
    const kind = themeField(fillBox, "Fill with", themeSelect(PiecesModel.FILL_KINDS, section.fill.kind));
    const fillBody = materialsElement("div");
    fillBox.appendChild(fillBody);

    function drawFill() {
      fillBody.innerHTML = "";
      drawInto = () => {};
      const fill = section.fill;
      if (fill.kind === "motif") {
        const picker = buildThemeMotifPicker({
          role: "melodic",
          items: () => ctx.items,
          onChange: (value) => {
            fill.item_id = value ? value.item_id : "";
            fill.version = value ? value.version : 0;
            problems();
            poolRefresh();
          },
        });
        picker.setValue(fill);
        const intoEl = materialsElement("div", "upload-controls");
        drawInto = () => {
          intoEl.innerHTML = "";
          if (PiecesModel.motifIsFree(section, state.meter)) {
            intoEl.appendChild(
              materialsElement("span", "piece-list-status", "Free phrase: a theme form needs 4 or 8 bars and no pickup."),
            );
            return;
          }
          const into = themeField(intoEl, "Into", themeSelect(PiecesModel.INTOS, fill.into || "sentence"));
          const [beginning, ending] = (fill.hybrid || "presentation+continuation").split("+");
          const beginningSelect = themeField(intoEl, "Beginning", themeSelect(ThemesModel.BEGINNINGS, beginning));
          const endingSelect = themeField(intoEl, "Ending", themeSelect(ThemesModel.ENDINGS, ending));
          function syncInto() {
            fill.into = into.value;
            fill.hybrid = `${beginningSelect.value}+${endingSelect.value}`;
            beginningSelect.parentElement.hidden = fill.into !== "hybrid";
            endingSelect.parentElement.hidden = fill.into !== "hybrid";
          }
          for (const control of [into, beginningSelect, endingSelect]) {
            control.addEventListener("input", syncInto);
          }
          syncInto();
        };
        drawInto();
        fillBody.append(picker.el, intoEl);
      } else if (fill.kind === "theme") {
        const picker = buildPieceThemePicker({
          items: () => ctx.items,
          onInfo: (value) => {
            themeLengths[PiecesModel.fillKey(value)] = value.beats;
            problems();
          },
          onChange: (value) => {
            fill.item_id = value ? value.item_id : "";
            fill.version = value ? value.version : 0;
            if (value && Number.isFinite(value.beats)) {
              applyBeats(value.beats);
            }
            problems();
          },
        });
        picker.setValue(fill);
        fillBody.appendChild(picker.el);
      } else if (fill.kind === "progression") {
        const row = materialsElement("div", "upload-controls");
        const numerals = themeField(row, "Numerals", themeInput("I vi IV V", fill.numerals));
        const cells = themeField(row, "Cells per chord", themeInput("default", fill.cells));
        for (const control of [numerals, cells]) {
          control.addEventListener("input", () => {
            fill.numerals = numerals.value;
            fill.cells = cells.value;
            problems();
          });
        }
        fillBody.append(
          row,
          materialsElement("div", "piece-list-status", "A progression does not change the cadence: end it on the chord the cadence needs."),
        );
      } else {
        fillBody.appendChild(materialsElement("div", "piece-list-status", "Empty: the Composer invents this section."));
      }
    }
    kind.addEventListener("input", () => {
      section.fill = pieceNewFill(kind.value);
      drawFill();
      problems();
    });
    drawFill();
    editorEl.appendChild(fillBox);
  }

  /* The basic motive */
  el.appendChild(materialsElement("h2", "section-title", "Basic motive (optional)"));
  const motivePicker = buildThemeMotifPicker({
    role: "melodic",
    items: () => ctx.items,
    onChange: (value) => {
      state.motive = value ? { item_id: value.item_id, version: value.version } : null;
    },
  });
  motivePicker.setValue(state.motive);
  el.appendChild(motivePicker.el);

  /* The pool: the saved forms each motif fill has */
  const pool = buildThemePoolSection(
    state,
    nameOf,
    (value) => {
      state.pool = value;
    },
    () =>
      state.sections
        .filter((section) => section.fill.kind === "motif" && section.fill.item_id)
        .map((section) => ({ label: `Section ${section.letter || section.id}`, item_id: section.fill.item_id })),
  );
  el.appendChild(pool.el);
  poolRefresh = pool.refresh;
  pool.refresh();

  el.appendChild(buildThemeAccompanimentSection(state, () => ctx.items, { motif: false }));

  el.appendChild(
    buildThemeBriefSection(state, {
      tempo: false,
      emptyText: "No brief: the piece is composed without a style.",
    }),
  );

  el.appendChild(problemsEl);

  function problems() {
    const list = PiecesModel.formProblems(state, structureOptions);
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
  const composeButton = themeButton("Compose", () => compose({}));
  const spinner = materialsElement("span", "theme-spinner");
  spinner.hidden = true;
  saveRow.append(noteInput, saveButton, composeButton, spinner);
  el.appendChild(saveRow);

  function setBusy(busy) {
    saveButton.disabled = busy;
    composeButton.disabled = busy;
    spinner.hidden = !busy;
    if (busy) {
      ctx.statusEl.textContent = "Composing...";
      ctx.statusEl.classList.remove("error");
    }
  }

  /* Saves the form first when it changed, then composes it into a new version. options: {update_uses}. */
  async function compose(options) {
    if (problems().length > 0) {
      return;
    }
    setBusy(true);
    const structure = ctx.body && ctx.body.piece ? ctx.body.piece.structure : null;
    const changed =
      JSON.stringify(PiecesModel.bodyFromState(state)) !== savedJson ||
      !structure ||
      typeof structure !== "object" ||
      !Array.isArray(structure.sections);
    if (changed) {
      const saved = await save("Edited before composing");
      if (!saved || !saved.ok) {
        setBusy(false);
        return;
      }
      savedJson = JSON.stringify(PiecesModel.bodyFromState(state));
    }
    const answer = await sendMaterialsRequest(PiecesModel.composeRequest(state, options));
    setBusy(false);
    if (!answer.ok) {
      ctx.statusEl.style.whiteSpace = "pre-wrap";
      ctx.statusEl.classList.add("error");
      ctx.statusEl.textContent = MaterialsModel.errorText(answer.data, "The piece could not be composed");
      return;
    }
    ctx.saved(answer);
  }

  redraw();
  return { el, state, problems, save, compose };
}
