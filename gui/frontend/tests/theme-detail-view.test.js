/* The theme detail page (views/theme-detail-view.js) on the fake page: a tab bar of Theme, Structure, Used by and Other, one pane each built when it is
 * first shown; the Theme pane holds the theme's fields, the Structure pane its form, the Other pane its versions. Plain Node script, run through ctest
 * (it reads the vocabulary the build generates). */
import MaterialsModel from "../public/models/materials-model.js";
import ThemesModel from "../public/models/themes-model.js";
import ThemeDetailView from "../public/views/theme-detail-view.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { openPage } from "./support/page.js";

const page = await openPage({});
const store = {};
globalThis.localStorage = { getItem: (key) => (key in store ? store[key] : null), setItem: (key, value) => { store[key] = value; } };

const state = ThemesModel.emptyState("mat-1");
const item = {
  id: "mat-1",
  type: "theme",
  name: "Sentence",
  project_id: "prj-1",
  current_version: 2,
  versions: [{ number: 1, created_at: "2026-11-01T10:00:00Z", note: "first" }, { number: 2, created_at: "2026-11-02T10:00:00Z", note: "second" }],
};
const statusEl = page.document.createElement("div");
const nameInput = page.document.createElement("input");
nameInput.value = "Sentence";
const projectSelect = page.document.createElement("select");
ThemeDetailView.render(page.content, {
  id: "mat-1",
  item,
  data: { item, body: ThemesModel.bodyFromForm(state), uses_status: [], used_by: [], subjects: [] },
  statusEl,
  allItems: [],
  nameOf: (id) => id,
  nameInput,
  projectSelect,
  rerender: () => {},
});

/* The tab bar: the four tabs in order, the first (or the one remembered) open. */
const bar = page.content.querySelector(".analysis-tabs");
const labels = bar.querySelectorAll("button").map((b) => b.textContent);
check(labels.join() === "Theme,Structure,Used by,Other", "the tab bar has the four tabs");
const panes = page.content.querySelectorAll(".materials-tab-pane");
check(panes.length === 4 && panes[0].hidden === false && panes.slice(1).every((p) => p.hidden === true), "the Theme tab is open and the others are hidden");
check(byText(bar, "button", "Theme")[0].getAttribute("aria-selected") === "true", "the open tab says it is selected");

/* The Theme pane: the subjects and the theme's own fields (mode, meter, tempo). */
check(panes[0].textContent.includes("Subjects") && panes[0].querySelectorAll("select").length > 0, "the Theme pane shows Subjects and the theme's fields");

/* The Structure pane is built when its tab is first opened; it holds the form. */
check(panes[1].children.length === 0, "the Structure pane is empty until its tab is opened");
byText(bar, "button", "Structure")[0].click();
check(panes[1].hidden === false && panes[0].hidden === true, "opening Structure shows it and hides Theme");
check(panes[1].textContent.includes("Form") && panes[1].querySelectorAll("select").length >= 6, "the Structure pane holds the form's selects");
check(store["ursatz.theme.tab"] === "structure", "the open tab is remembered");

/* The Used by pane says that nothing uses the theme; the Other pane lists the versions. */
byText(bar, "button", "Used by")[0].click();
check(panes[2].textContent.includes("Nothing uses this theme yet."), "the Used by pane says nothing uses the theme");
byText(bar, "button", "Other")[0].click();
check(panes[3].textContent.includes("Versions") && panes[3].textContent.includes("v2 (current)") && panes[3].textContent.includes("v1"), "the Other pane lists the versions");
check(MaterialsModel.versionRows(item).length === 2, "and they are the item's two versions");

finish();
