/* The motif detail page (P18 B3): a header, a tab bar (Motif, Variants, Used by, Other) and one pane per tab. The
 * panes are built once and shown or hidden, so the editor keeps what was typed when another tab is open. The
 * Variants pane mounts the first time it is shown. The pieces (buildMotifEditor, buildMotifScore, buildAnalysisPanel,
 * buildUsedByTable, buildUsesSection, buildVersionsList, buildSubjectsField, VariantTreeView, MotifCompareView)
 * are page globals from materials-view.js and the other view files. */
const MotifDetailView = (function () {
  /* ctx: {id, item, data (the item answer), analysisError, statusEl, allItems, headerEl, rerender(reason)}.
   * Appends the tab bar and panes to content, inside a `motif-detail` wrapper (up to 1200px wide). */
  function render(content, ctx) {
    const { id, item, data } = ctx;
    const bar = materialsElement("div", "analysis-tabs");
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
      const button = materialsElement("button", "", tab.label);
      button.type = "button";
      button.setAttribute("role", "tab");
      button.addEventListener("click", () => show(tab.id));
      buttons[tab.id] = button;
      bar.appendChild(button);
      panes[tab.id] = materialsElement("div", "materials-tab-pane");
      panes[tab.id].setAttribute("role", "tabpanel");
    }
    const wrapper = materialsElement("div", "motif-detail");
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
    const subjectsRow = materialsElement("div", "materials-subjects-row");
    subjectsRow.append(materialsElement("h2", "section-title", "Subjects"), subjectsField.el);
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
    const area = materialsElement("div", "motif-tab-status");
    area.setAttribute("role", "status");
    pane.appendChild(area);
    return area;
  }

  /* The one Save of the page: name and project go through the item update, an editor body that differs from the
   * current version writes a new version with the note. */
  function buildSaveRow(editor, ctx) {
    const { id, item, data } = ctx;
    const status = ctx.motifStatus;
    const row = materialsElement("div", "upload-controls button-row");
    const note = materialsElement("input");
    note.type = "text";
    note.placeholder = "What changed? (required)";
    note.size = 40;
    const save = materialsElement("button", "button-primary", "Save");
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
      if (bodyChanged) {
        const problems = MaterialsModel.motifFormProblems(values);
        editor.showProblems(problems);
        if (problems.length > 0) {
          return;
        }
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
          editor.showError(text, MaterialsModel.errorField(text));
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
        renderMaterialsDetail("motif", motifId);
      },
      send: sendMaterialsRequest,
    });
    ctx.variants = variants;
  }

  /* Versions, Derived from and Compare. */
  function buildOtherPane(pane, ctx) {
    const { id, item, data, statusEl } = ctx;
    const versionEl = materialsElement("div");
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

  return { render };
})();
