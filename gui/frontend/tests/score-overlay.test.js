/* Covers the score overlay (public/score/score-overlay.js) on the fake document: the lanes planned from the layer catalog and the layers turned on,
 * the text fitted to a width, the overlay drawn over the OSMD svg (a group per drawn item, the selected one marked, nothing drawn without a layout or
 * without the score's svg) and the playback cursor placed on a system and removed. The layers and the layout are those of the Mozart fixture and a
 * hand-built layout. Plain Node script, run through ctest. */
import ScoreLayers from "../public/score/score-layers.js";
import ScoreOverlay from "../public/score/score-overlay.js";
import { check, same, finish } from "./support/check.js";
import { installDom } from "./support/dom.js";
import { analyzerSnapshot, readJson } from "./support/fixtures.js";

const { document } = installDom();
const makeElement = document.createElement;
document.createElement = (tag) => {
  const node = makeElement(tag);
  if (tag === "canvas") {
    node.getContext = () => ({ font: "", measureText: (text) => ({ width: String(text).length * 7 }) });
  }
  return node;
};

const fixture = readJson(analyzerSnapshot("mozart-545.analysis.json"));
const catalog = ScoreLayers.buildLayerCatalog(fixture);

/* The pure parts. */
const planned = ScoreOverlay.planLanes(catalog, ["cadences", "chords"]);
check(planned.lanes.map((lane) => lane.layerId).sort().join() === "cadences,chords", "a lane is planned for each layer that is on");
check(planned.lanes.every((lane) => lane.rows >= 1), "each lane has at least one row");
check(ScoreOverlay.planLanes(catalog, []).lanes.length === 0, "no layer on, no lane");
const measure = (text) => text.length * 10;
check(ScoreOverlay.fitText("cadence", 100, measure) === "cadence", "text that fits is kept");
check(ScoreOverlay.fitText("cadence", 45, measure) === "cad…", "text that does not fit is cut with an ellipsis");
check(ScoreOverlay.fitText("cadence", 5, measure) === "", "a width that fits not one character gives nothing");

/* The overlay over the score. */
const paper = document.createElement("div");
const host = document.createElement("div");
document.body.append(paper);
paper.append(host);
const layout = {
  unit: 10,
  systems: [
    {
      index: 0, top: 0, bottom: 40, left: 10, right: 400,
      measures: [{ number: 1, start: 0, end: 8, x: 10, width: 190, entries: [{ t: 0, x: 20 }] }, { number: 2, start: 8, end: 16, x: 200, width: 190, entries: [] }],
    },
  ],
};
const overlay = ScoreOverlay.create(paper, host);
const svg = paper.querySelector("svg.ws-overlay");
check(svg !== null && svg.getAttribute("aria-label") === "Score layers", "creating the overlay adds its svg to the paper");

const cadences = catalog.find((layer) => layer.id === "cadences");
overlay.draw(layout, 0, catalog, ["cadences"], {});
check(svg.querySelectorAll("g").length === 0, "nothing is drawn while the score's svg is missing");
host.append(document.createElement("svg"));
overlay.draw(layout, 0, catalog, ["cadences"], {});
const groups = svg.querySelectorAll("g");
check(groups.length > 0 && groups.length <= cadences.items.length, "a group is drawn for each cadence that has a place on the layout");
check(groups.every((group) => group.getAttribute("data-layer") === "cadences" && group.getAttribute("data-key") !== null && group.getAttribute("role") === "button"), "each group names its layer and item, and is a button");
const key = groups[0].getAttribute("data-key");
overlay.draw(layout, 0, catalog, ["cadences"], { selectedKey: key });
check(svg.querySelectorAll("g").find((group) => group.getAttribute("data-key") === key).querySelectorAll("[stroke]").length > 0, "the selected item is drawn with a stroke");
overlay.draw(null, 0, catalog, ["cadences"], {});
check(svg.querySelectorAll("g").length === 0, "no layout clears the overlay");

/* The cursor. */
check(overlay.setCursor(null, 1) === null, "no layout, no cursor");
const spot = overlay.setCursor(layout, 4);
check(spot !== null && spot.system === 0 && spot.x > 20 && spot.x < 200, "the cursor is placed on the system and at an x inside the first measure");
check(svg.querySelectorAll("rect").length >= 2, "the cursor draws a band and a line");
overlay.hideCursor();
check(svg.querySelectorAll("rect").length === 0, "hiding the cursor removes it");
overlay.destroy();
check(paper.querySelector("svg.ws-overlay") === null, "destroying the overlay removes its svg");

finish();
