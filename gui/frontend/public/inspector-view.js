/* The Inspector panel's content (R-099): tabs, and in the Claim tab the
 * evidence of the selected item from InspectorModel.
 *
 *   const view = InspectorView.create(containerEl, {
 *     select(item, options),       select another item (options: reveal)
 *     setSecondary(secondary),     {tints: [{start, end}], noteIds} or null
 *     itemForClaim(claim),         the item showing a raw claim, or null
 *     layerOf(item),               the catalog layer of an item
 *     reveal(position),            scroll the score to a piece position
 *   });
 *   view.show(selection);          {item, layer, others} or null
 *
 * DOM only; the facts come from InspectorModel. */
(function (root) {
  const EMPTY_TEXT = "Nothing selected. Click a colored mark or note on the score, or pick a claim from the List.";

  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) {
      node.className = className;
    }
    if (text !== undefined) {
      node.textContent = text;
    }
    return node;
  }

  function button(className, text, onClick) {
    const node = el("button", className, text);
    node.type = "button";
    node.addEventListener("click", onClick);
    return node;
  }

  function clamp(value, low, high) {
    return Math.min(high, Math.max(low, value));
  }

  function create(container, api) {
    const tabBar = el("div", "ws-insp-tabs");
    tabBar.setAttribute("role", "tablist");
    const panels = el("div", "ws-insp-panels");
    container.append(tabBar, panels);

    const tabs = new Map();

    /* A tab and its panel; the first tab added is shown. */
    function addTab(id, label, panel) {
      const tab = button("ws-insp-tab", label, () => showTab(id));
      tab.id = `ws-insp-tab-${id}`;
      tab.setAttribute("role", "tab");
      panel.setAttribute("role", "tabpanel");
      panel.setAttribute("aria-labelledby", tab.id);
      tabBar.appendChild(tab);
      panels.appendChild(panel);
      tabs.set(id, { tab, panel });
      showTab(tabs.size === 1 ? id : currentTab);
    }

    let currentTab = null;

    function showTab(id) {
      currentTab = id;
      for (const [tabId, { tab, panel }] of tabs) {
        const on = tabId === id;
        tab.setAttribute("aria-selected", String(on));
        tab.tabIndex = on ? 0 : -1;
        panel.hidden = !on;
      }
    }

    tabBar.addEventListener("keydown", (event) => {
      if (event.key !== "ArrowRight" && event.key !== "ArrowLeft") {
        return;
      }
      const ids = [...tabs.keys()];
      const next = ids[(ids.indexOf(currentTab) + (event.key === "ArrowRight" ? 1 : ids.length - 1)) % ids.length];
      showTab(next);
      tabs.get(next).tab.focus();
      event.preventDefault();
    });

    /* Claim tab. */
    const claimPanel = el("div", "ws-insp-claim");
    addTab("claim", "Claim", claimPanel);

    /* The secondary highlight in force: an id naming the row that set it. */
    let secondaryId = null;
    let toggles = [];

    function syncToggles() {
      for (const { node, id } of toggles) {
        node.setAttribute("aria-pressed", String(id === secondaryId));
      }
    }

    function setSecondary(id, secondary, position) {
      if (secondaryId === id) {
        secondaryId = null;
        api.setSecondary(null);
      } else {
        secondaryId = id;
        api.setSecondary(secondary);
        if (position !== null && position !== undefined) {
          api.reveal(position);
        }
      }
      syncToggles();
    }

    function toggleButton(label, id, secondary, position) {
      const node = button("ws-show-btn", label, () => setSecondary(id, secondary, position));
      node.setAttribute("aria-pressed", "false");
      toggles.push({ node, id });
      return node;
    }

    function percentText(share) {
      return share === null ? "" : `${share}%`;
    }

    function shareBar(share, color) {
      const bar = el("div", "ws-bar");
      bar.setAttribute("role", "img");
      bar.setAttribute("aria-label", share === null ? "no share" : `${share} percent`);
      const fill = el("i");
      fill.style.width = `${share === null ? 0 : clamp(share, 0, 100)}%`;
      fill.style.background = color;
      bar.appendChild(fill);
      return bar;
    }

    function renderReadings(model, selection) {
      const { primary, alternative } = model.readings;
      const section = el("section", "ws-insp-section");
      section.appendChild(el("h3", "ws-insp-heading", "Readings"));
      const color = selection.layer.color;

      const main = el("div", "ws-reading");
      const name = el("span", "ws-reading-name");
      name.append(el("b", "", primary.abbr), document.createTextNode(` ${primary.type === null ? "" : primary.type}`));
      main.append(name, el("span", "ws-reading-share", percentText(primary.share)), shareBar(primary.share, color));
      section.appendChild(main);

      if (alternative) {
        const id = "alternative";
        const alt = el("div", "ws-reading ws-reading-alt");
        const altName = el("span", "ws-reading-name");
        altName.append(el("b", "", alternative.abbr), document.createTextNode(` ${alternative.reading === null ? "" : alternative.reading}`));
        const where = alternative.measure === null ? "" : `m${alternative.measure}${alternative.beat === null ? "" : ` b${alternative.beat}`}`;
        alt.append(altName, el("span", "ws-reading-share", percentText(alternative.share)), shareBar(alternative.share, color));
        if (where) {
          alt.appendChild(el("span", "ws-reading-where", `Arrival ${where}`));
        }
        section.appendChild(alt);
        if (alternative.noteIds.length > 0) {
          section.appendChild(toggleButton("Show its arrival notes", id, { tints: [], noteIds: alternative.noteIds }, null));
        }
      }
      return section;
    }

    function evidenceList(rows) {
      const list = el("dl", "ws-evidence");
      for (const row of rows) {
        list.appendChild(el("dt", "", row.label));
        const detail = el("dd");
        if (row.kind === "value") {
          detail.textContent = row.text;
        } else if (row.kind === "span") {
          if (row.location !== "-") {
            detail.appendChild(el("span", "", row.location));
          }
          detail.appendChild(
            toggleButton("Show", `span:${row.key}`, { tints: [{ start: row.start, end: row.end }], noteIds: [] }, row.start),
          );
        } else if (row.kind === "claims") {
          detail.appendChild(subClaims(row));
        } else {
          detail.appendChild(evidenceList(row.rows));
        }
        list.appendChild(detail);
      }
      return list;
    }

    /* A list of sub-claims (subphrases, occurrences, stages): collapsed to
     * its count; a row selects its item when a layer shows it, else tints
     * its span. */
    function subClaims(row) {
      const wrap = el("div", "ws-subclaims");
      const list = el("ul", "ws-sub-list");
      list.hidden = true;
      const toggle = button("ws-sub-toggle", `${row.count} ${row.count === 1 ? "item" : "items"}`, () => {
        list.hidden = !list.hidden;
        toggle.setAttribute("aria-expanded", String(!list.hidden));
      });
      toggle.setAttribute("aria-expanded", "false");
      row.items.forEach((sub, n) => {
        const li = el("li");
        const id = `sub:${row.key}:${n}`;
        const node = button("ws-sub-item", "", () => {
          const item = api.itemForClaim(sub.claim);
          if (item) {
            api.select(item, { reveal: true });
          } else if (sub.start !== null && sub.end !== null) {
            setSecondary(id, { tints: [{ start: sub.start, end: sub.end }], noteIds: [] }, sub.start);
          }
        });
        node.append(el("span", "", sub.text), el("span", "ws-dim", sub.location));
        if (!api.itemForClaim(sub.claim)) {
          node.setAttribute("aria-pressed", "false");
          toggles.push({ node, id });
        }
        li.appendChild(node);
        list.appendChild(li);
      });
      wrap.append(toggle, list);
      return wrap;
    }

    function copyJson(raw, node) {
      const done = (text) => {
        node.textContent = text;
        setTimeout(() => {
          node.textContent = "Copy JSON";
        }, 1500);
      };
      try {
        navigator.clipboard.writeText(raw).then(() => done("Copied"), () => done("Copy failed"));
      } catch (err) {
        done("Copy failed");
      }
    }

    function renderClaim(selection) {
      claimPanel.replaceChildren();
      toggles = [];
      secondaryId = null;
      if (!selection) {
        claimPanel.appendChild(el("p", "ws-empty", EMPTY_TEXT));
        return;
      }
      const { item, layer, others } = selection;
      const model = InspectorModel.buildInspectorModel(item, layer);
      const head = model.header;

      const header = el("header", "ws-insp-head");
      const kicker = el("div", "ws-insp-kicker");
      const dot = el("span", "ws-dot");
      dot.style.background = head.color;
      kicker.append(dot, el("span", "", head.layerLabel));
      const title = el("h3", "ws-insp-title", head.text);
      header.append(kicker, title);
      if (head.label) {
        header.appendChild(el("div", "ws-dim", head.label));
      }
      const where = [head.location, head.measures === null ? "" : `${head.measures} ${head.measures === 1 ? "measure" : "measures"}`];
      header.appendChild(el("div", "ws-insp-where", where.filter(Boolean).join(" · ")));
      claimPanel.appendChild(header);

      const actions = el("div", "ws-insp-actions");
      const previous = InspectorModel.neighborItem(layer.items, item.key, -1);
      const next = InspectorModel.neighborItem(layer.items, item.key, 1);
      const previousButton = button("ws-btn", "Previous", () => api.select(previous, { reveal: true }));
      previousButton.disabled = !previous;
      previousButton.title = "Previous in this layer ([)";
      const nextButton = button("ws-btn", "Next", () => api.select(next, { reveal: true }));
      nextButton.disabled = !next;
      nextButton.title = "Next in this layer (])";
      const copy = button("ws-btn", "Copy JSON", () => copyJson(model.raw, copy));
      actions.append(previousButton, nextButton, copy);
      claimPanel.appendChild(actions);

      if (others && others.length > 0) {
        const also = el("p", "ws-also");
        also.appendChild(document.createTextNode("Also here: "));
        others.forEach((other, n) => {
          if (n > 0) {
            also.appendChild(document.createTextNode(", "));
          }
          const otherLayer = api.layerOf(other);
          also.appendChild(button("ws-link", `${otherLayer ? otherLayer.label : ""} ${other.text}`.trim(), () => api.select(other)));
        });
        claimPanel.appendChild(also);
      }

      if (model.readings) {
        claimPanel.appendChild(renderReadings(model, selection));
      }

      if (model.evidence.length > 0) {
        const section = el("section", "ws-insp-section");
        section.appendChild(el("h3", "ws-insp-heading", "Evidence"));
        section.appendChild(evidenceList(model.evidence));
        claimPanel.appendChild(section);
      }

      if (model.scores.length > 0) {
        const section = el("section", "ws-insp-section ws-scores");
        section.appendChild(el("h3", "ws-insp-heading", "Scores"));
        const list = el("dl", "ws-evidence");
        for (const score of model.scores) {
          list.append(el("dt", "", score.name), el("dd", "", score.text));
        }
        section.appendChild(list);
        claimPanel.appendChild(section);
      }

      const raw = el("details", "ws-raw");
      raw.appendChild(el("summary", "", "Raw fields"));
      raw.appendChild(el("pre", "json-block", model.raw));
      claimPanel.appendChild(raw);

      const claim = item.claim || {};
      const footer = el("footer", "ws-insp-foot ws-dim");
      footer.textContent = [claim.framework, item.key].filter(Boolean).join(" · ");
      claimPanel.appendChild(footer);
    }

    renderClaim(null);

    function show(selection) {
      renderClaim(selection);
      if (selection) {
        showTab("claim");
      }
    }

    return { show, addTab, showTab, tab: () => currentTab };
  }

  root.InspectorView = { create };
})(typeof window !== "undefined" ? window : globalThis);
