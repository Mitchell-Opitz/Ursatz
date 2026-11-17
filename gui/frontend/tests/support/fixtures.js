/* Where the frontend tests find fixtures they do not keep in this folder.
 *
 *   const { generated, analyzerSnapshot } = require("./support/fixtures.js");
 *   generated("vocabulary.json")                  // written by the build from the in-tree Composer
 *   analyzerSnapshot("mozart-545.analysis.json")  // the Analyzer's own golden
 *
 * generated(name) needs URSATZ_GUI_GENERATED_FIXTURES, which ctest sets; the folder is filled by the
 * gui_frontend_fixtures build target (gui/backend/tests/write_frontend_fixtures.c).
 */
const path = require("path");

function generated(name) {
  const folder = process.env.URSATZ_GUI_GENERATED_FIXTURES;
  if (!folder) {
    throw new Error("run through ctest: URSATZ_GUI_GENERATED_FIXTURES is not set");
  }
  return path.join(folder, name);
}

function analyzerSnapshot(name) {
  return path.join(__dirname, "../../../../analyzer/tests/snapshots", name);
}

module.exports = { generated, analyzerSnapshot };
