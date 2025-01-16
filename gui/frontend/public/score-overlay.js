/* The claim layers drawn over the score (R-099, R-101): one SVG laid over
 * the OSMD SVG, redrawn from scratch on every change. Ribbons and marks sit
 * in lanes above each system and the chord numerals below it; the lane
 * heights come from ScoreGeometry.
 *
 *   const overlay = ScoreOverlay.create(paperEl, osmdHostEl);
 *   overlay.draw(layout, offset, catalog, enabledIds);
 *
 * `offset` turns a piece position into OSMD time (ScoreGeometry's timeline
 * offset). planLanes and fitText are pure; the rest needs a DOM. */
(function (root) {
  const SVG_NS = "http://www.w3.org/2000/svg";
  const MAX_RIBBON_ROWS = 4;
  const ROW = 18;
  const RIBBON_HEIGHT = 14;
  const RIBBON_RADIUS = 3;
  const MIN_RIBBON_WIDTH = 6;
  const MIN_TEXT_WIDTH = 28;
  const TEXT_PADDING = 4;
  const BAR_WIDTH = 2;
  const THIN_HEIGHT = 6;
  const MARK_HEIGHT = 16;
  const MARK_PADDING = 10;
  const CHORD_GAP = 4;
  const TICK_WIDTH = 2;
  const TICK_HEIGHT = 6;
  const INK = "#1f2937";
  const MONO = '"SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace';

  /* The lanes the enabled layers need: {lanes: [{layer, placement, rows}],
   * notes: [layer]}. Ribbon layers are packed into rows (setting row on
   * their items); a mark layer is one row. Empty layers are left out. */
  function planLanes(catalog, enabledIds) {
    const enabled = new Set(enabledIds);
    const lanes = [];
    const notes = [];
    for (const layer of catalog) {
      if (!enabled.has(layer.id) || layer.empty) {
        continue;
      }
      if (layer.kind === "notes") {
        notes.push(layer);
      } else if (layer.kind === "ribbon") {
        const { rows } = ScoreLayers.packRows(layer.items, MAX_RIBBON_ROWS);
        lanes.push({ layer, layerId: layer.id, placement: layer.placement, rows: Math.max(1, rows) });
      } else {
        layer.items.forEach((item) => {
          item.row = 0;
          item.overflow = false;
        });
        lanes.push({ layer, layerId: layer.id, placement: layer.placement, rows: 1 });
      }
    }
    return { lanes, notes };
  }

  /* `text` shortened with an ellipsis to fit `maxWidth` as measured by
   * `measure`; "" when not even one character fits. */
  function fitText(text, maxWidth, measure) {
    if (measure(text) <= maxWidth) {
      return text;
    }
    for (let n = text.length - 1; n >= 1; n--) {
      const candidate = `${text.slice(0, n)}…`;
      if (measure(candidate) <= maxWidth) {
        return candidate;
      }
    }
    return "";
  }

  function svgEl(tag, attrs, text) {
    const node = document.createElementNS(SVG_NS, tag);
    for (const [name, value] of Object.entries(attrs || {})) {
      node.setAttribute(name, String(value));
    }
    if (text !== undefined) {
      node.textContent = text;
    }
    return node;
  }

  function locationText(item) {
    const where = typeof formatLocation === "function" ? formatLocation(item.claim && item.claim.location) : "";
    return where;
  }

  function ariaLabel(layer, item) {
    return `${layer.label} ${item.text}, ${locationText(item)}`;
  }

  /* A rectangle path with separate left and right corner radii. */
  function roundedPath(x, y, w, h, rl, rr) {
    return [
      `M${x + rl} ${y}`,
      `H${x + w - rr}`,
      rr ? `A${rr} ${rr} 0 0 1 ${x + w} ${y + rr}` : "",
      `V${y + h - rr}`,
      rr ? `A${rr} ${rr} 0 0 1 ${x + w - rr} ${y + h}` : "",
      `H${x + rl}`,
      rl ? `A${rl} ${rl} 0 0 1 ${x} ${y + h - rl}` : "",
      `V${y + rl}`,
      rl ? `A${rl} ${rl} 0 0 1 ${x + rl} ${y}` : "",
      "Z",
    ].join(" ");
  }

  function create(paperEl, hostEl) {
    const svg = svgEl("svg", { class: "ws-overlay", role: "group", "aria-label": "Score layers" });
    paperEl.appendChild(svg);
    const canvas = document.createElement("canvas").getContext("2d");

    function measurer(font) {
      return (text) => {
        canvas.font = font;
        return canvas.measureText(text).width;
      };
    }
    const measureRibbon = measurer(`11px ${MONO}`);
    const measureMark = measurer(`bold 10px ${MONO}`);
    const measureChord = measurer(`12px ${MONO}`);

    function clear() {
      svg.replaceChildren();
    }

    /* Lay the overlay exactly over the OSMD SVG. */
    function place() {
      const osmdSvg = hostEl.querySelector("svg");
      if (!osmdSvg) {
        return false;
      }
      const base = paperEl.getBoundingClientRect();
      const rect = osmdSvg.getBoundingClientRect();
      svg.style.left = `${rect.left - base.left - paperEl.clientLeft}px`;
      svg.style.top = `${rect.top - base.top - paperEl.clientTop}px`;
      svg.setAttribute("width", String(rect.width));
      svg.setAttribute("height", String(rect.height));
      return true;
    }

    function rowTop(system, lane, row) {
      return lane.placement === "below"
        ? system.bottom + lane.offset + row * ROW
        : system.top - lane.offset - (row + 1) * ROW;
    }

    function hitGroup(layer, item, label) {
      const group = svgEl("g", {
        role: "button",
        tabindex: "-1",
        "aria-label": label,
        "data-key": item.key,
        "data-layer": layer.id,
      });
      return group;
    }

    function drawRibbon(out, layout, offset, layer, lane, item) {
      const segments = ScoreGeometry.spanSegments(layout, item.start + offset, item.end + offset);
      const group = hitGroup(layer, item, ariaLabel(layer, item));
      const thin = item.style === "thin";
      segments.forEach((segment, n) => {
        const system = layout.systems[segment.system];
        const x = segment.x1;
        const w = Math.max(segment.x2 - segment.x1, MIN_RIBBON_WIDTH);
        const rowY = rowTop(system, lane, item.row);
        if (thin) {
          group.appendChild(
            svgEl("rect", {
              x, y: rowY + (ROW - THIN_HEIGHT) / 2, width: w, height: THIN_HEIGHT, rx: 2,
              fill: layer.color, "fill-opacity": 0.5,
            }),
          );
          return;
        }
        const y = rowY + (ROW - RIBBON_HEIGHT) / 2;
        const path = svgEl("path", {
          d: roundedPath(x, y, w, RIBBON_HEIGHT, segment.openStart ? 0 : RIBBON_RADIUS, segment.openEnd ? 0 : RIBBON_RADIUS),
          fill: layer.color,
          "fill-opacity": 0.16,
        });
        if (item.style === "dashed") {
          path.setAttribute("stroke", layer.color);
          path.setAttribute("stroke-width", "1");
          path.setAttribute("stroke-dasharray", "3 2");
        }
        group.appendChild(path);
        const barred = n === 0;
        if (barred) {
          group.appendChild(svgEl("rect", { x, y, width: BAR_WIDTH, height: RIBBON_HEIGHT, fill: layer.color }));
        }
        if (w >= MIN_TEXT_WIDTH) {
          const left = x + TEXT_PADDING + (barred ? BAR_WIDTH : 0);
          const shown = fitText(item.text, x + w - TEXT_PADDING - left, measureRibbon);
          if (shown) {
            group.appendChild(
              svgEl(
                "text",
                { x: left, y: y + 10.5, "font-size": 11, "font-family": MONO, fill: INK },
                shown,
              ),
            );
          }
        }
      });
      out.appendChild(group);
    }

    /* A short pill at one point, e.g. a cadence arrival. */
    function drawMark(out, layout, offset, layer, lane, item) {
      const spot = ScoreGeometry.xAt(layout, item.at + offset);
      const system = layout.systems[spot.system];
      const width = measureMark(item.text) + MARK_PADDING;
      const x = spot.x - width / 2;
      const y = rowTop(system, lane, 0) + (ROW - MARK_HEIGHT) / 2;
      const group = hitGroup(layer, item, ariaLabel(layer, item));
      group.appendChild(svgEl("rect", { x, y, width, height: MARK_HEIGHT, rx: MARK_HEIGHT / 2, fill: layer.color }));
      group.appendChild(
        svgEl(
          "text",
          {
            x: spot.x, y: y + 11.5, "text-anchor": "middle", "font-size": 10, "font-weight": 700,
            "font-family": MONO, fill: "#fff",
          },
          item.text,
        ),
      );
      const alt = item.claim && item.claim.alternative;
      if (alt) {
        group.appendChild(svgEl("circle", { cx: x + width - 3, cy: y + 3, r: 2, fill: "#fff" }));
        const abbr = ScoreLayers.abbreviateCadence(alt.type);
        const percent = Math.round(alt.share) / 10;
        group.appendChild(svgEl("title", {}, `Also read as ${abbr} (${percent}%)`));
      }
      out.appendChild(group);
    }

    /* "+" where a ribbon did not fit in the lane's rows. */
    function drawOverflow(out, layout, offset, layer, lane, item) {
      const spot = ScoreGeometry.xAt(layout, item.start + offset);
      const system = layout.systems[spot.system];
      const hidden = layer.items.filter(
        (other) => other.overflow && other.start <= item.start && item.start < Math.max(other.end, other.start + 1e-9),
      ).length;
      const group = hitGroup(layer, item, `${layer.label}: ${hidden} hidden ${hidden === 1 ? "item" : "items"} here`);
      group.appendChild(
        svgEl(
          "text",
          { x: spot.x, y: rowTop(system, lane, lane.rows - 1) + 13, "font-size": 13, "font-weight": 700, "font-family": MONO, fill: layer.color },
          "+",
        ),
      );
      group.appendChild(svgEl("title", {}, `${hidden} more ${hidden === 1 ? "item" : "items"} here`));
      out.appendChild(group);
    }

    /* Roman numerals below each system, left to right. A numeral that would
     * crowd the previous drawn one becomes a tick, so zooming in shows more. */
    function drawChords(out, layout, offset, layer, lane) {
      const bySystem = new Map();
      for (const item of layer.items) {
        const spot = ScoreGeometry.xAt(layout, item.at + offset);
        if (!bySystem.has(spot.system)) {
          bySystem.set(spot.system, []);
        }
        bySystem.get(spot.system).push({ item, x: spot.x });
      }
      for (const [systemIndex, entries] of bySystem) {
        const system = layout.systems[systemIndex];
        entries.sort((a, b) => a.x - b.x);
        const widths = entries.map(({ item }) => measureChord(item.text));
        const drawn = ScoreGeometry.chordLabelsToDraw(entries.map((e) => e.x), widths, CHORD_GAP);
        const rowY = rowTop(system, lane, 0);
        entries.forEach(({ item, x }, i) => {
          const group = hitGroup(layer, item, ariaLabel(layer, item));
          if (drawn[i]) {
            group.appendChild(
              svgEl("text", { x, y: rowY + 13, "font-size": 12, "font-family": MONO, fill: layer.color }, item.text),
            );
          } else {
            group.appendChild(
              svgEl("rect", {
                x: x - TICK_WIDTH / 2, y: rowY + (ROW - TICK_HEIGHT) / 2, width: TICK_WIDTH, height: TICK_HEIGHT,
                fill: layer.color,
              }),
            );
            group.appendChild(svgEl("title", {}, item.text));
          }
          out.appendChild(group);
        });
      }
    }

    function drawLane(out, layout, offset, lane, stackEntry) {
      const layer = lane.layer;
      const placed = { placement: lane.placement, offset: stackEntry.offset, rows: lane.rows };
      if (layer.kind === "mark" && lane.placement === "below") {
        drawChords(out, layout, offset, layer, placed);
        return;
      }
      for (const item of layer.items) {
        if (item.overflow) {
          drawOverflow(out, layout, offset, layer, placed, item);
        } else if (layer.kind === "ribbon") {
          drawRibbon(out, layout, offset, layer, placed, item);
        } else {
          drawMark(out, layout, offset, layer, placed, item);
        }
      }
    }

    /* Redraws everything for the enabled layers. */
    function draw(layout, offset, catalog, enabledIds) {
      clear();
      if (!layout || layout.systems.length === 0 || !place()) {
        return;
      }
      const { lanes } = planLanes(catalog, enabledIds);
      const stack = ScoreGeometry.laneStack(lanes);
      const stackEntries = new Map([...stack.above, ...stack.below].map((entry) => [entry.layerId, entry]));
      const out = document.createDocumentFragment();
      for (const lane of lanes) {
        drawLane(out, layout, offset, lane, stackEntries.get(lane.layerId));
      }
      svg.appendChild(out);
    }

    function destroy() {
      svg.remove();
    }

    return { draw, clear, destroy };
  }

  const api = { create, planLanes, fitText };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreOverlay = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
