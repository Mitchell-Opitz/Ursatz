/* Covers what the materials views share (views/materials-common.js): whether the materials view is the one on screen, opening a linked item in its own
 * tool, and the Used by table of a motif or a theme. Plain Node script, run through ctest. */
import { buildUsedByTable, materialsState, materialsViewActive, openLinkedItem } from "../public/views/materials-common.js";
import { setShell } from "../public/views/shell-context.js";
import { check, same, finish } from "./support/check.js";
import { installDom } from "./support/dom.js";

installDom();

/* A shell that says which tool is active and keeps what the views asked of it, in order. */
let active = "themes";
const asked = [];
setShell({
  activeToolId: () => active,
  leaveWorkspace: () => asked.push("leaveWorkspace"),
  setActiveTool: (id) => asked.push(`setActiveTool ${id}`),
  openItem: (type, id) => asked.push(`openItem ${type} ${id}`),
});

/* The materials view is active when the tool on screen is the one of the type the state holds. */
materialsState.type = "theme";
check(materialsViewActive() === true, "the themes tool with a theme type is the materials view");
materialsState.type = "motif";
check(materialsViewActive() === false, "the themes tool with a motif type is not");
materialsState.type = null;
check(materialsViewActive() === false, "no type is not the materials view");
active = null;
materialsState.type = "theme";
check(materialsViewActive() === false, "no tool on screen is not the materials view");

/* Opening a linked item: an unknown type does nothing; a known one leaves the workspace, picks its tool and opens the item, in that order. */
openLinkedItem("bogus", "x");
check(asked.length === 0, "a type that is none asks the shell for nothing");
openLinkedItem("motif", "mat-1");
check(same(asked, ["leaveWorkspace", "setActiveTool motifs", "openItem motif mat-1"]), "a motif is opened in the motifs tool, after leaving the workspace");
check(materialsState.type === "motif", "the state follows the type opened");

/* The Used by table: nothing uses it yet, or one row with its version and whether an update is available. */
const empty = buildUsedByTable([], 1, "theme");
check(empty.textContent.includes("Used by") && empty.textContent.includes("Nothing uses this theme yet."), "no users: Nothing uses this theme yet.");
check(buildUsedByTable([], 1).textContent.includes("Nothing uses this motif yet."), "the noun is motif by default");
materialsState.projects = [{ id: "prj-1", name: "Work" }];
const table = buildUsedByTable([{ id: "thm-1", name: "Sentence", type: "theme", project_id: "prj-1", version: 1 }], 2, "motif");
const text = table.textContent;
check(text.includes("Used by") && text.includes("Sentence") && text.includes("theme") && text.includes("v1") && text.includes("Work"), "one user: its name, type, version and project");
check(text.includes("update available"), "a use behind the current version says an update is available");
const current = buildUsedByTable([{ id: "thm-1", name: "Sentence", type: "theme", project_id: "prj-1", version: 2 }], 2, "motif");
check(!current.textContent.includes("update available"), "a use of the current version does not");

/* The name opens the item through the shell. */
asked.length = 0;
const link = table.querySelectorAll("a")[0];
link.click();
check(same(asked, ["leaveWorkspace", "setActiveTool themes", "openItem theme thm-1"]), "the name of a user opens it in its tool");

finish();
