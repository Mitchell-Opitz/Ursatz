/* A small fake document for the view tests: elements with children, attributes, class lists, listeners, text, select options and
 * innerHTML (a tag, attribute and text parser, enough for the templates of the views), plus the lookups the views use (getElementById, querySelector
 * and querySelectorAll by tag, class, id and attribute, with descendant and child combinators). No layout, no events beyond what a test dispatches.
 *
 *   const { installDom, fakeFetch, byText } = await import("./support/dom.js");
 *   const dom = installDom({ ids: ["content", "tool-list"] });   // sets globalThis.document and window; ids are elements the page has
 *   const calls = fakeFetch((request) => ({ ok: true, items: [] })); // sets globalThis.fetch; calls lists {method, url, body}
 *   dom.document.getElementById("content") ... dom.click(button) ... await dom.settle();
 */

const VOID = new Set(["input", "br", "hr", "img", "meta", "link"]);

export class FakeText {
  constructor(text) {
    this.nodeType = 3;
    this.textContent = String(text);
    this.parentNode = null;
  }
}

function parseSelector(text) {
  return text.split(",").map((part) => {
    const steps = [];
    let combinator = " ";
    for (const token of part.trim().replace(/\s*>\s*/g, " > ").split(/\s+(?![^\[]*\])/)) {
      if (token === ">") {
        combinator = ">";
        continue;
      }
      const compound = { tag: "", id: "", classes: [], attrs: [], combinator };
      const re = /^([a-z][\w-]*)|#([\w-]+)|\.([\w-]+)|\[([\w-]+)(?:([~^$*]?=)"?([^\]"]*)"?)?\]/gi;
      let m;
      let rest = token;
      while ((m = re.exec(token)) !== null) {
        if (m[1]) {
          compound.tag = m[1].toLowerCase();
        } else if (m[2]) {
          compound.id = m[2];
        } else if (m[3]) {
          compound.classes.push(m[3]);
        } else {
          compound.attrs.push({ name: m[4], op: m[5] || "", value: m[6] });
        }
        rest = rest.replace(m[0], "");
      }
      steps.push(compound);
      combinator = " ";
    }
    return steps;
  });
}

function matchesCompound(el, c) {
  if (c.tag && el.tag !== c.tag) {
    return false;
  }
  if (c.id && el.id !== c.id) {
    return false;
  }
  if (!c.classes.every((name) => el.classList.contains(name))) {
    return false;
  }
  return c.attrs.every((a) => {
    const value = el.getAttribute(a.name);
    if (value === null) {
      return false;
    }
    return a.op === "" || (a.op === "=" && value === a.value) || (a.op === "~=" && value.split(" ").includes(a.value)) || (a.op === "^=" && value.startsWith(a.value));
  });
}

function matchesSteps(el, steps, index) {
  if (!matchesCompound(el, steps[index])) {
    return false;
  }
  if (index === 0) {
    return true;
  }
  if (steps[index].combinator === ">") {
    return el.parentNode instanceof FakeEl && matchesSteps(el.parentNode, steps, index - 1);
  }
  for (let up = el.parentNode; up instanceof FakeEl; up = up.parentNode) {
    if (matchesSteps(up, steps, index - 1)) {
      return true;
    }
  }
  return false;
}

export class FakeEl {
  constructor(tag, ownerDocument) {
    this.nodeType = 1;
    this.tag = String(tag).toLowerCase();
    this.tagName = this.tag.toUpperCase();
    this.ownerDocument = ownerDocument || null;
    this.children = [];
    this.parentNode = null;
    this.attributes = {};
    this.listeners = {};
    this.style = { setProperty() {}, removeProperty() {} };
    this.dataset = {};
    this._value = undefined;
    this.checked = false;
    this.disabled = false;
    this.hidden = false;
    this.open = false;
    this.type = "";
    this.selected = false;
    const owner = this;
    this.classList = {
      names: () => owner.className.split(/\s+/).filter(Boolean),
      contains: (name) => this.classList.names().includes(name),
      add: (...names) => {
        owner.className = [...new Set([...this.classList.names(), ...names])].join(" ");
      },
      remove: (...names) => {
        owner.className = this.classList.names().filter((n) => !names.includes(n)).join(" ");
      },
      toggle: (name, force) => {
        const on = force === undefined ? !this.classList.contains(name) : Boolean(force);
        if (on) {
          this.classList.add(name);
        } else {
          this.classList.remove(name);
        }
        return on;
      },
    };
    this.className = "";
  }

  get value() {
    if (this.tag === "select") {
      const chosen = this.options.find((option) => option.selected) || this.options[0];
      return chosen ? chosen.value : "";
    }
    if (this.tag === "option") {
      return this._value !== undefined ? this._value : this.textContent;
    }
    return this._value !== undefined ? this._value : "";
  }

  set value(next) {
    if (this.tag === "select") {
      for (const option of this.options) {
        option.selected = option.value === String(next);
      }
    } else {
      this._value = String(next);
    }
  }

  get id() {
    return this.attributes.id || "";
  }

  set id(value) {
    this.attributes.id = String(value);
  }

  get isConnected() {
    let node = this;
    while (node.parentNode) {
      node = node.parentNode;
    }
    return node.tag === "body" || node.tag === "html" || node.isRoot === true;
  }

  get parentElement() {
    return this.parentNode instanceof FakeEl ? this.parentNode : null;
  }

  get firstChild() {
    return this.children[0] || null;
  }

  get lastChild() {
    return this.children[this.children.length - 1] || null;
  }

  get childNodes() {
    return this.children;
  }

  get options() {
    return this.children.filter((child) => child.tag === "option");
  }

  /* The option a select shows: the selected one, else the first (as the value reads it). */
  get selectedOptions() {
    const chosen = this.tag === "select" ? this.options.find((option) => option.selected) || this.options[0] : null;
    return chosen ? [chosen] : [];
  }

  get textContent() {
    return this.children.map((child) => child.textContent).join("");
  }

  set textContent(text) {
    this.children = [];
    if (String(text) !== "") {
      this.appendChild(new FakeText(text));
    }
  }

  set innerHTML(html) {
    this.children = [];
    parseHtml(String(html), this);
  }

  get innerHTML() {
    return this.children.map((child) => (child instanceof FakeText ? child.textContent : child.outerHTML)).join("");
  }

  get outerHTML() {
    const attrs = Object.entries(this.attributes).map(([k, v]) => ` ${k}="${v}"`).join("");
    return `<${this.tag}${attrs}>${this.innerHTML}</${this.tag}>`;
  }

  setAttribute(name, value) {
    if (name === "class") {
      this.className = String(value);
    } else {
      this.attributes[name] = String(value);
    }
    if (name === "value") {
      this._value = String(value);
    }
  }

  getAttribute(name) {
    if (name === "class") {
      return this.className === "" ? null : this.className;
    }
    return name in this.attributes ? this.attributes[name] : null;
  }

  hasAttribute(name) {
    return this.getAttribute(name) !== null;
  }

  removeAttribute(name) {
    delete this.attributes[name];
  }

  appendChild(node) {
    if (node instanceof FakeEl && node.isFragment) {
      for (const child of [...node.children]) {
        this.appendChild(child);
      }
      node.children = [];
      return node;
    }
    if (node.parentNode) {
      node.parentNode.removeChild(node);
    }
    node.parentNode = this;
    this.children.push(node);
    return node;
  }

  append(...nodes) {
    for (const node of nodes) {
      this.appendChild(typeof node === "string" ? new FakeText(node) : node);
    }
  }

  prepend(...nodes) {
    for (const node of nodes.reverse()) {
      this.insertBefore(typeof node === "string" ? new FakeText(node) : node, this.children[0] || null);
    }
  }

  insertBefore(node, ref) {
    if (node.parentNode) {
      node.parentNode.removeChild(node);
    }
    node.parentNode = this;
    const at = ref ? this.children.indexOf(ref) : -1;
    if (at < 0) {
      this.children.push(node);
    } else {
      this.children.splice(at, 0, node);
    }
    return node;
  }

  get nextSibling() {
    return this.parentNode ? this.parentNode.children[this.parentNode.children.indexOf(this) + 1] || null : null;
  }

  get previousSibling() {
    return this.parentNode ? this.parentNode.children[this.parentNode.children.indexOf(this) - 1] || null : null;
  }

  get nextElementSibling() {
    let next = this.nextSibling;
    while (next && next.nodeType !== 1) {
      next = next.nextSibling;
    }
    return next;
  }

  after(...nodes) {
    const parent = this.parentNode;
    let ref = this.nextSibling;
    for (const node of nodes) {
      parent.insertBefore(typeof node === "string" ? new FakeText(node) : node, ref);
    }
  }

  before(...nodes) {
    for (const node of nodes) {
      this.parentNode.insertBefore(typeof node === "string" ? new FakeText(node) : node, this);
    }
  }

  replaceWith(...nodes) {
    this.after(...nodes);
    this.remove();
  }

  removeChild(node) {
    this.children = this.children.filter((child) => child !== node);
    node.parentNode = null;
    return node;
  }

  remove() {
    if (this.parentNode) {
      this.parentNode.removeChild(this);
    }
  }

  replaceChildren(...nodes) {
    this.children = [];
    this.append(...nodes);
  }

  addEventListener(type, fn) {
    (this.listeners[type] ||= []).push(fn);
  }

  removeEventListener(type, fn) {
    this.listeners[type] = (this.listeners[type] || []).filter((f) => f !== fn);
  }

  dispatchEvent(event) {
    const e = Object.assign({ target: this, currentTarget: this, preventDefault() {}, stopPropagation() {} }, event);
    for (const fn of [...(this.listeners[e.type] || [])]) {
      fn(e);
    }
    return true;
  }

  click() {
    if (this.disabled) {
      return;
    }
    if (this.tag === "input" && this.type === "checkbox") {
      this.checked = !this.checked;
      this.dispatchEvent({ type: "change" });
    }
    this.dispatchEvent({ type: "click" });
  }

  focus() {}
  blur() {}
  select() {}
  scrollIntoView() {}
  getBoundingClientRect() {
    return { left: 0, top: 0, right: 0, bottom: 0, width: 0, height: 0 };
  }

  contains(node) {
    for (let up = node; up; up = up.parentNode) {
      if (up === this) {
        return true;
      }
    }
    return false;
  }

  all() {
    return this.children.flatMap((child) => (child instanceof FakeEl ? [child, ...child.all()] : []));
  }

  matches(selector) {
    return parseSelector(selector).some((steps) => matchesSteps(this, steps, steps.length - 1));
  }

  closest(selector) {
    for (let up = this; up instanceof FakeEl; up = up.parentNode) {
      if (up.matches(selector)) {
        return up;
      }
    }
    return null;
  }

  querySelectorAll(selector) {
    const parsed = parseSelector(selector);
    return this.all().filter((el) => parsed.some((steps) => matchesSteps(el, steps, steps.length - 1)));
  }

  querySelector(selector) {
    return this.querySelectorAll(selector)[0] || null;
  }

  getElementById(id) {
    return this.all().find((el) => el.id === id) || null;
  }
}

const ATTR = /([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g;

function decode(text) {
  return text.replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"').replace(/&nbsp;/g, " ");
}

/* Parses a fragment of HTML into parent: elements with attributes, text, void tags and comments. */
function parseHtml(html, parent) {
  const stack = [parent];
  const re = /<!--[\s\S]*?-->|<\/([a-z][\w-]*)\s*>|<([a-z][\w-]*)((?:\s+[^>]*?)?)(\/?)>|([^<]+)/gi;
  let m;
  while ((m = re.exec(html)) !== null) {
    const top = stack[stack.length - 1];
    if (m[1]) {
      if (stack.length > 1) {
        stack.pop();
      }
    } else if (m[2]) {
      const el = new FakeEl(m[2], parent.ownerDocument);
      let a;
      ATTR.lastIndex = 0;
      while ((a = ATTR.exec(m[3] || "")) !== null) {
        const value = a[2] !== undefined ? a[2] : a[3] !== undefined ? a[3] : a[4] !== undefined ? a[4] : "";
        el.setAttribute(a[1], decode(value));
        if (a[1] === "hidden") {
          el.hidden = true;
        }
        if (a[1] === "type") {
          el.type = value;
        }
      }
      top.appendChild(el);
      if (!VOID.has(el.tag) && !m[4]) {
        stack.push(el);
      }
    } else if (m[5] !== undefined) {
      top.appendChild(new FakeText(decode(m[5])));
    }
  }
}

export function createDocument() {
  const document = {
    isFake: true,
    body: null,
    createElement: (tag) => new FakeEl(tag, document),
    createElementNS: (ns, tag) => new FakeEl(tag, document),
    createTextNode: (text) => new FakeText(text),
    createDocumentFragment() {
      const fragment = new FakeEl("fragment", document);
      fragment.isFragment = true;
      return fragment;
    },
    getElementById: (id) => document.body.getElementById(id),
    querySelector: (selector) => document.body.querySelector(selector),
    querySelectorAll: (selector) => document.body.querySelectorAll(selector),
    addEventListener() {},
    removeEventListener() {},
  };
  document.body = new FakeEl("body", document);
  document.body.isRoot = true;
  return document;
}

/* Installs a fake document (with the given ids as empty divs in the body) and a window with a storage; returns helpers. */
export function installDom(options) {
  const o = options || {};
  const document = createDocument();
  for (const id of o.ids || []) {
    const el = document.createElement(o.tags && o.tags[id] ? o.tags[id] : "div");
    el.id = id;
    document.body.appendChild(el);
  }
  const store = new Map();
  const localStorage = { getItem: (key) => (store.has(key) ? store.get(key) : null), setItem: (key, value) => store.set(key, String(value)), removeItem: (key) => store.delete(key) };
  globalThis.document = document;
  globalThis.window = { localStorage, addEventListener() {}, removeEventListener() {}, location: { hash: "", search: "" } };
  globalThis.localStorage = localStorage;
  return {
    document,
    localStorage,
    click: (el) => el.click(),
    /* Lets the promises the views wait on run (a few turns of the event loop). */
    async settle(turns = 6) {
      for (let i = 0; i < turns; i++) {
        await new Promise((resolve) => setTimeout(resolve, 0));
      }
    },
  };
}

/* The elements below root (or the document's body) of a tag whose text is exactly text. */
export function byText(root, tag, text) {
  return root.all().filter((el) => el.tag === tag && el.textContent === text);
}

/* Replaces fetch: each call is answered by handler({method, url, body}) with the JSON the route would send (or {status, body}); returns the call log. */
export function fakeFetch(handler) {
  const calls = [];
  globalThis.fetch = async (url, init) => {
    const call = { method: (init && init.method) || "GET", url: String(url), body: init && init.body ? JSON.parse(init.body) : undefined };
    calls.push(call);
    const answer = handler(call);
    const status = answer && answer.status !== undefined ? answer.status : 200;
    const body = answer && answer.status !== undefined ? answer.body : answer;
    return { ok: status >= 200 && status < 300, status, json: async () => body, text: async () => JSON.stringify(body) };
  };
  return calls;
}
