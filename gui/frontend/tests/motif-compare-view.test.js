/* The Compare section of the motif detail (views/motif-compare-view.js) on the fake page: it shows its title, the project picker from the projects and
 * the Compare with button; with no other motif to pick the button is off and a click sends nothing; with one, Compare asks for the recognition and shows
 * what it found. Plain Node script, run through ctest (it reads the vocabulary the build generates). */
import MotifCompareView from "../public/views/motif-compare-view.js";
import MaterialsModel from "../public/models/materials-model.js";
import { check, finish } from "./support/check.js";
import { byText } from "./support/dom.js";
import { openPage } from "./support/page.js";

const page = await openPage({});
const projects = [{ id: "prj-1", name: "Work" }, { id: "prj-2", name: "Play" }];
const sent = [];
let linked = 0;
const make = (items, answer) =>
  MotifCompareView.mount(page.content, {
    id: "mat-1",
    version: 2,
    projectId: "prj-1",
    projects,
    items,
    send: async (request) => {
      sent.push(request);
      return answer || { ok: true, status: 200, data: { ok: true, found: false } };
    },
    linked: () => linked++,
  });

/* The section: its title, the search box, a project select from the projects and the Compare with button. */
const alone = make([{ id: "mat-1", type: "motif", name: "This", project_id: "prj-1", current_version: 2 }]);
const text = alone.el.textContent;
check(text.includes("Compare") && byText(alone.el, "button", "Compare with").length === 1, "the section shows Compare and the Compare with button");
const projectSelect = alone.el.querySelectorAll("select")[0];
check(
  projectSelect.options.map((o) => o.textContent).join() === MaterialsModel.projectOptions(projects).map((o) => o.label).join() && projectSelect.value === "prj-1",
  "the project select lists the project options and starts on the motif's project",
);

/* With no other motif there is nothing to compare with: the button is off, and a click sends nothing. */
const compare = byText(alone.el, "button", "Compare with")[0];
check(compare.disabled === true, "with no other motif the button is off");
compare.click();
await page.settle();
check(sent.length === 0, "Compare with nothing chosen sends nothing");

/* With another motif the choice is offered, and Compare sends the two references; a recognition that found nothing says so. */
page.content.innerHTML = "";
const pair = make([
  { id: "mat-1", type: "motif", name: "This", project_id: "prj-1", current_version: 2 },
  { id: "mat-2", type: "motif", name: "Other", project_id: "prj-1", current_version: 1 },
]);
const other = byText(pair.el, "button", "Compare with")[0];
check(other.disabled === false && pair.el.querySelectorAll("option").map((o) => o.value).includes("mat-2"), "another motif is offered");
other.click();
await page.settle();
check(sent.length === 1, "Compare sends one request");
const body = JSON.stringify(sent[0].body || sent[0]);
check(body.includes("mat-1") && body.includes("mat-2"), "the request names both motifs");
check(pair.el.querySelector(".compare-unrelated") !== null, "a recognition that found nothing says the motifs are unrelated");

finish();
