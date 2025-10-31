/* The Variants section of the motif detail (P13 B9): the Generate control
 * with its operation picker, and the unsaved candidates. Renders from
 * VariantTree; ctx is {id, name, body, send(request)}, send being the page's
 * JSON request helper that answers {ok, status, data}. */
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
  function paramControls(operation, values) {
    const box = h("div", "variant-params");
    for (const param of operation.params) {
      const label = h("label", "filter-field", `${param.name} `);
      let control;
      if (param.kind === "bool") {
        control = h("input");
        control.type = "checkbox";
        control.addEventListener("change", () => {
          values[param.name] = control.checked;
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

  function buildPicker(groups, steps, onChange) {
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
        paramBox.appendChild(paramControls(operation, values));
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
        const remove = h("button", "", "Remove");
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
    const add = h("button", "", "Add step");
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

  function candidateCard(candidate) {
    const view = VariantTree.candidateView(candidate);
    const card = h("li", "variant-candidate");
    card.appendChild(h("div", "variant-derivation", view.derivation));
    const meta = h("div", "variant-meta");
    meta.appendChild(h("span", `variant-badge variant-kind-${view.kind.value}`, view.kind.text));
    meta.appendChild(chipRow(view.chips));
    if (view.recognizability) {
      meta.appendChild(h("span", "variant-recognizability", view.recognizability));
    }
    card.appendChild(meta);
    return card;
  }

  function mount(parent, ctx) {
    const section = h("section", "panel materials-variants");
    section.appendChild(h("h2", "section-title", "Variants"));
    const status = h("div", "upload-status");
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
    const generate = h("button", "", "Generate");
    generate.type = "button";
    const countLabel = h("label", "filter-field", "Count ");
    countLabel.appendChild(count);
    controls.append(countLabel, seed, generate, summary);

    const pickerHost = h("details", "variant-picker-host");
    pickerHost.appendChild(h("summary", "", "Choose the operations (empty: random chains)"));
    const candidates = h("ol", "variant-candidates");
    section.append(controls, pickerHost, status, candidates);
    parent.appendChild(section);

    let picker = null;
    function updateSummary() {
      summary.textContent = steps.length > 0 ? steps.map((step) => step.op).join(" then ") : "Random operations";
    }
    ctx.send(VariantTree.operationsRequest()).then((answer) => {
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not load the operations"), true);
        return;
      }
      picker = buildPicker(VariantTree.operationGroups(answer.data.operations), steps, updateSummary);
      pickerHost.appendChild(picker.el);
    });

    let shown = [];
    function drawCandidates() {
      candidates.innerHTML = "";
      for (const candidate of shown) {
        candidates.appendChild(candidateCard(candidate));
      }
    }

    generate.addEventListener("click", async () => {
      const made = VariantTree.generateRequest(ctx.id, { count: count.value, seed: seed.value, steps });
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
      drawCandidates();
      setStatus(status, shown.length === 0 ? "No variants came out" : `Seed ${answer.data.seed}`, false);
    });

    return { el: section };
  }

  return { mount };
})();
