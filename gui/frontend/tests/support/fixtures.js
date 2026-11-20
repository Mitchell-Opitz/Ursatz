/* Where the frontend tests find fixtures they do not keep in this folder.
 *
 *   import { generated, analyzerSnapshot } from "./support/fixtures.js";
 *   generated("vocabulary.json")                  // written by the build from the in-tree Composer
 *   analyzerSnapshot("mozart-545.analysis.json")  // the Analyzer's own golden
 *
 * generated(name) needs URSATZ_GUI_GENERATED_FIXTURES, which ctest sets; the folder is filled by the
 * gui_frontend_fixtures build target (gui/backend/tests/write_frontend_fixtures.c).
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const here = path.dirname(fileURLToPath(import.meta.url));

/* The parsed JSON file at a path (a fixture the test reads once). */
export function readJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}


export function generated(name) {
  const folder = process.env.URSATZ_GUI_GENERATED_FIXTURES;
  if (!folder) {
    throw new Error("run through ctest: URSATZ_GUI_GENERATED_FIXTURES is not set");
  }
  return path.join(folder, name);
}

export function analyzerSnapshot(name) {
  return path.join(here, "../../../../analyzer/tests/snapshots", name);
}

/* The text of the named modules of the page (paths under public/), for the checks that a piece of markup or a call is in the page's code. */
export function pageSource(...modules) {
  return modules.map((name) => fs.readFileSync(path.join(here, "..", "..", "public", name), "utf8")).join("\n");
}

/* The text of public/index.html. */
export function indexSource() {
  return fs.readFileSync(path.join(here, "..", "..", "public", "index.html"), "utf8");
}
