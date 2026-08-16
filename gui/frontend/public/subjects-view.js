/* The Subjects tool (R-117): list, create, and a detail with related
 * subjects and linked items and pieces. Pure parts are in subjects-panel.js;
 * sendMaterialsRequest, materialsElement and the page globals come from
 * materials-view.js and app.js. */
const subjectsState = { subjects: [] };

async function loadSubjectList() {
  const answer = await sendMaterialsRequest(SubjectsPanel.buildSubjectListRequest());
  subjectsState.subjects = answer.ok && Array.isArray(answer.data.subjects) ? answer.data.subjects : [];
  return answer;
}

function subjectStatus(text, isError) {
  const el = materialsElement("div", isError ? "upload-status error" : "upload-status", text || "");
  el.setAttribute("role", "status");
  return el;
}

function fillKindSelect(select, selected) {
  select.innerHTML = "";
  for (const entry of SubjectsPanel.KINDS) {
    const option = materialsElement("option", "", entry.title);
    option.value = entry.kind;
    select.appendChild(option);
  }
  select.value = selected || SubjectsPanel.KINDS[0].kind;
}

async function renderSubjectsView() {
  const content = getContentEl();
  content.innerHTML = "";
  content.appendChild(materialsElement("h1", "view-title", "Subjects"));

  const controls = materialsElement("div", "upload-controls");
  const name = materialsElement("input");
  name.type = "text";
  name.placeholder = "New subject name";
  const kind = materialsElement("select");
  fillKindSelect(kind);
  const note = materialsElement("input");
  note.type = "text";
  note.placeholder = "Note (optional)";
  const create = materialsElement("button", "", "New");
  create.type = "button";
  const statusEl = subjectStatus("");
  async function submit() {
    if (name.value.trim() === "") {
      return;
    }
    const answer = await sendMaterialsRequest(
      SubjectsPanel.buildCreateSubjectRequest({ name: name.value, kind: kind.value, note: note.value }),
    );
    if (!answer.ok) {
      statusEl.textContent = SubjectsPanel.errorText(answer.data, "Could not create the subject");
      statusEl.classList.add("error");
      return;
    }
    notifySubjectsChanged();
    renderSubjectsView();
  }
  create.addEventListener("click", submit);
  name.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      submit();
    }
  });
  controls.append(name, kind, note, create);
  content.append(controls, statusEl);

  const listEl = materialsElement("div");
  content.appendChild(listEl);
  const answer = await loadSubjectList();
  if (!answer.ok) {
    listEl.appendChild(
      materialsElement("div", "piece-list-status error", SubjectsPanel.errorText(answer.data, "Could not load subjects")),
    );
    return;
  }
  const rows = SubjectsPanel.subjectRows(subjectsState.subjects);
  if (rows.length === 0) {
    listEl.appendChild(materialsElement("div", "piece-list-status", "No subjects yet."));
    return;
  }
  const table = materialsElement("table", "library-table");
  const head = materialsElement("tr");
  for (const label of ["Name", "Kind", "Items", "Pieces", "Related"]) {
    head.appendChild(materialsElement("th", "", label));
  }
  table.appendChild(head);
  for (const row of rows) {
    const tr = materialsElement("tr", "clickable");
    for (const value of [row.name, row.kindLabel, row.items, row.pieces, row.related]) {
      tr.appendChild(materialsElement("td", "", String(value)));
    }
    tr.addEventListener("click", () => renderSubjectDetail(row.id));
    table.appendChild(tr);
  }
  const panel = materialsElement("section", "panel library-panel");
  panel.appendChild(table);
  listEl.appendChild(panel);
}

/* Opens a subject from the sidebar panel or a chip. */
function openSubject(id) {
  leaveWorkspace();
  setActiveTool("subjects");
  renderSubjectDetail(id);
}

/* Opens a linked item in its own tool. */
function openLinkedItem(type, id) {
  const toolId = MATERIALS_TOOL_IDS[type];
  if (!toolId) {
    return;
  }
  leaveWorkspace();
  setActiveTool(toolId);
  materialsState.type = type;
  renderMaterialsDetail(type, id);
}

function buildLinkList(title, rows, emptyText, onOpen, onRemove) {
  const section = materialsElement("div");
  section.appendChild(materialsElement("h2", "section-title", title));
  if (rows.length === 0) {
    section.appendChild(materialsElement("div", "piece-list-status", emptyText));
    return section;
  }
  const list = materialsElement("ul", "materials-list");
  for (const row of rows) {
    const li = materialsElement("li");
    if (onOpen && !row.missing) {
      const open = materialsElement("a", "clickable", row.label);
      open.addEventListener("click", () => onOpen(row));
      li.appendChild(open);
    } else {
      li.appendChild(materialsElement("span", row.missing ? "error" : "", row.missing ? `${row.label} (missing)` : row.label));
    }
    li.appendChild(document.createTextNode(" "));
    const remove = materialsElement("button", "", "Remove");
    remove.type = "button";
    remove.addEventListener("click", () => onRemove(row));
    li.appendChild(remove);
    list.appendChild(li);
  }
  section.appendChild(list);
  return section;
}

async function renderSubjectDetail(id) {
  const content = getContentEl();
  content.innerHTML = "";
  const back = materialsElement("a", "back-link clickable", "Back to Subjects");
  back.addEventListener("click", () => renderSubjectsView());
  content.appendChild(back);

  const [subjectAnswer, , itemsAnswer, piecesAnswer] = await Promise.all([
    sendMaterialsRequest(SubjectsPanel.buildSubjectGetRequest(id)),
    loadSubjectList(),
    sendMaterialsRequest({ method: "GET", url: "/api/materials/items" }),
    sendMaterialsRequest({ method: "GET", url: "/api/analyzer/pieces" }),
  ]);
  if (!subjectAnswer.ok) {
    content.appendChild(subjectStatus(SubjectsPanel.errorText(subjectAnswer.data, "Could not load the subject"), true));
    return;
  }
  const subject = subjectAnswer.data.subject;
  const items = itemsAnswer.ok && Array.isArray(itemsAnswer.data.items) ? itemsAnswer.data.items : [];
  const pieces = piecesAnswer.ok && Array.isArray(piecesAnswer.data.pieces) ? piecesAnswer.data.pieces : [];
  content.appendChild(materialsElement("h1", "view-title", subject.name));
  const statusEl = subjectStatus("");

  const fail = (answer, fallback) => {
    statusEl.textContent = SubjectsPanel.errorText(answer.data, fallback);
    statusEl.classList.add("error");
  };
  const changed = () => {
    notifySubjectsChanged();
    renderSubjectDetail(id);
  };

  const controls = materialsElement("div", "upload-controls");
  const name = materialsElement("input");
  name.type = "text";
  name.value = subject.name;
  const kind = materialsElement("select");
  fillKindSelect(kind, subject.kind);
  const note = materialsElement("input");
  note.type = "text";
  note.value = subject.note || "";
  note.placeholder = "Note";
  const save = materialsElement("button", "", "Save");
  save.type = "button";
  save.addEventListener("click", async () => {
    const answer = await sendMaterialsRequest(
      SubjectsPanel.buildUpdateSubjectRequest(id, { name: name.value, kind: kind.value, note: note.value }),
    );
    if (!answer.ok) {
      fail(answer, "Could not save the subject");
      return;
    }
    changed();
  });
  const del = materialsElement("button", "", "Delete");
  del.type = "button";
  del.addEventListener("click", async () => {
    if (!window.confirm(`Delete subject "${subject.name}"? Its links are removed.`)) {
      return;
    }
    const answer = await sendMaterialsRequest(SubjectsPanel.buildDeleteSubjectRequest(id));
    if (!answer.ok) {
      fail(answer, "Could not delete the subject");
      return;
    }
    notifySubjectsChanged();
    renderSubjectsView();
  });
  controls.append(name, kind, note, save, del);
  content.append(controls, statusEl);

  async function link(group, otherId, add) {
    const answer = await sendMaterialsRequest(SubjectsPanel.buildLinkRequest(id, group, otherId, add));
    if (!answer.ok) {
      fail(answer, "Could not change the link");
      return;
    }
    changed();
  }

  const related = SubjectsPanel.relatedRows(subject.related, subjectsState.subjects);
  content.appendChild(
    buildLinkList("Related subjects", related, "No related subjects.", (row) => openSubject(row.id), (row) =>
      link("related", row.id, false),
    ),
  );
  const options = SubjectsPanel.pickerOptions(subjectsState.subjects, [id, ...(subject.related || [])]);
  if (options.length > 0) {
    const picker = materialsElement("div", "upload-controls");
    const select = materialsElement("select");
    for (const option of options) {
      const o = materialsElement("option", "", option.label);
      o.value = option.value;
      select.appendChild(o);
    }
    const add = materialsElement("button", "", "Add related");
    add.type = "button";
    add.addEventListener("click", () => link("related", select.value, true));
    picker.append(select, add);
    content.appendChild(picker);
  }

  const itemRows = SubjectsPanel.itemLinkRows(subject.items, items).map((row) => ({
    ...row,
    label: row.type ? `${row.name} (${row.type})` : row.name,
  }));
  content.appendChild(
    buildLinkList("Linked items", itemRows, "No linked items.", (row) => openLinkedItem(row.type, row.id), (row) =>
      link("items", row.id, false),
    ),
  );
  const pieceRows = SubjectsPanel.pieceLinkRows(subject.pieces, pieces).map((row) => ({ ...row, label: row.name }));
  content.appendChild(
    buildLinkList("Linked Library pieces", pieceRows, "No linked pieces.", (row) => showAnalysisView(row.id), (row) =>
      link("pieces", row.id, false),
    ),
  );
}

/* The Subjects field of an item detail or a Library piece editor: chips with
 * remove, a picker over the existing subjects, and an inline create. group is
 * "items" or "pieces"; targetId the item or piece id. Returns
 * {el, setSubjects(list), applyTo(id)}; the field shows "Loading..." until
 * subjects are set. Every change refreshes the sidebar panel. With a null
 * targetId (an item not saved yet) the chosen subjects stay in a pending list
 * and applyTo(id) links each of them, answering the names that failed. */
function buildSubjectsField(group, targetId, initial) {
  const el = materialsElement("div", "materials-subjects");
  let current = Array.isArray(initial) ? initial.slice() : null;
  let all = [];
  const errorEl = materialsElement("div", "materials-field-error");

  async function changeLink(subjectId, add, subject) {
    if (targetId === null || targetId === undefined) {
      current = add ? SubjectsPanel.withSubject(current, subject) : SubjectsPanel.withoutSubject(current, subjectId);
      return true;
    }
    const answer = await sendMaterialsRequest(SubjectsPanel.buildLinkRequest(subjectId, group, targetId, add));
    if (!answer.ok) {
      errorEl.textContent = SubjectsPanel.errorText(answer.data, "Could not change the subject");
      return false;
    }
    errorEl.textContent = "";
    const subject = answer.data.subject;
    current = add
      ? current.some((entry) => entry.id === subject.id)
        ? current
        : [...current, { id: subject.id, name: subject.name, kind: subject.kind }]
      : current.filter((entry) => entry.id !== subjectId);
    notifySubjectsChanged();
    return true;
  }

  async function draw() {
    el.innerHTML = "";
    if (current === null) {
      el.appendChild(materialsElement("span", "piece-list-status", "Loading..."));
      return;
    }
    const chips = materialsElement("div", "materials-chips");
    for (const chip of SubjectsPanel.chipRows(current)) {
      const span = materialsElement("span", "materials-chip", chip.label);
      span.title = chip.title;
      const remove = materialsElement("button", "", "×");
      remove.type = "button";
      remove.setAttribute("aria-label", `Remove ${chip.label}`);
      remove.addEventListener("click", async () => {
        if (await changeLink(chip.id, false, chip)) {
          draw();
        }
      });
      span.appendChild(remove);
      chips.appendChild(span);
    }
    if (current.length === 0) {
      chips.appendChild(materialsElement("span", "piece-list-status", "No subjects."));
    }
    el.appendChild(chips);

    const options = SubjectsPanel.pickerOptions(all, current.map((entry) => entry.id));
    if (options.length > 0) {
      const picker = materialsElement("div", "upload-controls");
      const select = materialsElement("select");
      for (const option of options) {
        const o = materialsElement("option", "", option.label);
        o.value = option.value;
        select.appendChild(o);
      }
      const add = materialsElement("button", "", "Add subject");
      add.type = "button";
      add.addEventListener("click", async () => {
        const picked = all.find((entry) => entry.id === select.value);
        if (picked && (await changeLink(select.value, true, picked))) {
          draw();
        }
      });
      picker.append(select, add);
      el.appendChild(picker);
    }

    const create = materialsElement("div", "upload-controls");
    const name = materialsElement("input");
    name.type = "text";
    name.placeholder = "New subject name";
    const kind = materialsElement("select");
    fillKindSelect(kind);
    const make = materialsElement("button", "", "Create and add");
    make.type = "button";
    make.addEventListener("click", async () => {
      if (name.value.trim() === "") {
        return;
      }
      const made = await sendMaterialsRequest(
        SubjectsPanel.buildCreateSubjectRequest({ name: name.value, kind: kind.value }),
      );
      if (!made.ok) {
        errorEl.textContent = SubjectsPanel.errorText(made.data, "Could not create the subject");
        return;
      }
      all = [...all, made.data.subject];
      if (await changeLink(made.data.subject.id, true, made.data.subject)) {
        draw();
      }
    });
    create.append(name, kind, make);
    el.appendChild(create);
    el.appendChild(errorEl);
  }

  async function setSubjects(list) {
    current = Array.isArray(list) ? list.slice() : [];
    await loadSubjectList();
    all = subjectsState.subjects;
    draw();
  }

  /* Links the pending subjects to a newly created item or piece. Answers the
   * names that could not be linked. */
  async function applyTo(id) {
    const failed = [];
    for (const entry of current || []) {
      const answer = await sendMaterialsRequest(SubjectsPanel.buildLinkRequest(entry.id, group, id, true));
      if (!answer.ok) {
        failed.push(entry.name);
      }
    }
    if ((current || []).length > 0) {
      notifySubjectsChanged();
    }
    return failed;
  }

  if (current !== null) {
    setSubjects(current);
  } else {
    draw();
  }
  return { el, setSubjects, applyTo };
}
