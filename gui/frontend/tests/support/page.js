/* The page the view tests mount their views on: the sidebar and content area of public/index.html on the fake document, a fetch that answers by
 * route, and the Composer vocabulary and forms the build generates.
 *
 *   const page = await openPage({ "GET /api/subjects": { ok: true, subjects: [] } });   // routes the test answers (others: 404)
 *   page.calls                // [{method, url, body}] the page sent
 *   page.content              // the #content element
 *   await page.settle();      // lets the pending promises of the views run
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import Vocabulary from "../../public/core/vocabulary.js";
import { setShell } from "../../public/views/shell-context.js";
import { fakeFetch, installDom } from "./dom.js";
import { generated, readJson } from "./fixtures.js";

const here = path.dirname(fileURLToPath(import.meta.url));

/* The inside of <body> of index.html up to its scripts. */
function indexBody() {
  const html = fs.readFileSync(path.join(here, "..", "..", "public", "index.html"), "utf8");
  return html.slice(html.indexOf("<body>") + 6, html.indexOf("<script"));
}

/* routes: {"METHOD /path": answer | (call) => answer}; a path matches by its start up to the query. */
export async function openPage(routes, options) {
  const o = options || {};
  const dom = installDom();
  dom.document.body.innerHTML = indexBody();
  const table = Object.assign({}, routes);
  const calls = fakeFetch((call) => {
    const route = `${call.method} ${call.url.split("?")[0]}`;
    if (table[route] !== undefined) {
      return typeof table[route] === "function" ? table[route](call) : table[route];
    }
    return { status: 404, body: { ok: false, error: `no route ${route}`, code: "not-found" } };
  });
  Vocabulary.set(readJson(generated("vocabulary.json")), readJson(generated("forms.json")));
  const page = {
    dom,
    document: dom.document,
    calls,
    routes: table,
    content: dom.document.getElementById("content"),
    settle: dom.settle,
    /* The calls made to a path (by method and start of the url). */
    callsTo: (method, url) => calls.filter((call) => call.method === method && call.url.startsWith(url)),
    /* Gives the views the shell they call; the test gives only what its view uses. */
    setShell(context) {
      setShell(Object.assign({ contentEl: () => page.content }, context));
    },
  };
  if (o.shell !== false) {
    page.setShell({});
  }
  return page;
}
