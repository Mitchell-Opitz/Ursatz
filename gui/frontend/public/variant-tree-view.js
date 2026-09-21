/* The Variants section of the motif detail (P13 B9), top to bottom: the Generate control with its operation
 * picker, the unsaved candidates and the saved tree. Each candidate and each saved variant (on request) shows its
 * notation in a compact score workspace of its own (P18 B4). Renders from
 * VariantTree; ctx is {id, name, body, projectId, projects, open(id), send(request)}, send being the page's
 * JSON request helper that answers {ok, status, data}, open going to a motif's Motif tab. */
const VariantTreeView = (function () {
  function h(tag, className, text) {
    const el = document.createElement(tag);
    if (className) {
      el.className = className;
    }
    if (text !== undefined) {
      el.textContent = text;
    }
    return el;
  }

  function setStatus(el, text, isError) {
    el.textContent = text;
    el.classList.toggle("error", Boolean(isError));
  }

  function errorText(data, fallback) {
    return data && typeof data.error === "string" && data.error !== "" ? data.error : fallback;
  }

  function chipRow(chips) {
    const row = h("span", "variant-chips");
    for (const chip of chips) {
      row.appendChild(h("span", `variant-chip variant-chip-${chip.state}`, chip.text));
    }
    return row;
  }

  /* One control per parameter of the chosen operation; an empty text leaves it open. */
  function paramControls(operation, values, features) {
    const box = h("div", "variant-params");
    for (const param of operation.params) {
      const label = h("label", "filter-field", `${param.name === "feature" ? "segment" : param.name} `);
      let control;
      if (param.kind === "bool") {
        control = h("input");
        control.type = "checkbox";
        control.addEventListener("change", () => {
          values[param.name] = control.checked;
        });
      } else if (param.name === "feature" && param.kind === "int") {
        control = h("select");
        for (const option of VariantTree.featureOptions(features)) {
          const o = h("option", "", option.text);
          o.value = option.value;
          control.appendChild(o);
        }
        control.addEventListener("change", () => {
          values[param.name] = control.value;
        });
      } else if (param.kind === "position") {
        control = h("select");
        for (const name of ["", "leading", "trailing", "middle"]) {
          const o = h("option", "", name === "" ? "drawn" : name);
          o.value = name;
          control.appendChild(o);
        }
        control.addEventListener("change", () => {
          values[param.name] = control.value;
        });
      } else {
        control = h("input");
        control.type = "text";
        control.size = 8;
        control.placeholder = param.kind === "ratio" ? "drawn, or 2:1" : "drawn";
        control.addEventListener("input", () => {
          values[param.name] = control.value;
        });
      }
      control.title = param.description || "";
      label.appendChild(control);
      box.appendChild(label);
    }
    return box;
  }

  function buildPicker(groups, steps, onChange, features) {
    const box = h("div", "variant-picker");
    const list = h("ol", "variant-steps");
    const select = h("select");
    const byName = new Map();
    for (const group of groups) {
      const optgroup = h("optgroup");
      optgroup.label = group.title;
      for (const operation of group.operations) {
        byName.set(operation.name, operation);
        const o = h("option", "", `${operation.label} (${operation.citation})`);
        o.value = operation.name;
        optgroup.appendChild(o);
      }
      select.appendChild(optgroup);
    }
    const values = {};
    const paramBox = h("div");
    const error = h("div", "upload-status");
    function showParams() {
      const operation = byName.get(select.value);
      for (const key of Object.keys(values)) {
        delete values[key];
      }
      paramBox.innerHTML = "";
      if (operation) {
        paramBox.appendChild(paramControls(operation, values, features));
      }
    }
    function drawSteps() {
      list.innerHTML = "";
      steps.forEach((step, index) => {
        const item = h("li", "variant-step");
        const params = step.params
          ? ` (${Object.keys(step.params)
              .map((name) => `${name} ${step.params[name]}`)
              .join(", ")})`
          : "";
        item.appendChild(h("span", "", `${byName.has(step.op) ? byName.get(step.op).label : step.op}${params} `));
        const remove = h("button", "button-secondary", "Remove");
        remove.type = "button";
        remove.addEventListener("click", () => {
          steps.splice(index, 1);
          drawSteps();
          onChange();
        });
        item.appendChild(remove);
        list.appendChild(item);
      });
      onChange();
    }
    const add = h("button", "button-secondary", "Add step");
    add.type = "button";
    add.addEventListener("click", () => {
      const operation = byName.get(select.value);
      if (!operation) {
        return;
      }
      const made = VariantTree.stepFromValues(operation, values);
      if (made.error) {
        setStatus(error, made.error, true);
        return;
      }
      setStatus(error, "", false);
      steps.push(made.step);
      drawSteps();
    });
    select.addEventListener("change", showParams);
    box.append(select, paramBox, add, error, list);
    showParams();
    return { el: box, clear: () => { steps.length = 0; drawSteps(); } };
  }

  function actionButton(text, onClick, variant) {
    const button = h("button", variant || "button-secondary", text);
    button.type = "button";
    button.addEventListener("click", onClick);
    return button;
  }

  /* actions: {play, approve, reject, reroll}, each called with no arguments. */
  function candidateCard(candidate, actions) {
    const view = VariantTree.candidateView(candidate);
    const card = h("li", "variant-candidate");
    card.appendChild(h("div", "variant-derivation", view.derivation));
    card.scoreHost = h("div", "variant-score");
    card.appendChild(card.scoreHost);
    const meta = h("div", "variant-meta");
    meta.appendChild(h("span", `variant-badge variant-kind-${view.kind.value}`, view.kind.text));
    meta.appendChild(chipRow(view.chips));
    if (view.recognizability) {
      meta.appendChild(h("span", "variant-recognizability", view.recognizability));
    }
    if (view.remoteness) {
      meta.appendChild(h("span", "variant-remoteness", view.remoteness));
    }
    card.appendChild(meta);
    card.appendChild(h("div", "variant-suits-line", view.suits));
    if (view.harmony) {
      card.appendChild(h("div", "variant-harmony", view.harmony));
    }
    const buttons = h("div", "variant-actions");
    buttons.append(
      actionButton("Play", actions.play),
      actionButton("Approve", actions.approve),
      actionButton("Reject", actions.reject, "button-danger"),
      actionButton("Reroll", actions.reroll),
    );
    card.appendChild(buttons);
    return card;
  }

  /* The inline Save as new motif form of a saved variant: its name, the project (the parent's to start with) and
   * Save, which calls actions.promote(row, name, projectId). Returns the elements to append. */
  function promoteFields(row, actions) {
    const name = h("input");
    name.type = "text";
    name.value = row.name;
    const nameLabel = h("label", "filter-field", "Name ");
    nameLabel.appendChild(name);
    const project = h("select");
    for (const entry of actions.projects()) {
      const option = h("option", "", entry.name);
      option.value = entry.id;
      project.appendChild(option);
    }
    project.value = actions.projectId();
    const projectLabel = h("label", "filter-field", "Project ");
    projectLabel.appendChild(project);
    const save = actionButton("Save", () => actions.promote(row, name.value, project.value), "button-primary");
    return [nameLabel, projectLabel, save];
  }

  /* The Promoted list under the saved variants: each promoted motif as "<name> (opens it)" with its derivation.
   * open(id) opens that motif. */
  function promotedList(promoted, open) {
    const box = h("div", "variant-promoted");
    if (promoted.length === 0) {
      return box;
    }
    box.appendChild(h("h3", "section-title", "Promoted"));
    const list = h("ul", "variant-promoted-list");
    for (const entry of promoted) {
      const item = h("li", "variant-promoted-item");
      const link = h("a", "clickable", VariantTree.promotedText(entry));
      link.addEventListener("click", () => open(entry.id));
      item.appendChild(link);
      if (entry.derivation) {
        item.appendChild(h("div", "variant-derivation", entry.derivation));
      }
      list.appendChild(item);
    }
    box.appendChild(list);
    return box;
  }

  /* One saved node: indented by depth, with its derivation, flag, kind and actions.
   * actions: {flag(id, flag), kind(id, kind), reroll(id), play(row), toggle(row)}; isOpen is whether
   * its notation is shown. The row's scoreHost is where the compact score mounts. */
  function treeRow(row, actions, isOpen) {
    const item = h("li", `variant-node variant-flag-${row.flag.value}`);
    item.style.marginLeft = `${row.depth * 1.5}rem`;
    const title = h("div", "variant-node-title");
    title.appendChild(h("strong", "", row.name));
    title.appendChild(h("span", "variant-badge", `v${row.version}`));
    if (!row.isRoot) {
      title.appendChild(h("span", `variant-badge variant-flag-badge-${row.flag.value}`, row.flag.text));
    }
    item.appendChild(title);
    if (row.isRoot) {
      return item;
    }
    if (row.derivation) {
      item.appendChild(h("div", "variant-derivation", row.derivation));
    }
    item.appendChild(chipRow(row.chips));
    const meta = h("div", "variant-meta");
    if (row.remoteness) {
      meta.appendChild(h("span", "variant-remoteness", row.remoteness));
    }
    item.appendChild(meta);
    item.appendChild(h("div", "variant-suits-line", row.suits));
    if (row.harmony) {
      item.appendChild(h("div", "variant-harmony", row.harmony));
    }
    const buttons = h("div", "variant-actions");
    const kind = h("select");
    for (const value of VariantTree.KINDS) {
      const o = h("option", "", VariantTree.KIND_LABELS[value]);
      o.value = value;
      kind.appendChild(o);
    }
    kind.value = row.kindValue;
    kind.addEventListener("change", () => actions.kind(row.id, kind.value));
    buttons.append(
      kind,
      actionButton("Play", () => actions.play(row)),
      actionButton("Approve", () => actions.flag(row.id, "approved")),
      actionButton("Reject", () => actions.flag(row.id, "rejected"), "button-danger"),
      actionButton("Unflag", () => actions.flag(row.id, "unflagged")),
      ...(row.seed === null ? [] : [actionButton("Reroll", () => actions.reroll(row.id))]),
    );
    buttons.appendChild(actionButton(isOpen ? "Hide notation" : "Show notation", () => actions.toggle(row)));
    const promoteForm = h("div", "variant-promote-form");
    promoteForm.hidden = true;
    buttons.appendChild(
      actionButton("Save as new motif", () => {
        promoteForm.hidden = !promoteForm.hidden;
      }),
    );
    item.appendChild(buttons);
    item.appendChild(promoteForm);
    promoteForm.append(...promoteFields(row, actions));
    item.scoreHost = h("div", "variant-score");
    item.appendChild(item.scoreHost);
    return item;
  }

  const REJECTED_KEY = "ursatz.variants.showRejected";

  function loadShowRejected() {
    try {
      return window.localStorage.getItem(REJECTED_KEY) === "1";
    } catch (error) {
      return false;
    }
  }

  function storeShowRejected(value) {
    try {
      window.localStorage.setItem(REJECTED_KEY, value ? "1" : "0");
    } catch (error) {
      /* The toggle still works for this page view. */
    }
  }

  function mount(parent, ctx) {
    const section = h("section", "panel materials-variants");
    section.appendChild(h("h2", "section-title", "Variants"));
    /* The tab's one status area, above the panel (R-191). */
    const status = h("div", "motif-tab-status");
    status.setAttribute("role", "status");
    const steps = [];
    const controls = h("div", "upload-controls");
    const count = h("input");
    count.type = "number";
    count.min = "1";
    count.max = String(VariantTree.MAX_COUNT);
    count.value = String(VariantTree.DEFAULT_COUNT);
    const seed = h("input");
    seed.type = "text";
    seed.size = 10;
    seed.placeholder = "seed (random)";
    const summary = h("span", "variant-chain-summary", "Random operations");
    const generate = h("button", "button-primary", "Generate");
    generate.type = "button";
    const countLabel = h("label", "filter-field", "Count ");
    countLabel.appendChild(count);
    const rejectedLabel = h("label", "filter-field", "Show rejected ");
    const rejectedBox = h("input");
    rejectedBox.type = "checkbox";
    rejectedBox.checked = loadShowRejected();
    rejectedLabel.appendChild(rejectedBox);
    const distance = h("select");
    for (const option of VariantTree.DISTANCES) {
      const o = h("option", "", option.text);
      o.value = option.value;
      distance.appendChild(o);
    }
    const distanceLabel = h("label", "filter-field", "Distance ");
    distanceLabel.appendChild(distance);
    const functionBoxes = new Map();
    const functionFilter = h("span", "variant-function-filter", "Only variants that suit ");
    for (const name of VariantTree.FUNCTIONS) {
      const label = h("label", "filter-field", `${name} `);
      const box = h("input");
      box.type = "checkbox";
      label.appendChild(box);
      functionFilter.appendChild(label);
      functionBoxes.set(name, box);
    }
    controls.append(countLabel, distanceLabel, seed, generate, summary, rejectedLabel);
    controls.appendChild(functionFilter);

    const pickerHost = h("details", "variant-picker-host");
    pickerHost.appendChild(h("summary", "", "Choose the operations (empty: random chains)"));
    /* The Suits filter narrows the candidates and the saved variants shown, not what is generated or stored. */
    const suitsRow = h("div", "variant-suits");
    const suits = h("select");
    for (const option of VariantTree.SUITS_FILTERS) {
      const o = h("option", "", option.text);
      o.value = option.value;
      suits.appendChild(o);
    }
    const suitsLabel = h("label", "filter-field", "Suits ");
    suitsLabel.appendChild(suits);
    suitsRow.append(suitsLabel, h("div", "piece-list-status", VariantTree.SUITS_EXPLANATION));
    const candidates = h("ol", "variant-candidates");
    const tree = h("ul", "variant-tree");
    const treeNote = h("div", "piece-list-status");
    const promotedHost = h("div");
    section.append(controls, pickerHost, suitsRow, candidates, h("h3", "section-title", "Saved variants"), treeNote, tree, promotedHost);
    parent.appendChild(status);
    parent.appendChild(section);

    let picker = null;
    let operationList = [];
    function updateSummary() {
      /* A distance is drawn from random chains, so chosen operations switch it off. */
      distance.disabled = steps.length > 0;
      if (distance.disabled) {
        distance.value = "";
      }
      summary.textContent = steps.length > 0 ? steps.map((step) => step.op).join(" then ") : "Random operations";
    }
    ctx.send(VariantTree.operationsRequest()).then((answer) => {
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not load the operations"), true);
        return;
      }
      operationList = answer.data.operations;
      if (eachOpResults) {
        drawEachOp();
      }
      picker = buildPicker(
        VariantTree.operationGroups(answer.data.operations),
        steps,
        updateSummary,
        ctx.body && ctx.body.motif ? ctx.body.motif.features : [],
      );
      pickerHost.appendChild(picker.el);
    });

    let shown = [];
    const rejectedListeners = [];
    rejectedBox.addEventListener("change", () => {
      storeShowRejected(rejectedBox.checked);
      for (const listener of rejectedListeners) {
        listener(rejectedBox.checked);
      }
    });

    /* Saves a candidate with a flag and takes it off the list. */
    async function saveCandidate(candidate, flag) {
      const answer = await ctx.send(VariantTree.saveRequest(ctx.id, candidate, flag));
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not save the variant"), true);
        return;
      }
      shown = shown.filter((other) => other !== candidate);
      drawCandidates();
      setStatus(status, `Saved ${answer.data.item.name} as ${flag}`, false);
      refreshTree();
    }

    /* Regenerates only this candidate, with the next seed. */
    async function rerollCandidate(candidate) {
      setStatus(status, "Rerolling...", false);
      const answer = await ctx.send(VariantTree.candidateRerollRequest(ctx.id, candidate));
      const next = answer.ok && Array.isArray(answer.data.variants) ? answer.data.variants[0] : null;
      if (!next) {
        setStatus(status, errorText(answer.data, "No different variant came out"), true);
        return;
      }
      shown = shown.map((other) => (other === candidate ? next : other));
      drawCandidates();
      setStatus(status, `Rerolled with seed ${next.seed}`, false);
    }

    /* Each candidate card carries a compact score. They render through a queue of VariantTree.RENDER_LIMIT, and the
     * Composer's answer is kept per candidate so a redraw does not ask again. The blob URLs live as long as the cards
     * (revoked on redraw and when the tab is left). */
    const renderQueue = VariantTree.createQueue(VariantTree.RENDER_LIMIT);
    let rendered = new Map();
    let candidateScores = [];
    let drawGeneration = 0;

    function clearCandidateScores() {
      drawGeneration++;
      for (const entry of candidateScores) {
        if (entry.workspace) {
          entry.workspace.destroy();
        }
        for (const url of entry.urls) {
          URL.revokeObjectURL(url);
        }
      }
      candidateScores = [];
    }

    async function showCandidateScore(candidate, host, entry, generation) {
      host.textContent = "Rendering...";
      let data = rendered.get(candidate);
      if (!data) {
        const answer = await ctx.send(MaterialsModel.renderRequest(VariantTree.candidateBody(ctx.body, candidate)));
        if (generation !== drawGeneration) {
          return;
        }
        if (!answer.ok) {
          host.textContent = errorText(answer.data, "Could not render the variant");
          host.classList.add("error");
          return;
        }
        data = answer.data;
        rendered.set(candidate, data);
      }
      if (generation !== drawGeneration) {
        return;
      }
      host.textContent = "";
      const xml = URL.createObjectURL(new Blob([data.musicxml], { type: "application/xml" }));
      const playback = URL.createObjectURL(new Blob([JSON.stringify(data.playback)], { type: "application/json" }));
      entry.urls = [xml, playback];
      entry.workspace = ScoreWorkspace.mount(host, {
        compact: true,
        title: VariantTree.candidateName(ctx.name, candidate),
        subtitle: `candidate · seed ${candidate.seed}`,
        musicxmlUrl: xml,
        playbackUrl: playback,
      });
    }

    function drawCandidates() {
      clearCandidateScores();
      const generation = drawGeneration;
      candidates.innerHTML = "";
      for (const candidate of shown.filter((c) => VariantTree.suitsMatch(c.functions, suits.value))) {
        const entry = { workspace: null, urls: [] };
        candidateScores.push(entry);
        const card = candidateCard(candidate, {
          play: () => (entry.workspace ? entry.workspace.play() : setStatus(status, "The notation is still rendering", false)),
          approve: () => saveCandidate(candidate, "approved"),
          reject: () => saveCandidate(candidate, "rejected"),
          reroll: () => rerollCandidate(candidate),
        });
        candidates.appendChild(card);
        renderQueue.add(() => (generation === drawGeneration ? showCandidateScore(candidate, card.scoreHost, entry, generation) : null));
      }
    }

    /* The saved rows whose notation is open, by item id. The set outlives refreshTree; the scores are mounted again
     * each time the tree is drawn. */
    const openRows = new Set();
    let rowScores = [];

    function clearRowScores() {
      for (const workspace of rowScores) {
        workspace.destroy();
      }
      rowScores = [];
    }

    function mountRowScore(row, host, autoplay) {
      const urls = MaterialsModel.versionScoreUrls(row.id, row.version);
      rowScores.push(
        ScoreWorkspace.mount(host, {
          compact: true,
          autoplay,
          title: row.name,
          subtitle: `variant · v${row.version}`,
          musicxmlUrl: urls.musicxmlUrl,
          playbackUrl: urls.playbackUrl,
        }),
      );
    }

    function toggleNotation(row) {
      if (openRows.has(row.id)) {
        openRows.delete(row.id);
      } else {
        openRows.add(row.id);
      }
      drawTree();
    }

    /* Plays the row's score, opening its notation first when it is closed. */
    let autoplayId = null;
    function playSaved(row) {
      if (openRows.has(row.id)) {
        const index = rowScoreIds.indexOf(row.id);
        if (index >= 0) {
          rowScores[index].play();
        }
        return;
      }
      openRows.add(row.id);
      autoplayId = row.id;
      drawTree();
    }

    let savedTree = null;
    const treeActions = {
      async flag(id, flag) {
        const answer = await ctx.send(VariantTree.flagRequest(id, flag));
        setStatus(status, answer.ok ? "" : errorText(answer.data, "Could not change the flag"), !answer.ok);
        refreshTree();
      },
      async kind(id, kind) {
        const row = VariantTree.treeRows(savedTree, true).rows.find((r) => r.id === id);
        const flag = row ? row.flag.value : "unflagged";
        const answer = await ctx.send(VariantTree.flagRequest(id, flag, kind));
        setStatus(status, answer.ok ? "" : errorText(answer.data, "Could not change the kind"), !answer.ok);
        refreshTree();
      },
      async reroll(id) {
        setStatus(status, "Rerolling...", false);
        const answer = await ctx.send(VariantTree.rerollRequest(id));
        setStatus(status, answer.ok ? "Rerolled" : errorText(answer.data, "Could not reroll"), !answer.ok);
        refreshTree();
      },
      async promote(row, name, projectId) {
        if (String(name).trim() === "") {
          setStatus(status, "Give the new motif a name", true);
          return;
        }
        const answer = await ctx.send(VariantTree.promoteRequest(row.id, name, projectId));
        if (!answer.ok) {
          setStatus(status, errorText(answer.data, "Could not save the variant as a new motif"), true);
          return;
        }
        ctx.open(row.id);
      },
      projects: () => ctx.projects,
      projectId: () => ctx.projectId,
      play: playSaved,
      toggle: toggleNotation,
    };

    let rowScoreIds = [];
    function drawTree() {
      clearRowScores();
      rowScoreIds = [];
      tree.innerHTML = "";
      const shownRows = VariantTree.treeRows(savedTree, rejectedBox.checked, suits.value);
      for (const row of shownRows.rows) {
        const open = !row.isRoot && openRows.has(row.id);
        const item = treeRow(row, treeActions, open);
        tree.appendChild(item);
        if (open) {
          rowScoreIds.push(row.id);
          mountRowScore(row, item.scoreHost, autoplayId === row.id);
        }
      }
      autoplayId = null;
      promotedHost.innerHTML = "";
      promotedHost.appendChild(promotedList(shownRows.promoted, ctx.open));
      const saved = shownRows.rows.length - 1;
      treeNote.textContent =
        saved <= 0 && shownRows.hidden === 0
          ? suits.value
            ? `No saved variant suits ${suits.value}.`
            : "No saved variants yet."
          : shownRows.hidden > 0
            ? `${shownRows.hidden} rejected hidden`
            : "";
    }

    async function refreshTree() {
      const answer = await ctx.send(VariantTree.treeRequest(ctx.id));
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not load the variant tree"), true);
        return;
      }
      savedTree = answer.data.tree;
      drawTree();
    }
    rejectedListeners.push(drawTree);
    suits.addEventListener("change", () => {
      drawCandidates();
      drawTree();
    });

    generate.addEventListener("click", async () => {
      const made = VariantTree.generateRequest(ctx.id, {
        count: count.value,
        seed: seed.value,
        steps,
        functions: VariantTree.FUNCTIONS.filter((name) => functionBoxes.get(name).checked),
        distance: distance.value,
      });
      if (made.error) {
        setStatus(status, made.error, true);
        return;
      }
      setStatus(status, "Generating...", false);
      const answer = await ctx.send(made.request);
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not generate variants"), true);
        return;
      }
      shown = Array.isArray(answer.data.variants) ? answer.data.variants : [];
      rendered = new Map();
      drawCandidates();
      setStatus(status, VariantTree.generatedNote(shown, Number(count.value), answer.data.seed, distance.value), false);
    });

    const blocked = VariantTree.noDegreesMessage(ctx.body && ctx.body.motif);
    if (blocked) {
      generate.disabled = true;
      setStatus(status, blocked, true);
    }

    /* Try every operation (R-190): each registry operation on this motif, marked Works or Wrong. The marks live in
     * the motif's op-checks file, not in a version. Scores render through the same queue as the candidates. */
    const eachOp = h("div", "variant-each-op");
    eachOp.appendChild(h("h3", "section-title", "Try every operation"));
    const eachOpControls = h("div", "upload-controls");
    const eachOpSeed = h("input");
    eachOpSeed.type = "text";
    eachOpSeed.size = 10;
    eachOpSeed.placeholder = "seed (random)";
    const eachOpRun = h("button", "button-primary", "Run");
    eachOpRun.type = "button";
    const eachOpCopy = h("button", "button-secondary", "Copy findings");
    eachOpCopy.type = "button";
    eachOpControls.append(eachOpSeed, eachOpRun, eachOpCopy);
    const eachOpList = h("div", "variant-each-op-results");
    eachOp.append(eachOpControls, eachOpList);
    section.appendChild(eachOp);

    let eachOpResults = null;
    let opChecks = {};
    let opScores = [];
    let opGeneration = 0;
    const opRendered = new Map();

    function clearOpScores() {
      opGeneration++;
      for (const entry of opScores) {
        if (entry.workspace) {
          entry.workspace.destroy();
        }
        for (const url of entry.urls) {
          URL.revokeObjectURL(url);
        }
      }
      opScores = [];
    }

    async function showOpScore(variant, op, host, entry, generation) {
      host.textContent = "Rendering...";
      let data = opRendered.get(variant);
      if (!data) {
        const answer = await ctx.send(MaterialsModel.renderRequest(VariantTree.candidateBody(ctx.body, variant)));
        if (generation !== opGeneration) {
          return;
        }
        if (!answer.ok) {
          host.textContent = errorText(answer.data, "Could not render the result");
          host.classList.add("error");
          return;
        }
        data = answer.data;
        opRendered.set(variant, data);
      }
      if (generation !== opGeneration) {
        return;
      }
      host.textContent = "";
      const xml = URL.createObjectURL(new Blob([data.musicxml], { type: "application/xml" }));
      const playback = URL.createObjectURL(new Blob([JSON.stringify(data.playback)], { type: "application/json" }));
      entry.urls = [xml, playback];
      entry.workspace = ScoreWorkspace.mount(host, {
        compact: true,
        title: VariantTree.candidateName(ctx.name, variant),
        subtitle: `${op} · seed ${variant.seed}`,
        musicxmlUrl: xml,
        playbackUrl: playback,
      });
    }

    async function markOperation(op, verdict, comment, variant) {
      const next = { ...opChecks, [op]: VariantTree.makeMark(verdict, comment, variant, new Date().toISOString().slice(0, 10)) };
      const answer = await ctx.send(VariantTree.opChecksPutRequest(ctx.id, next));
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not save the mark"), true);
        return;
      }
      opChecks = answer.data.checks;
      setStatus(status, `${op} marked ${verdict}`, false);
      drawEachOp();
    }

    function eachOpEntry(entry, generation) {
      const item = h("div", "variant-each-op-entry");
      item.appendChild(h("strong", "", `${entry.label}${entry.citation ? ` (${entry.citation})` : ""}`));
      if (entry.variant.derivation) {
        item.appendChild(h("div", "variant-derivation", entry.variant.derivation));
      }
      const score = { workspace: null, urls: [] };
      opScores.push(score);
      const host = h("div", "variant-score");
      item.appendChild(host);
      const mark = VariantTree.markView(opChecks[entry.op], entry.variant);
      if (mark) {
        const line = h("div", `variant-mark variant-mark-${mark.verdict}`);
        line.appendChild(h("span", "variant-badge", mark.text));
        line.appendChild(h("span", "", `${mark.comment ? `${mark.comment} ` : ""}(${mark.date})`));
        if (mark.changed) {
          line.appendChild(h("span", "variant-badge variant-mark-changed", "changed since"));
        }
        item.appendChild(line);
      }
      const comment = h("input");
      comment.type = "text";
      comment.size = 30;
      comment.maxLength = VariantTree.COMMENT_MAX;
      comment.placeholder = "comment";
      comment.value = opChecks[entry.op] ? String(opChecks[entry.op].comment || "") : "";
      const buttons = h("div", "variant-actions");
      buttons.append(
        actionButton("Play", () => (score.workspace ? score.workspace.play() : setStatus(status, "The notation is still rendering", false))),
        actionButton("Works", () => markOperation(entry.op, "works", comment.value, entry.variant)),
        actionButton("Wrong", () => markOperation(entry.op, "wrong", comment.value, entry.variant), "button-danger"),
        comment,
      );
      item.appendChild(buttons);
      renderQueue.add(() => (generation === opGeneration ? showOpScore(entry.variant, entry.op, host, score, generation) : null));
      return item;
    }

    function drawEachOp() {
      clearOpScores();
      const generation = opGeneration;
      eachOpList.innerHTML = "";
      if (!eachOpResults) {
        return;
      }
      for (const group of VariantTree.eachOpGroups(eachOpResults.variants, operationList)) {
        eachOpList.appendChild(h("h4", "section-title", group.title));
        for (const entry of group.entries) {
          eachOpList.appendChild(eachOpEntry(entry, generation));
        }
      }
      const skipped = Array.isArray(eachOpResults.skipped) ? eachOpResults.skipped : [];
      if (skipped.length > 0) {
        eachOpList.appendChild(h("h4", "section-title", "Skipped"));
        const list = h("ul", "variant-skipped");
        for (const entry of skipped) {
          list.appendChild(h("li", "", `${entry.op}: ${entry.reason}`));
        }
        eachOpList.appendChild(list);
      }
    }

    eachOpRun.addEventListener("click", async () => {
      const made = VariantTree.eachOpRequest(ctx.id, eachOpSeed.value);
      if (made.error) {
        setStatus(status, made.error, true);
        return;
      }
      setStatus(status, "Running every operation...", false);
      const answer = await ctx.send(made.request);
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not run the operations"), true);
        return;
      }
      eachOpResults = { variants: answer.data.variants || [], skipped: answer.data.skipped || [], seed: answer.data.seed };
      opRendered.clear();
      drawEachOp();
      setStatus(status, `${eachOpResults.variants.length} operations ran with seed ${eachOpResults.seed}`, false);
    });
    /* The marked operations as text, for pasting into a chat. Registry order comes from the operations list. */
    eachOpCopy.addEventListener("click", async () => {
      const text = VariantTree.findingsText(
        ctx.name,
        ctx.body && ctx.body.motif,
        opChecks,
        operationList.map((operation) => operation.name),
      );
      try {
        await navigator.clipboard.writeText(text);
        setStatus(status, "Copied the findings", false);
      } catch (error) {
        setStatus(status, "Could not copy the findings", true);
      }
    });
    ctx.send(VariantTree.opChecksRequest(ctx.id)).then((answer) => {
      if (answer.ok && answer.data.checks) {
        opChecks = answer.data.checks;
        if (eachOpResults) {
          drawEachOp();
        }
      }
    });
    if (blocked) {
      eachOpRun.disabled = true;
    }

    refreshTree();
    return {
      el: section,
      refresh: refreshTree,
      /* The tab was left: stop and free the scores. Back on the tab they are drawn again. */
      leave: () => {
        clearCandidateScores();
        clearRowScores();
        clearOpScores();
      },
      enter: () => {
        if (shown.length > 0 && candidateScores.length === 0) {
          drawCandidates();
        }
        if (savedTree) {
          drawTree();
        }
        if (eachOpResults && opScores.length === 0) {
          drawEachOp();
        }
      },
      showRejected: () => rejectedBox.checked,
      onShowRejected: (fn) => rejectedListeners.push(fn),
    };
  }

  return { mount };
})();
