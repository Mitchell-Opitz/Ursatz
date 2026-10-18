/* The theme detail page (P19 B5): a tab bar (Theme, Structure, Used by, Other) and one pane per tab, built once and
 * shown or hidden so the form keeps what was typed when another tab is open. The pieces (buildThemeEditor,
 * buildThemeResult, themeVersionView, buildUsedByTable, buildUsesSection, buildVersionsList) are page globals from
 * materials-view.js and themes-view.js. */
const ThemeDetailView = (function () {
  /* ctx: {id, item, data (the item answer), statusEl, allItems, nameOf, rerender()}.
   * Appends the tab bar and panes to content, inside a `themes-section` wrapper. */
  function render(content, ctx) {
    const { item, data } = ctx;
    const bar = materialsElement("div", "analysis-tabs");
    bar.setAttribute("role", "tablist");
    const panes = {};
    const buttons = {};
    const mounted = {};
    const builders = {
      theme: (pane) => buildThemePane(pane, ctx),
      structure: () => {},
      "used-by": (pane) => pane.appendChild(buildUsedByTable(data.used_by, item.current_version)),
      other: (pane) => buildOtherPane(pane, ctx),
    };

    function show(tabId) {
      ThemesModel.writeThemeTab(tabId);
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
      buildStatusArea(panes[tab.id]);
    }
    const wrapper = materialsElement("div", "themes-section");
    wrapper.appendChild(bar);
    for (const tab of ThemesModel.THEME_TABS) {
      wrapper.appendChild(panes[tab.id]);
    }
    content.appendChild(wrapper);
    show(ThemesModel.readThemeTab());
  }

  /* The status area at the top of a tab: saves, errors and progress of every section of the tab show here. */
  function buildStatusArea(pane) {
    const area = materialsElement("div", "motif-tab-status");
    area.setAttribute("role", "status");
    pane.appendChild(area);
    return area;
  }

  function buildThemePane(pane, ctx) {
    const { id, item, data } = ctx;
    const editor = buildThemeEditor({
      id,
      body: data.body,
      usesStatus: data.uses_status,
      items: ctx.allItems,
      statusEl: ctx.statusEl,
      saved: () => ctx.rerender(),
    });
    pane.appendChild(editor.el);
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
