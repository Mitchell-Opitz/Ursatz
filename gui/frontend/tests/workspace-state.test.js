/* Confirms the pure state of the score workspace (workspace-state.js): zoom
 * steps and labels, which panel layout fits a window width, the shortcut
 * table and which events it ignores, and the saved interface state with
 * defaults and a storage that throws. Plain Node script, no dependencies.
 * Run directly: `node frontend/tests/workspace-state.test.js`. */

const WorkspaceState = require("../public/workspace-state.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Zoom. */
const { ZOOM_STEPS, nextZoom, zoomLabel, panelMode, shortcutFor, loadUiState, saveUiState } = WorkspaceState;
check(
  JSON.stringify(ZOOM_STEPS) === JSON.stringify([0.5, 0.625, 0.75, 0.875, 1, 1.25, 1.5, 1.75, 2]),
  "the zoom steps run from 50% to 200%",
);
check(nextZoom(1, 1) === 1.25 && nextZoom(1, -1) === 0.875, "zooming in and out moves one step from 100%");
check(nextZoom(2, 1) === 2, "zooming in at 200% stays at 200%");
check(nextZoom(0.5, -1) === 0.5, "zooming out at 50% stays at 50%");
check(nextZoom(1.1, 1) === 1.25 && nextZoom(1.1, -1) === 1, "a zoom between steps goes to the next step in that direction");
check(nextZoom(9, 1) === 2 && nextZoom(0.1, -1) === 0.5, "a zoom outside the range is clamped");
check(zoomLabel(1) === "100%" && zoomLabel(0.5) === "50%" && zoomLabel(1.75) === "175%", "zoom labels are percentages");

/* Panel modes at the thresholds. */
check(panelMode(1023) === "overlay", "1023 px: both panels float");
check(panelMode(1024) === "inspector-overlay", "1024 px: only the inspector floats");
check(panelMode(1279) === "inspector-overlay", "1279 px: only the inspector floats");
check(panelMode(1280) === "docked", "1280 px: both panels are docked");
check(panelMode(1920) === "docked" && panelMode(320) === "overlay", "wide and narrow windows");

/* Shortcuts. */
function press(key, extra) {
  return shortcutFor({ key, target: { tagName: "DIV" }, ...extra });
}
const actions = {
  l: "toggle-layers",
  L: "toggle-layers",
  i: "toggle-inspector",
  f: "toggle-focus",
  "+": "zoom-in",
  "=": "zoom-in",
  "-": "zoom-out",
  0: "zoom-reset",
  "?": "toggle-shortcuts",
  Escape: "escape",
  " ": "play-pause",
  Home: "stop",
  c: "follow",
  C: "follow",
  Enter: "play-claim",
};
for (const [key, action] of Object.entries(actions)) {
  const row = press(key);
  check(row !== null && row.action === action, `${key} triggers ${action}`);
}
check(press("x") === null && press("Tab") === null, "an unlisted key triggers nothing");

/* Follow: where the score scrolls to keep the playing system in view. */
const { followScrollTarget } = WorkspaceState;
const viewport = { scrollTop: 1000, height: 800, contentHeight: 5000 };
check(followScrollTarget(viewport, { top: 1200, bottom: 1350 }) === null, "a system inside the band stays put");
check(followScrollTarget(viewport, { top: 1080, bottom: 1230 }) === null, "a system top at 10% is inside the band");
check(followScrollTarget(viewport, { top: 1600, bottom: 1750 }) === null, "a system top at 75% is inside the band");
check(followScrollTarget(viewport, { top: 1601, bottom: 1751 }) === 1441, "a system just below 75% scrolls its top to 20%");
check(followScrollTarget(viewport, { top: 1079, bottom: 1229 }) === 919, "a system just above 10% scrolls its top to 20%");
check(followScrollTarget(viewport, { top: 1050, bottom: 1200 }) === 890, "a system near the top edge scrolls to 20%");
check(followScrollTarget(viewport, { top: 1500, bottom: 1850 }) === 1340, "a system cut off at the bottom scrolls to 20%");
check(followScrollTarget(viewport, { top: 400, bottom: 550 }) === 240, "a system above the viewport scrolls back to it");
check(followScrollTarget({ ...viewport, scrollTop: 500 }, { top: 100, bottom: 250 }) === 0, "the target never goes above the start");
check(followScrollTarget(viewport, { top: 4900, bottom: 5000 }) === 4200, "the target stays inside the scrollable range");
check(
  followScrollTarget({ ...viewport, scrollTop: 4200 }, { top: 4900, bottom: 5000 }) === null,
  "a target within a pixel of the scroll position gives null",
);
check(followScrollTarget(viewport, { top: 1200, bottom: 1350 }, true) === 1040, "force scrolls even inside the band");
check(followScrollTarget(viewport, { top: 1160, bottom: 1310 }, true) === null, "force at the target already gives null");
check(
  ["INPUT", "SELECT", "TEXTAREA", "input"].every((tagName) => shortcutFor({ key: "l", target: { tagName } }) === null),
  "typing in an input, select or textarea triggers nothing",
);
check(shortcutFor({ key: "l", target: { tagName: "DIV", isContentEditable: true } }) === null, "typing in an editable element triggers nothing");
check(
  press("l", { ctrlKey: true }) === null && press("l", { metaKey: true }) === null && press("l", { altKey: true }) === null,
  "Ctrl, Meta and Alt combinations trigger nothing",
);
check(press("L", { shiftKey: true }) !== null, "Shift alone does not block a shortcut");
check(shortcutFor({ key: "l" }) !== null, "an event without a target still matches");
check(shortcutFor(null) === null && shortcutFor({}) === null, "an empty event triggers nothing");
check(
  new Set(WorkspaceState.SHORTCUTS.map((row) => row.key)).size === WorkspaceState.SHORTCUTS.length &&
    WorkspaceState.SHORTCUTS.every((row) => row.key && row.label && row.action),
  "every shortcut row has a distinct key, a label and an action",
);

/* Saved interface state. */
const defaults = { layersOpen: true, inspectorOpen: true, zoom: 1, mode: "score", volume: 80, reverb: 20 };
check(JSON.stringify(loadUiState(null)) === JSON.stringify(defaults), "no storage gives the defaults");

function fakeStorage() {
  const data = new Map();
  return {
    getItem: (key) => (data.has(key) ? data.get(key) : null),
    setItem: (key, value) => data.set(key, String(value)),
  };
}
const storage = fakeStorage();
check(JSON.stringify(loadUiState(storage)) === JSON.stringify(defaults), "an empty storage gives the defaults");
saveUiState(storage, { layersOpen: false, inspectorOpen: true, zoom: 1.5, mode: "data" });
check(
  JSON.stringify(loadUiState(storage)) === JSON.stringify({ layersOpen: false, inspectorOpen: true, zoom: 1.5, mode: "data", volume: 80, reverb: 20 }),
  "a saved state is loaded back",
);
check(storage.getItem("ursatz.workspace.v1") !== null, "the state is stored under ursatz.workspace.v1");

storage.setItem("ursatz.workspace.v1", "not json");
check(JSON.stringify(loadUiState(storage)) === JSON.stringify(defaults), "unreadable stored text gives the defaults");
storage.setItem("ursatz.workspace.v1", JSON.stringify({ layersOpen: "no", zoom: 3, mode: "tree", inspectorOpen: false }));
check(
  JSON.stringify(loadUiState(storage)) === JSON.stringify({ layersOpen: true, inspectorOpen: false, zoom: 1, mode: "score", volume: 80, reverb: 20 }),
  "a field of the wrong type or out of range falls back to its default",
);
storage.setItem("ursatz.workspace.v1", "[1,2]");
check(JSON.stringify(loadUiState(storage)) === JSON.stringify(defaults), "a stored array gives the defaults");

const throwing = {
  getItem() {
    throw new Error("blocked");
  },
  setItem() {
    throw new Error("blocked");
  },
};
check(JSON.stringify(loadUiState(throwing)) === JSON.stringify(defaults), "a storage that throws on read gives the defaults");
let threw = false;
try {
  saveUiState(throwing, defaults);
  saveUiState(null, defaults);
} catch (err) {
  threw = true;
}
check(!threw, "saving to a storage that throws or is missing does not throw");

process.exit(failures === 0 ? 0 : 1);
