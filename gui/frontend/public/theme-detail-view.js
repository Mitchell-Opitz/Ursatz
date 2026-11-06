/* The theme detail page (P19 B5): a tab bar (Theme, Structure, Used by, Other) and one pane per tab, built once and
 * shown or hidden so the form keeps what was typed when another tab is open. The pieces (buildThemeEditor,
 * buildThemeResult, themeVersionView, buildUsedByTable, buildUsesSection, buildVersionsList) are page globals from
 * materials-view.js and themes-view.js. */
const ThemeDetailView = (function () {
  /* ctx: {id, item, data (the item answer), statusEl, allItems, nameOf, rerender()}.
   * Appends the tab bar and panes to content, inside a `themes-section` wrapper. */
  function render(content, ctx) {
    const { id, item, data } = ctx;
    const bar = materialsElement("div", "analysis-tabs");
    bar.setAttribute("role", "tablist");
    const panes = {};
    const buttons = {};
    const mounted = {};
    /* The page status line, mounted once between the Save row and the panes: a message shows on every tab. */
    const statusLine = buildStatusArea();
    ctx.statusLine = statusLine;
    ctx.status = MaterialsModel.motifTabStatus(statusLine);
    const builders = {
      theme: (pane) => buildThemePane(pane, ctx),
      structure: (pane) => buildStructurePane(pane, ctx),
      "used-by": (pane) => pane.appendChild(buildUsedByTable(data.used_by, item.current_version, "theme")),
      other: (pane) => buildOtherPane(pane, ctx),
    };

    let current = null;
    ctx.openTab = () => current;
    function show(tabId) {
      ThemesModel.writeThemeTab(tabId);
      current = tabId;
      for (const tab of ThemesModel.THEME_TABS) {
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

    for (const tab of ThemesModel.THEME_TABS) {
      const button = materialsElement("button", "", tab.label);
      button.type = "button";
      button.setAttribute("role", "tab");
      button.addEventListener("click", () => show(tab.id));
      buttons[tab.id] = button;
      bar.appendChild(button);
      panes[tab.id] = materialsElement("div", "materials-tab-pane");
      panes[tab.id].setAttribute("role", "tabpanel");
    }
    const wrapper = materialsElement("div", "themes-section");
    wrapper.appendChild(buildMaterialExport(id, item.current_version, item.name));
    wrapper.appendChild(bar);
    wrapper.append(editorOf(ctx).sections.result, statusLine);
    for (const tab of ThemesModel.THEME_TABS) {
      wrapper.appendChild(panes[tab.id]);
    }
    content.appendChild(wrapper);
    show(ThemesModel.readThemeTab());
  }

  /* The status line of the page: saves, errors and progress of every section show here, whatever tab is open. */
  function buildStatusArea() {
    const area = materialsElement("div", "motif-tab-status");
    area.setAttribute("role", "status");
    area.style.whiteSpace = "pre-wrap";
    return area;
  }

  /* The editor is built when the page opens; the Theme and Structure tabs mount its sections. */
  function editorOf(ctx) {
    if (!ctx.editor) {
      ctx.editor = buildThemeEditor({
        id: ctx.id,
        item: ctx.item,
        body: ctx.data.body,
        usesStatus: ctx.data.uses_status,
        items: ctx.allItems,
        status: ctx.status,
        openTab: () => ctx.openTab(),
        statusText: () => ctx.statusLine.textContent,
        nameInput: ctx.nameInput,
        projectSelect: ctx.projectSelect,
        saved: () => ctx.rerender(),
      });
    }
    return ctx.editor;
  }

  /* Subjects, the theme fields, Advanced, then the composed theme. The Save row sits above the tabs. */
  function buildThemePane(pane, ctx) {
    const { id, item, data } = ctx;
    const sections = editorOf(ctx).sections;
    pane.appendChild(sections.notices);
    const subjectsField = buildSubjectsField("items", id, data.subjects);
    const subjectsRow = materialsElement("div", "materials-subjects-row");
    subjectsRow.append(materialsElement("h2", "section-title", "Subjects"), subjectsField.el);
    pane.append(subjectsRow, sections.theme, sections.advanced);
    const resultEl = buildThemeResult({
      id,
      version: item.current_version,
      theme: data.body.theme,
      nameOf: ctx.nameOf,
      title: item.name,
    });
    if (resultEl) {
      pane.appendChild(resultEl);
    }
  }

  function buildStructurePane(pane, ctx) {
    const sections = editorOf(ctx).sections;
    pane.append(sections.structure, sections.slots);
  }

  /* Versions with their notes and Derived from (the motifs the theme uses, with update notices). */
  function buildOtherPane(pane, ctx) {
    const { id, item, data, statusEl } = ctx;
    const versionEl = materialsElement("div");
    const openVersion = (number, body) => themeVersionView(versionEl, id, number, body, ctx.nameOf, item.name);
    pane.appendChild(buildVersionsList(item, versionEl, statusEl, openVersion));
    pane.appendChild(versionEl);
    pane.appendChild(
      buildUsesSection({
        id,
        type: "theme",
        body: data.body,
        statuses: data.uses_status,
        allItems: ctx.allItems,
        statusEl,
      }),
    );
  }

  return { render };
})();
