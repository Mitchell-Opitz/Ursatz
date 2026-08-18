/* The motif detail page (P18 B3): a header, a tab bar (Motif, Variants, Used by, Other) and one pane per tab. The
 * panes are built once and shown or hidden, so the editor keeps what was typed when another tab is open. The
 * Variants pane mounts the first time it is shown. The pieces (buildMotifEditor, buildMotifScore, buildAnalysisPanel,
 * buildUsedBySection, buildUsesSection, buildVersionsList, buildSubjectsField, VariantTreeView, MotifCompareView)
 * are page globals from materials-view.js and the other view files. */
const MotifDetailView = (function () {
  /* ctx: {id, item, data (the item answer), analysisError, statusEl, allItems, headerEl, rerender(reason)}.
   * Appends the tab bar and panes to content. */
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
      "used-by": (pane) => pane.appendChild(buildUsedBySection(data.used_by)),
      other: (pane) => buildOtherPane(pane, ctx),
    };

    function show(tabId) {
      MaterialsModel.writeMotifTab(tabId);
      for (const tab of MaterialsModel.MOTIF_TABS) {
        const active = tab.id === tabId;
        panes[tab.id].hidden = !active;
        buttons[tab.id].classList.toggle("active", active);
        buttons[tab.id].setAttribute("aria-selected", active ? "true" : "false");
      }
      if (!mounted[tabId]) {
        mounted[tabId] = true;
        builders[tabId](panes[tabId]);
      }
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
    content.appendChild(bar);
    for (const tab of MaterialsModel.MOTIF_TABS) {
      content.appendChild(panes[tab.id]);
    }
    show(MaterialsModel.readMotifTab());
  }

  /* The score, the editor, the analysis and the save row. */
  function buildMotifPane(pane, ctx) {
    const { id, item, data, statusEl } = ctx;
    const score = buildMotifScore(id, item, data.body);
    if (score) {
      pane.appendChild(score);
    }
    const editor = buildMotifEditor(data.body);
    pane.appendChild(editor.el);
    const subjectsField = buildSubjectsField("items", id, data.subjects);
    const subjectsRow = materialsElement("div", "materials-subjects-row");
    subjectsRow.append(materialsElement("h2", "section-title", "Subjects"), subjectsField.el);
    pane.appendChild(subjectsRow);
    pane.appendChild(
      buildAnalysisPanel(data.body, ctx.analysisError, {
        send: () => sendMaterialsRequest(MaterialsModel.buildReanalyzeRequest(id)),
        done: (reason) => ctx.rerender(reason),
      }),
    );
    pane.appendChild(
      buildMotifSaveControls(editor, statusEl, "Save as new version", true, async (values, note) => {
        const request = MaterialsModel.buildSaveMotifRequest({
          id,
          note,
          body: MaterialsModel.buildMotifBody(values, data.body),
        });
        const saved = await sendMaterialsRequest(request);
        if (saved.ok) {
          ctx.rerender(saved.data.item.analysis_error);
        }
        return saved;
      }),
    );
  }

  function buildVariantsPane(pane, ctx) {
    const { id, item, data } = ctx;
    const variants = VariantTreeView.mount(pane, {
      id,
      name: item.name,
      body: data.body,
      send: sendMaterialsRequest,
      open: (variantId) => renderMaterialsDetail("motif", variantId),
    });
    ctx.variants = variants;
  }

  /* Versions, Uses and Compare. */
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
