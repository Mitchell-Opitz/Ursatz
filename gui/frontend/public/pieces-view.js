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

/* The read-only strip of a sized form at its size: a block per section wide by its bars, then a table of what the Composer
 * plans for each section (role, length, beginning and ending, cadence, keys, what it returns to, the slots and their ideas). */
function pieceSizedStrip(form, bars, count) {
  const box = materialsElement("div");
  const sections = PiecesModel.sectionsOf(form, bars, count);
  if (sections.length === 0) {
    box.appendChild(materialsElement("div", "piece-list-status", "The Composer has no sections for this size."));
    return box;
  }
  const lengthOf = (section) => (Number.isFinite(section.bars) ? `${section.bars} bars` : Number.isFinite(section.beats) ? `${section.beats} beats` : "");
  const total = Math.max(1, sections.reduce((sum, section) => sum + (Number.isFinite(section.bars) ? section.bars : 0), 0));
  const track = materialsElement("div", "piece-lane-track");
  for (const section of sections) {
    const halves = PiecesModel.sectionConstruction(section).halves;
    const label = `${section.letter} · ${lengthOf(section)}${halves ? ` · ${halves}` : ""}`;
    const block = materialsElement("div", "piece-block", label);
    block.title = `${section.role ? section.role.label : ""} ${label}`.trim();
    block.style.flex = `0 0 ${((Number.isFinite(section.bars) ? section.bars : 0) / total) * 100}%`;
    track.appendChild(block);
  }
  const lane = materialsElement("div", "piece-lane");
  lane.appendChild(track);
  box.appendChild(lane);
  /* The Construction column is there only when a section has one. */
  const constructions = sections.map((section) => PiecesModel.sectionConstruction(section).text);
  const withConstruction = constructions.some((text) => text !== "");
  box.appendChild(
    pieceTable(
      ["Section", "Role", "Length", "Beginning", "Ending", "Cadence", "Keys", "Harmony", "Returns", "Slots", ...(withConstruction ? ["Construction"] : [])],
      sections.map((section, at) => [
        section.letter,
        section.role ? section.role.label : "",
        lengthOf(section),
        section.beginning ? section.beginning.label : "",
        section.ending ? section.ending.label : "",
        section.cadence ? section.cadence.label : "",
        section.keys ? `${section.keys.start} to ${section.keys.end}` : "",
        section.pedal ? section.pedal.label : "",
        section.returns ? (section.variation ? `${section.returns} (${section.variation})` : section.returns) : "",
        (Array.isArray(section.slots) ? section.slots : []).map((slot) => `${slot.slot}: ${slot.ideas.join(", ")}`).join("; "),
        ...(withConstruction ? [constructions[at]] : []),
      ]),
    ),
  );
  return box;
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
function pieceVersionTables(view, heading, check, sized) {
  const box = materialsElement("div", "piece-version");
  box.appendChild(materialsElement("h3", "section-title", heading));
  /* The Construction column is there only when a section has one (P47). */
  const withConstruction = view.rows.some((row) => row.construction !== "");
  const extraHead = withConstruction ? ["Construction"] : [];
  const extra = (row) => (withConstruction ? [row.construction] : []);
  box.appendChild(
    sized
      ? pieceTable(
          ["Section", "Role", "Letter", "Start", "Length", "Cadence", "Heard", "Key heard", "Harmony", "Returns", "How it differs", ...extraHead],
          view.rows.map((row) => [row.id, row.partLabel, row.letter, row.start, row.length, row.cadence, row.heard, row.keyHeard, row.pedal, row.returns, row.changes, ...extra(row)]),
        )
      : pieceTable(
          ["Section", "Part", "Letter", "Start", "Length", "Cadence", "Heard", "Fill", "Source", ...extraHead],
          view.rows.map((row) => [row.id, row.partLabel, row.letter, row.start, row.length, row.cadence, row.heard, row.fill, row.source, ...extra(row)]),
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
function buildPieceActions(ctx, state, rows) {
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

  for (const section of PiecesModel.actionSections(state, rows)) {
    const lanes = section.lanes;
    const whole = lanes.length === PiecesModel.LANES.length;
    const row = materialsElement("div", "upload-controls piece-action-row");
    row.appendChild(materialsElement("strong", "", `${section.letter} (${section.length})`));
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
 * The result of a piece version: the Composer's summary as a table for each version of its form, the scores
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
  const view = PiecesModel.resultView(result, { sourceNames, form: state.form, bars: state.bars, count: state.count });
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
  for (const version of view.versions) {
    section.appendChild(pieceVersionTables(version.view, version.label, ctx.piece.check === true, PiecesModel.isSized(PiecesModel.formOf(state.form))));
  }
  const downloads = materialsElement("div", "upload-controls");
  for (const version of view.versions) {
    for (const [label, name] of [
      [`Download ${version.name} MIDI`, `${version.name}.mid`],
      [`Download ${version.name} MusicXML`, `${version.name}.musicxml`],
    ]) {
      const link = materialsElement("a", "back-link clickable", label);
      link.href = PiecesModel.versionFileUrl(ctx.id, ctx.version, name);
      link.download = name;
      downloads.appendChild(link);
    }
  }
  section.appendChild(downloads);

  /* The scores: a tab for each version, the first shown. A version that loops repeats its loop and plays the version
   * that comes after it (its `after`) as its ending. */
  const tabs = materialsElement("div", "upload-controls");
  const scoreHost = materialsElement("div", "materials-score-mount");
  function showScore(version) {
    const looping = version.loop && view.playLoop;
    const ending = looping ? view.versions.find((other) => other.after === version.name) : null;
    themeMountScore(scoreHost, ctx.id, ctx.version, ctx.title, {
      playback: version.name,
      musicxml: `${version.name}.musicxml`,
      subtitle: `piece · v${ctx.version} · ${version.name} version`,
      loop: looping ? { start: view.playLoop.start, end: view.playLoop.end } : null,
      ending: ending ? { playback: ending.name, musicxml: `${ending.name}.musicxml`, bodyEnd: view.playLoop.bodyEnd } : null,
    });
    for (const button of tabs.children) {
      button.classList.toggle("selected", button.dataset.which === version.name);
    }
  }
  for (const version of view.versions) {
    const button = themeButton(`${version.name.charAt(0).toUpperCase()}${version.name.slice(1)} score`, () => showScore(version));
    button.dataset.which = version.name;
    tabs.appendChild(button);
  }
  section.append(tabs, scoreHost);
  if (view.versions.length > 0) {
    showScore(view.versions[0]);
  }
  if (ctx.statusEl && ctx.saved) {
    section.appendChild(buildPieceActions(ctx, state, view.versions.length > 0 ? view.versions[0].view.rows : []));
  }
  return section;
}

/* Shows an older version read only: its structure, then its result. */
function pieceVersionView(viewEl, id, number, body, nameOf, title) {
  viewEl.innerHTML = "";
  viewEl.appendChild(materialsElement("h2", "section-title", `Version ${number} (read only)`));
  viewEl.appendChild(buildMaterialExport(id, number, title));
  const piece = body && body.piece ? body.piece : {};
  const state = PiecesModel.stateFromBody(body, id);
  const form = PiecesModel.formOf(state.form);
  /* A sized form has no sections of its own: its size, whether it modulates and the sections the Composer plans. */
  const sizedFacts = () => {
    const size = PiecesModel.sizeOf(form, state.bars, state.count);
    return [
      ["Size", size ? PiecesModel.sizeLabel(size) : "not found"],
      ...(form.settings && form.settings.modulate === true ? [["Modulate", state.modulate ? "yes" : "no"]] : []),
      ...PiecesModel.sectionsOf(form, state.bars, state.count).map((section) => [
        `${section.role ? section.role.label : ""} ${section.letter}`,
        `${Number.isFinite(section.bars) ? `${section.bars} bars` : `${section.beats} beats`}${section.cadence ? `, cadence ${section.cadence.core}` : ""}`,
      ]),
    ];
  };
  /* The cadences a section leaves unchosen come from the Composer (the validate route); the facts are drawn again when
   * they arrive. */
  const facts = (served) =>
    themeFacts([
      ["Form", form ? form.label : state.form],
      ["Key", `${state.key} ${state.mode}, ${state.meter}`],
      ...(PiecesModel.isSized(form) ? sizedFacts() : []),
      ...state.sections.map((section, index) => {
        const cadence = PiecesModel.cadenceOf(state, index, served);
        return [
          `${PiecesModel.roleLabel(form, section.part)} ${section.letter || section.id}`,
          `${PiecesModel.beatsLabel(section.beats, state.meter)}, ${cadence ? `cadence ${cadence}, ` : ""}${section.fill.kind}${
            section.fill.item_id ? ` ${nameOf(section.fill.item_id)} v${section.fill.version}` : ""
          }`,
        ];
      }),
      ["Brief", ThemesModel.briefLabel(ThemesModel.briefFromPicks(state.picks))],
    ]);
  let factsEl = facts([]);
  viewEl.appendChild(factsEl);
  sendMaterialsRequest(PiecesModel.validateRequest(state)).then((answer) => {
    const served = answer.ok && answer.data ? PiecesModel.servedCadences(answer.data.request, state) : [];
    if (served.length > 0 && factsEl.isConnected) {
      const next = facts(served);
      factsEl.replaceWith(next);
      factsEl = next;
    }
  });
  const result = buildPieceResult({ id, version: number, piece, nameOf, title });
  viewEl.appendChild(result || materialsElement("div", "piece-list-status", "This version was not composed."));
}

/*
 * The piece editor. ctx: {id, body, usesStatus, items (all items), statusEl, saved(answer), version and title (the
 * current version number and the item name, for Export)}.
 * Returns {el, state, problems(), save(note)}.
 */
function buildPieceEditor(ctx) {
  const state = PiecesModel.stateFromBody(ctx.body, ctx.id);
  if (state.sections.length === 0 && !PiecesModel.isSized(PiecesModel.formOf(state.form))) {
    state.sections = [PiecesModel.newSection(state, PiecesModel.defaultRole(PiecesModel.formOf(state.form)))];
  }
  const el = materialsElement("section", "panel materials-editor piece-editor");
  const nameOf = (itemId) => {
    const found = ctx.items.find((item) => item.id === itemId);
    return found ? found.name : itemId;
  };
  const problemsEl = materialsElement("div", "materials-field-error");
  let selected = state.sections.length > 0 ? state.sections[0].id : "";
  let served = [];
  let showDefaultCadence = () => {};
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
  if (ctx.version) {
    el.appendChild(buildMaterialExport(ctx.id, ctx.version, ctx.title));
  }
  const header = materialsElement("div", "upload-controls");
  const formSelect = themeField(
    header,
    "Form",
    themeSelect(
      Vocabulary.forms().map((form) => ({ value: form.name, label: form.label })),
      state.form,
    ),
  );
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

  /* The strip: the lanes of a form whose request lists its sections, the Composer's sections of a sized form */
  el.appendChild(materialsElement("h2", "section-title", "Structure"));
  const sizeRow = materialsElement("div", "upload-controls");
  const sizeSelect = themeSelect([], "");
  themeField(sizeRow, "Size", sizeSelect);
  const modulateBox = materialsElement("input");
  modulateBox.type = "checkbox";
  themeField(sizeRow, "Modulate", modulateBox);
  const stripEl = materialsElement("div", "piece-strip");
  const editorEl = materialsElement("div", "piece-section-editor");
  el.append(sizeRow, stripEl, editorEl);

  /* The size and modulate controls of a sized form (hidden for any other); a size is its bars and count. */
  function drawSizeControls() {
    const form = PiecesModel.formOf(state.form);
    sizeRow.hidden = !PiecesModel.isSized(form);
    if (sizeRow.hidden) {
      return;
    }
    const options = PiecesModel.sizeOptions(form);
    sizeSelect.innerHTML = "";
    for (const option of options) {
      const el = materialsElement("option", "", option.label);
      el.value = option.value;
      sizeSelect.appendChild(el);
    }
    const chosen = PiecesModel.sizeOf(form, state.bars, state.count);
    sizeSelect.value = chosen ? `${chosen.bars}:${chosen.count}` : "";
    const canModulate = form.settings && form.settings.modulate === true;
    modulateBox.parentElement.hidden = !canModulate;
    modulateBox.checked = canModulate && state.modulate;
  }
  sizeSelect.addEventListener("input", () => {
    const [bars, count] = sizeSelect.value.split(":").map(Number);
    state.bars = bars;
    state.count = count;
    drawStrip();
    problems();
  });
  modulateBox.addEventListener("input", () => {
    state.modulate = modulateBox.checked;
    problems();
  });
  formSelect.addEventListener("input", () => {
    Object.assign(state, PiecesModel.withForm(state, formSelect.value));
    selected = state.sections.length > 0 ? state.sections[0].id : "";
    poolRefresh();
    redraw();
  });

  function drawStrip() {
    stripEl.innerHTML = "";
    const form = PiecesModel.formOf(state.form);
    drawSizeControls();
    if (PiecesModel.isSized(form)) {
      stripEl.appendChild(pieceSizedStrip(form, state.bars, state.count));
      return;
    }
    const timeline = PiecesModel.sectionTimeline(state);
    const total = Math.max(1, timeline.reduce((sum, entry) => sum + entry.beats, 0));
    for (const part of PiecesModel.rolesOf(form)) {
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
    if (PiecesModel.isSized(PiecesModel.formOf(state.form))) {
      return;
    }
    if (index < 0) {
      editorEl.appendChild(materialsElement("div", "piece-list-status", "Select a section to edit it."));
      return;
    }
    const section = state.sections[index];
    editorEl.appendChild(
      materialsElement("h3", "section-title", `Section ${section.letter || section.id} (${PiecesModel.roleLabel(PiecesModel.formOf(state.form), section.part)})`),
    );
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
    for (const value of PiecesModel.pickups(state.meter)) {
      const option = materialsElement("option");
      option.value = value;
      list.appendChild(option);
    }
    const cadence = themeField(
      controls,
      "Cadence",
      themeSelect([{ value: "", label: "Default" }, ...PiecesModel.CADENCES], section.cadence || ""),
    );
    /* The default's label names the cadence the Composer served for this section, once it has. */
    showDefaultCadence = () => {
      const shown = section.cadence ? "" : PiecesModel.cadenceOf(state, index, served);
      cadence.options[0].textContent = shown ? `Default (${shown})` : "Default";
    };
    showDefaultCadence();
    let drawInto = () => {};
    function syncSection() {
      section.letter = letter.value;
      const barCount = Number(bars.value);
      const extraCount = Number(extra.value);
      section.beats = (Number.isInteger(barCount) && barCount >= 0 ? barCount : 0) * per + (Number.isInteger(extraCount) && extraCount >= 0 ? extraCount : 0);
      lengthEl.textContent = PiecesModel.beatsLabel(section.beats, state.meter);
      section.pickup = pickup.value.trim() === "" ? "0" : pickup.value;
      section.cadence = cadence.value || null;
      showDefaultCadence();
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
          const free = PiecesModel.motifIsFree(section, state.meter);
          if (free) {
            intoEl.appendChild(
              materialsElement(
                "span",
                "piece-list-status",
                "Free phrase: a theme needs 4 or 8 bars, or a construction, and no pickup.",
              ),
            );
          }
          if (!PiecesModel.constructionPossible(section, state.meter)) {
            return;
          }
          /* The choice of a section of 3 to 16 whole bars: a free phrase (a theme as today at 4 or 8 bars), a theme the
           * Composer builds a construction for, or a theme with the construction given (P47, R-468). */
          const regular = ThemesModel.REGULAR_BARS.includes(Number(section.beats) / PiecesModel.barBeats(state.meter));
          const choice = themeField(
            intoEl,
            "As",
            themeSelect(
              [
                { value: "default", label: regular ? "Theme" : "Free phrase" },
                { value: "auto", label: "Theme, chosen" },
                { value: "given", label: "Theme, given" },
              ],
              PiecesModel.constructionChoice(fill),
            ),
          );
          choice.addEventListener("input", () => {
            if (choice.value === "auto") {
              fill.construction = "auto";
            } else if (choice.value === "given") {
              fill.construction = ThemesModel.defaultConstruction(
                ThemesModel.pairingIdeaNames(ThemesModel.pairing(fill)),
                section.beats / PiecesModel.barBeats(state.meter),
              );
            } else {
              fill.construction = null;
            }
            drawInto();
            problems();
          });
          if (free) {
            return;
          }
          const into = themeField(intoEl, "Into", themeSelect(PiecesModel.INTOS, fill.into || "sentence"));
          const [beginning, ending] = (fill.hybrid || "presentation+continuation").split("+");
          const beginningSelect = themeField(intoEl, "Beginning", themeSelect(ThemesModel.BEGINNINGS, beginning));
          const endingSelect = themeField(intoEl, "Ending", themeSelect(ThemesModel.ENDINGS, ending));
          let block = null;
          function syncInto() {
            fill.into = into.value;
            fill.hybrid = `${beginningSelect.value}+${endingSelect.value}`;
            beginningSelect.parentElement.hidden = fill.into !== "hybrid";
            endingSelect.parentElement.hidden = fill.into !== "hybrid";
            if (block) {
              block.refresh();
            }
          }
          for (const control of [into, beginningSelect, endingSelect]) {
            control.addEventListener("input", syncInto);
          }
          if (PiecesModel.constructionChoice(fill) === "given") {
            block = buildConstructionBlock({
              ideaNames: () => ThemesModel.pairingIdeaNames(ThemesModel.pairing(fill)),
              bars: () => section.beats / PiecesModel.barBeats(state.meter),
              get: () => fill.construction,
              set: (value) => {
                fill.construction = value;
              },
              changed: () => problems(),
              modeHidden: true,
            });
            block.element.open = true;
            intoEl.appendChild(block.element);
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

  /* The Composer's checks of the values (key, meter, tempo, seed, pickups, numerals, sizes) come from the validate route. */
  const validation = attachFormValidation({
    roots: [el],
    build: () => PiecesModel.validateRequest(state),
    field: PiecesModel.fieldForPath,
    fields: () => ({ form: formSelect, key, mode, meter, tempo, sections: stripEl, size: sizeSelect, modulate: modulateBox }),
    onResult: (result) => {
      served = result.ok && result.complete ? PiecesModel.servedCadences(result.request, state) : [];
      showDefaultCadence();
    },
  });
  el.appendChild(validation.element);

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
      (typeof structure.form !== "string" && !Array.isArray(structure.sections));
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
      validation.refuse(answer.data);
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
