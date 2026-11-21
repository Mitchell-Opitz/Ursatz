/* Covers the Settings section of the theme page (public/models/settings-panel.js and views/settings-panel.js, R-671): the model words the answer of
 * settings.resolve (source labels, values, overrides, the taste line), the view asks settings.resolve when it is opened, and ticking the box sends
 * settings.switch with "taste" added to the item's off list (its other words kept) and asks again. Plain Node script, run through ctest. */
import SettingsPanel from "../public/models/settings-panel.js";
import { buildSettingsSection } from "../public/views/settings-panel.js";
import { byCommand, openPage } from "./support/page.js";
import { check, finish, same } from "./support/check.js";

const eq = (actual, expected, what) => check(same(actual, expected), what);

/* the model */
const resolution = {
  taste: "none",
  off: [],
  values: [
    { key: "melody.keep", value: 3, from: "item:thm-1" },
    { key: "melody.nct_types", value: ["passing", "neighbor"], from: "taste:none" },
    { key: "melody.plain_shaping", value: true, from: "taste:all" },
    { key: "motif.avoid_repeats", value: false, from: "project:prj-1" },
    { key: "development.basic_as_written", value: true, from: "style:sty-1" },
    { key: "melody.leaps", value: 2, from: "request" },
    { key: "melody.register", value: 1, from: "explore" },
    { key: "melody.density", value: 1, from: "default" },
  ],
  overrides: [
    { rule: "leaps", action: "reweight", weight: 9, from: "item:thm-1" },
    { rule: "parallel-fifths", action: "waive", from: "project:prj-1" },
  ],
};
eq(
  SettingsPanel.settingsRows(resolution).map((row) => [row.key, row.value, row.from]),
  [
    ["melody.keep", "3", "this theme"],
    ["melody.nct_types", "passing, neighbor", "taste profile none"],
    ["melody.plain_shaping", "on", "taste profile all"],
    ["motif.avoid_repeats", "off", "project"],
    ["development.basic_as_written", "on", "style"],
    ["melody.leaps", "2", "this request"],
    ["melody.register", "1", "explore"],
    ["melody.density", "1", "default"],
  ],
  "the rows word each value and each source",
);
eq(
  SettingsPanel.overrideRows(resolution),
  [
    { text: "leaps reweight 9", from: "this theme" },
    { text: "parallel-fifths waive", from: "project" },
  ],
  "the overrides read as the rule, the action and a weight, with their source",
);
check(SettingsPanel.tasteLine(resolution) === "Taste profile: none", "the taste line names the profile");
check(SettingsPanel.tasteLine({ taste: "none", off: ["style", "taste"] }) === "Taste profile: off for this theme", "or says it is off for this theme");
eq(SettingsPanel.withTaste(["project"], true), ["taste", "project"], "taste is added in front of the other words");
eq(SettingsPanel.withTaste(["taste", "style"], false), ["style"], "and taken out leaving the others");
eq(SettingsPanel.settingsRows(null), [], "no resolution has no rows");

/* the view */
(async () => {
  let off = ["style"];
  const page = await openPage({
    "POST /api/workshop/query/settings.resolve": () => ({
      status: 200,
      body: { ok: true, taste: "none", off: off.includes("taste") ? off : [], values: [{ key: "melody.keep", value: 3, from: "item:thm-1" }], overrides: [], document: null },
    }),
    "POST /api/workshop/query/settings.scope": () => ({ status: 200, body: { ok: true, scope: "item:thm-1", values: {}, off } }),
    "POST /api/workshop/command": byCommand({
      "settings.switch": (command) => {
        off = command.params.off;
        return { status: 200, body: { ok: true, scope: command.params.scope, values: {}, off } };
      },
    }),
  });
  const section = buildSettingsSection({ id: "thm-1" });
  page.content.appendChild(section);
  check(section.tagName === "DETAILS" && !section.open, "the section is a closed details");
  check(page.callsTo("POST", "/api/workshop/query/settings.resolve").length === 0, "nothing is asked until it is opened");
  section.open = true;
  section.dispatchEvent({ type: "toggle" });
  await page.settle();
  const asked = page.callsTo("POST", "/api/workshop/query/settings.resolve");
  check(asked.length === 1 && asked[0].body.item_id === "thm-1", "opening it sends settings.resolve for the theme");
  const text = section.textContent;
  check(text.includes("Taste profile: none") && text.includes("melody.keep") && text.includes("this theme"), "it shows the taste line and the settings with their source");
  const box = section.querySelector("input");
  check(box && box.type === "checkbox" && box.checked === false, "the checkbox is not ticked");
  box.click();
  await page.settle();
  const sent = page.commands("settings.switch");
  check(sent.length === 1 && sent[0].params.scope === "item:thm-1" && same(sent[0].params.off, ["taste", "style"]), "ticking it sends settings.switch with taste added and the other word kept");
  check(page.callsTo("POST", "/api/workshop/query/settings.resolve").length === 2, "and it resolves again");
  check(section.textContent.includes("Taste profile: off for this theme"), "the taste line says it is off for this theme");
  finish();
})();
