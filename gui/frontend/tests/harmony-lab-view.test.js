/* The Harmony Lab (views/harmony-lab-view.js) on the fake page: the palette chips come from knowledge.palette, a chip fills the focused unit, Hear posts the
 * request to composer.progression and shows the bars' numbers, Save sends item.create then item.compose, and Apply sends item.apply-harmony. Plain Node
 * script, run through ctest. */
import { renderHarmonyLabView, setHarmonyLabScore } from "../public/views/harmony-lab-view.js";
import { check } from "./support/check.js";
import { finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { byCommand, openPage } from "./support/page.js";

const palette = {
  key: "C major",
  materials: [
    { material: "omt.material.major", word: "major", chords: [{ degree: 1, numeral: "I", name: "C", quality: "major", text: "I", shared: true, color: false }, { degree: 5, numeral: "V", name: "G", quality: "major", text: "V", shared: false, color: false }] },
    { material: "omt.material.dorian", word: "dorian", chords: [{ degree: 4, numeral: "IV", name: "F", quality: "major", text: "IV:dorian", shared: false, color: true }] },
  ],
  shared: ["I"],
  color: [],
};
const heard = {
  ok: true,
  key: "C major",
  musicxml: "<score-partwise/>",
  playback: { notes: [] },
  heard: null,
  chords: [],
  measures: [{ bar: 1, tension: "1", brightness: "4" }, { bar: 2, tension: "1", brightness: "9/2" }],
  harmony: { chords: [{ bar: 1, text: "I" }, { bar: 2, text: "V" }], stages: [] },
};
const page = await openPage({
  "POST /api/workshop/query/knowledge.palette": { ok: true, ...palette },
  "POST /api/workshop/query/composer.progression": (call) => (call.body.list_schemas ? { ok: true, schemas: [], sequences: [], progression_schemas: [{ id: "omt.schema.doo-wop", name: "Doo-wop", loop: true, materials: [] }] } : heard),
  "POST /api/workshop/query/item.list": (call) => ({ ok: true, items: call.body.type === "harmony" ? [{ id: "hrm-1", name: "Sad" }] : [{ id: "thm-1", name: "Theme" }] }),
  "POST /api/workshop/query/project.list": { ok: true, projects: [{ id: "prj-1", name: "Work" }] },
  "POST /api/workshop/command": byCommand({
    "item.create": { ok: true, item: { id: "hrm-2", current_version: 1 } },
    "item.compose": { ok: true, item: { id: "hrm-2", current_version: 2 } },
    "item.apply-harmony": { ok: true, item: { id: "thm-1", current_version: 4 } },
  }),
});
page.setShell({});
globalThis.URL.createObjectURL = globalThis.URL.createObjectURL || (() => "blob:test");
globalThis.URL.revokeObjectURL = globalThis.URL.revokeObjectURL || (() => {});

const mounted = [];
setHarmonyLabScore((host, options) => {
  mounted.push(options);
  return { destroy() {} };
});

await renderHarmonyLabView();
await page.settle();

const palettePosts = page.callsTo("POST", "/api/workshop/query/knowledge.palette");
check(palettePosts.length >= 1 && /^[A-G][#b]? \w+/.test(palettePosts[0].body.key), "the palette is asked for the page's key");
const chips = page.content.querySelectorAll("button.lab-chip");
check(chips.length === 3 && chips[0].className.includes("lab-chip-shared") && chips[2].className.includes("lab-chip-color"), "the chips are the palette's chords, marked shared and color");
check(page.content.textContent.includes("Harmony Lab") && page.content.querySelectorAll(".lab-unit").length === 8, "the timeline has a unit for each of the 8 bars");

const unit = page.content.querySelectorAll(".lab-unit input")[2];
unit.dispatchEvent({ type: "focus" });
chips[1].click();
await page.settle();
check(page.content.querySelectorAll(".lab-unit input")[2].value === "V", "a chip fills the focused unit with its text");

byText(page.content, "button", "Hear")[0].click();
await page.settle();
const hears = page.callsTo("POST", "/api/workshop/query/composer.progression").filter((call) => !call.body.list_schemas);
check(hears.length === 1 && hears[0].body.palette.chords[0].text === "V" && hears[0].body.palette.chords[0].bar === 3 && hears[0].body.cadence === "none", "Hear posts the request with the typed chord");
check(mounted.length === 1 && mounted[0].subtitle === "harmony plan" && mounted[0].musicxmlUrl.length > 0 && mounted[0].playbackUrl.length > 0, "the score is mounted from the answer's MusicXML and playback");
check(page.content.textContent.includes("bar 2: tension 1, brightness 9/2"), "the bars' tension and brightness are shown under the timeline");

byText(page.content, "button", "Save")[0].click();
await page.settle();
const created = page.commands("item.create")[0];
check(created !== undefined && created.params.type === "harmony" && created.params.project_id === "prj-1" && created.params.body.request.palette.chords.length === 1, "Save creates a harmony item with the request");
const composed = page.commands("item.compose");
check(composed.length === 1 && composed[0].target === "hrm-2", "and composes it");

byText(page.content, "button", "Apply")[0].click();
await page.settle();
const applied = page.commands("item.apply-harmony")[0];
check(applied === undefined && page.content.textContent.includes("Choose a theme or a piece first."), "Apply with no target chosen sends nothing");
const targets = page.content.querySelectorAll(".lab-apply select");
check(targets[1].options.map((o) => o.value).join() === ",thm-1", "the themes are offered as targets");
targets[1].value = "thm-1";
byText(page.content, "button", "Apply")[0].click();
await page.settle();
const sent = page.commands("item.apply-harmony")[0];
check(sent !== undefined && sent.target === "thm-1" && sent.params.harmony === "hrm-2" && sent.params.version === 2 && sent.params.section === undefined, "Apply sends item.apply-harmony with the plan and its version");

finish();
