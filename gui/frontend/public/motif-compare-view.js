/* The Compare section of the motif detail (P14 B10): pick another motif, see how it derives from this one (the chain,
 * the derivation, what was kept and changed, "N% related") and link it as a form of this motif. Renders from
 * VariantTree; ctx is {id, version, projectId, projects, items, send(request), linked()}, send being the page's JSON
 * request helper that answers {ok, status, data} and items the list of every item. */
const MotifCompareView = (function () {
  function h(tag, className, text) {
    const el = document.createElement(tag);
    if (className) {
      el.className = className;
    }
    if (text !== undefined) {
      el.textContent = text;
    }
    return el;
  }

  function errorText(data, fallback) {
    return data && typeof data.error === "string" && data.error !== "" ? data.error : fallback;
  }

  function setStatus(el, text, isError) {
    el.textContent = text;
    el.classList.toggle("error", Boolean(isError));
  }

  function mount(parent, ctx) {
    const section = h("div", "materials-compare");
    section.appendChild(h("h2", "section-title", "Compare"));
    const picker = h("div", "upload-controls");
    const query = h("input");
    query.type = "text";
    query.placeholder = "Search motifs by name";
    const projectSelect = h("select");
    for (const option of MaterialsModel.projectOptions(ctx.projects)) {
      const o = h("option", "", option.label);
      o.value = option.value;
      projectSelect.appendChild(o);
    }
    projectSelect.value = ctx.projectId;
    const choice = h("select", "materials-use-choice");
    const compare = h("button", "", "Compare with");
    compare.type = "button";
    const status = h("div", "piece-list-status");
    const result = h("div", "compare-result");

    function fillChoices() {
      const found = MaterialsModel.pickerItems(ctx.items, ctx.projects, {
        query: query.value,
        type: "motif",
        project: projectSelect.value,
        exclude: [ctx.id],
      });
      choice.innerHTML = "";
      for (const item of found) {
        const option = h("option", "", item.label);
        option.value = item.id;
        option.dataset.version = String(item.version);
        option.dataset.name = item.name;
        choice.appendChild(option);
      }
      compare.disabled = found.length === 0;
    }
    query.addEventListener("input", fillChoices);
    projectSelect.addEventListener("input", fillChoices);

    function showResult(recognition, target) {
      result.innerHTML = "";
      const view = VariantTree.compareView(recognition);
      if (!view.found) {
        result.appendChild(h("div", "compare-unrelated", view.text));
        return;
      }
      const title = h("div", "compare-title");
      title.appendChild(h("strong", "", `${target.name} from this motif`));
      title.appendChild(h("span", "variant-badge", view.related));
      result.appendChild(title);
      result.appendChild(h("div", "variant-derivation", view.chain));
      result.appendChild(h("div", "variant-meta", view.derivation));
      const row = h("span", "variant-chips");
      for (const chip of view.chips) {
        row.appendChild(h("span", `variant-chip variant-chip-${chip.state}`, chip.text));
      }
      result.appendChild(row);
      const link = h("button", "", "Link as a form of this motif");
      link.type = "button";
      link.addEventListener("click", async () => {
        const answer = await ctx.send(
          VariantTree.linkFormRequest(target.id, { item: ctx.id, version: ctx.version }, recognition),
        );
        setStatus(status, answer.ok ? `${target.name} is now a form of this motif` : errorText(answer.data, "Could not link the motif"), !answer.ok);
        if (answer.ok) {
          link.disabled = true;
          ctx.linked();
        }
      });
      result.appendChild(link);
    }

    compare.addEventListener("click", async () => {
      const selected = choice.selectedOptions[0];
      if (!selected) {
        return;
      }
      const target = { id: selected.value, version: Number(selected.dataset.version), name: selected.dataset.name };
      setStatus(status, "Comparing...", false);
      result.innerHTML = "";
      const answer = await ctx.send(
        VariantTree.compareRequest({ item: ctx.id, version: ctx.version }, { item: target.id, version: target.version }),
      );
      if (!answer.ok) {
        setStatus(status, errorText(answer.data, "Could not compare the motifs"), true);
        return;
      }
      setStatus(status, "", false);
      showResult(answer.data, target);
    });

    fillChoices();
    picker.append(query, projectSelect, choice, compare);
    section.append(picker, status, result);
    parent.appendChild(section);
    return { el: section };
  }

  return { mount };
})();
