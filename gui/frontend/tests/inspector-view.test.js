/* The Inspector panel (score/inspector-view.js) on the fake page and the Analyzer's Mozart K. 545 snapshot: it starts on the Claim tab with the empty
 * message; selecting an item of a layer shows its header, its Previous, Next and Play buttons, which go to the callbacks the score gives; nothing selected
 * shows the empty message again. Plain Node script, run through ctest. */
import InspectorView from "../public/score/inspector-view.js";
import ScoreLayers from "../public/score/score-layers.js";
import { analyzerSnapshot, readJson } from "./support/fixtures.js";
import { check, finish } from "./support/check.js";
import { byText, installDom } from "./support/dom.js";

const { document } = installDom();
const doc = readJson(analyzerSnapshot("mozart-545.analysis.json"));
const catalog = ScoreLayers.buildLayerCatalog(JSON.parse(JSON.stringify(doc)));
const cadences = catalog.find((l) => l.id === "cadences");
const items = cadences.items;

/* The callbacks the score gives the panel (the ten of the header): the panel asks, the test keeps what it asked for. */
const asked = [];
let selected = null;
const api = {
  select: (item, options) => asked.push(["select", item.key, options]),
  setSecondary: (secondary) => asked.push(["setSecondary", secondary]),
  itemForClaim: () => null,
  layerOf: () => cadences,
  reveal: (position) => asked.push(["reveal", position]),
  anyLayerOn: () => true,
  listItems: () => items,
  selectedKey: () => (selected ? selected.key : null),
  canPlay: () => true,
  play: (item) => asked.push(["play", item.key]),
};
const container = document.createElement("div");
document.body.appendChild(container);
const view = InspectorView.create(container, api);

/* The panel starts on the Claim tab with the empty message. */
check(view.tab() === "claim", "the Claim tab is shown first");
check(container.querySelector(".ws-insp-claim .ws-empty").textContent.startsWith("Nothing selected."), "with nothing selected the panel says so");
check(container.querySelectorAll(".ws-insp-tab").length >= 1 && container.querySelector(".ws-insp-tab").textContent === "Claim", "the tab bar has the Claim tab");

/* A selected item shows its claim: the header names the layer, the title is the item's text. */
selected = items[1];
view.show({ item: items[1], layer: cadences, others: [] });
check(container.querySelector(".ws-insp-claim .ws-empty") === null, "the empty message is gone once an item is selected");
check(container.querySelector(".ws-insp-title").textContent === items[1].text, "the title is the item's text");
check(container.querySelector(".ws-insp-kicker").textContent.includes("Cadences"), "the header names the layer");
const play = byText(container, "button", "Play")[0];
check(play && play.disabled === false, "Play is on when playback is ready");
play.click();
check(asked.some((call) => call[0] === "play" && call[1] === items[1].key), "Play asks the score to play the item");
const next = byText(container, "button", "Next")[0];
check(next && next.disabled === false, "Next is on for an item with one after it");
next.click();
check(asked.some((call) => call[0] === "select" && call[1] === items[2].key), "Next selects the item after it");
const previous = byText(container, "button", "Previous")[0];
previous.click();
check(asked.some((call) => call[0] === "select" && call[1] === items[0].key), "Previous selects the item before it");

/* Nothing selected again: the empty message returns. */
view.show(null);
check(container.querySelector(".ws-insp-claim .ws-empty") !== null, "showing nothing brings the empty message back");

finish();
