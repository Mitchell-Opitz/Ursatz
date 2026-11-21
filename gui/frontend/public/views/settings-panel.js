/* The Settings section of the theme page (R-671, spec 6.1): the taste profile in use, the settings that are not at their default with where each comes
 * from, the overrides, and the switch that makes this theme ignore the taste profile. It asks the workshop (settings.resolve) when it is opened and
 * again when the workshop says the settings changed; it changes only the theme's off list (settings.switch). Editing values is later. */

import { commandResult, queryResult } from "../core/api.js";
import { el as element } from "../core/dom.js";
import { events } from "../core/events.js";
import SettingsPanel from "../models/settings-panel.js";

/* ctx: {id} the theme's item id. Returns the closed details element. */
export function buildSettingsSection(ctx) {
  const scope = `item:${ctx.id}`;
  const section = element("details", "settings-section");
  section.appendChild(element("summary", "section-title", "Settings"));
  const body = element("div", "settings-body");
  section.appendChild(body);
  let current = 0;

  function clear() {
    while (body.firstChild) {
      body.removeChild(body.firstChild);
    }
  }

  function render(resolution) {
    clear();
    body.appendChild(element("p", "settings-taste", SettingsPanel.tasteLine(resolution)));
    const box = element("input", "settings-ignore-taste");
    box.type = "checkbox";
    box.checked = SettingsPanel.tasteIsOff(resolution);
    const label = element("label", "settings-switch");
    label.appendChild(box);
    label.appendChild(element("span", "", "Ignore my taste profile for this theme"));
    body.appendChild(label);
    box.addEventListener("change", () => switchTaste(box.checked));
    const rows = SettingsPanel.settingsRows(resolution);
    if (rows.length === 0) {
      body.appendChild(element("p", "settings-none", "No setting differs from its default."));
    }
    for (const row of rows) {
      const line = element("div", "settings-row");
      line.appendChild(element("span", "settings-key", row.key));
      line.appendChild(element("span", "settings-value", row.value));
      line.appendChild(element("span", "settings-from", row.from));
      body.appendChild(line);
    }
    for (const row of SettingsPanel.overrideRows(resolution)) {
      const line = element("div", "settings-row settings-override");
      line.appendChild(element("span", "settings-key", row.text));
      line.appendChild(element("span", "settings-from", row.from));
      body.appendChild(line);
    }
  }

  /* Asks the workshop what the settings of this theme resolve to and shows it; a late answer of an older ask is dropped. */
  async function refresh() {
    const ask = ++current;
    const result = await queryResult("settings.resolve", { item_id: ctx.id });
    if (ask !== current) {
      return;
    }
    if (!result.ok || !result.data) {
      clear();
      body.appendChild(element("p", "settings-error", "The settings could not be read."));
      return;
    }
    render(result.data);
  }

  /* Adds or removes "taste" in the item's off list, keeping the other words it holds. */
  async function switchTaste(ignore) {
    const held = await queryResult("settings.scope", { scope });
    const off = SettingsPanel.withTaste(held.ok && held.data ? held.data.off : [], ignore);
    const changed = await commandResult("settings.switch", { params: { scope, off } });
    if (changed.ok) {
      await refresh();
    }
  }

  section.addEventListener("toggle", () => {
    if (section.open) {
      refresh();
    }
  });
  events.on("settings-changed", () => {
    if (section.open) {
      refresh();
    }
  });
  section.refresh = refresh;
  return section;
}
