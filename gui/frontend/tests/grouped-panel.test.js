/* Covers the grouped panel the Tags and Subjects panels are built from (views/grouped-panel.js): the row of a group, the note when the groups cannot be
 * read, a group with no rows, the open groups remembered in the page's storage and the fallback when that storage holds bad JSON. Plain Node script. */
import Panel from "../public/views/grouped-panel.js";
import { check, same, finish } from "./support/check.js";
import { installDom } from "./support/dom.js";

const { document } = installDom();
let store = {};
globalThis.localStorage = { getItem: (key) => (key in store ? store[key] : null), setItem: (key, value) => { store[key] = value; } };

/* The row of a group: an item with a button holding the label and the count; the active row says so; a click calls the handler. */
let clicks = 0;
const row = Panel.buildPanelRow("etude", 3, () => { clicks++; }, true);
const button = row.querySelector("button.tags-panel-row");
check(row.tag === "li" && row.className === "tags-panel-item", "a row is a list item");
check(button.getAttribute("aria-current") === "true", "the active row says it is current");
check(button.querySelector(".tags-panel-label").textContent === "etude" && button.querySelector(".tags-panel-count").textContent === "3", "the row shows its label and its count");
button.click();
check(clicks === 1, "a click calls the handler");
check(Panel.buildPanelRow("a", 1, () => {}, false).querySelector("button").getAttribute("aria-current") === null, "a row that is not active has no aria-current");

const kinds = [{ kind: "tag", title: "Tags" }, { kind: "composer", title: "Composers" }];
function host() {
  const section = document.createElement("section");
  document.body.appendChild(section);
  return section;
}
const config = (groups, extra) => ({
  openKey: "panel-open",
  kinds,
  unavailable: "Groups are unavailable",
  fetchGroups: async () => groups,
  rows: (all, kind) => all[kind] || [],
  buildRow: (r) => Panel.buildPanelRow(r.label, r.count, () => {}, false),
  ...(extra || {}),
});

/* Groups that cannot be read: the note, and loaded() says no. */
let section = host();
let panel = Panel.mount(section, config(null));
await panel.refresh();
check(section.querySelector(".tags-panel-note").textContent === "Groups are unavailable" && panel.loaded() === false, "groups that resolve to null show the note and are not loaded");
panel = Panel.mount(host(), config(null, { fetchGroups: async () => { throw new Error("down"); } }));
await panel.refresh();
check(panel.loaded() === false, "groups that reject are not loaded either");

/* A group with no rows says so; a group with rows lists them. */
section = host();
panel = Panel.mount(section, config({ tag: [{ label: "etude", count: 5 }], composer: [] }));
await panel.refresh();
check(panel.loaded() === true && section.querySelectorAll("details").length === 2, "loaded groups are one details each");
const details = section.querySelectorAll("details");
check(details[0].querySelectorAll("li").length === 1 && details[0].querySelector("summary").textContent === "Tags", "a group with a row lists it under its title");
check(details[1].querySelector(".tags-panel-note").textContent === "None yet", "a group with no rows says None yet");

/* Every group is open to begin with; a toggle writes the open groups to the page's storage. */
check(details[0].open === true && details[1].open === true, "with nothing stored every group is open");
details[1].open = false;
details[1].dispatchEvent({ type: "toggle" });
check(same(JSON.parse(store["panel-open"]), ["tag"]), "closing a group writes the open groups to the storage");
details[1].open = true;
details[1].dispatchEvent({ type: "toggle" });
check(same(JSON.parse(store["panel-open"]).sort(), ["composer", "tag"]), "opening it writes it back");

/* What the storage holds is read when the panel is mounted; bad JSON opens every group. */
store = { "panel-open": JSON.stringify(["composer"]) };
section = host();
panel = Panel.mount(section, config({ tag: [], composer: [] }));
await panel.refresh();
check(section.querySelectorAll("details")[0].open === false && section.querySelectorAll("details")[1].open === true, "the stored open groups are the ones open");
store = { "panel-open": "{not json" };
section = host();
panel = Panel.mount(section, config({ tag: [], composer: [] }));
await panel.refresh();
check(section.querySelectorAll("details").every((d) => d.open === true), "bad JSON in the storage opens every group");

finish();
