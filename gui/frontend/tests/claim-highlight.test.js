/* Confirms how the Score tab finds a claim's notes (score-notes.js): the
 * MusicXML key builder on a two-part, two-voice fixture with a chord, a
 * tie across a barline, a <backup> and a <forward>; a fake OSMD graph (the
 * same shape OSMD 3.10.1 gives) maps every xml:id to its notes; barline
 * fragments match their base id. Plain Node script, no dependencies. Run
 * directly: `node frontend/tests/claim-highlight.test.js`. */

const fs = require("fs");
const path = require("path");
const ScoreNotes = require("../public/score-notes.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Minimal XML reader giving the element shape score-notes.js needs
 * (tagName, children, getAttribute, textContent). Enough for the fixture:
 * no CDATA, no namespaces. */
function parseXml(text) {
  const root = { tagName: "#root", children: [], attrs: {}, text: "" };
  const stack = [root];
  const tag = /<!--[\s\S]*?-->|<\?[\s\S]*?\?>|<!DOCTYPE[^>]*>|<\/([\w:-]+)\s*>|<([\w:-]+)((?:\s+[\w:-]+="[^"]*")*)\s*(\/?)>|([^<]+)/g;
  let m;
  while ((m = tag.exec(text)) !== null) {
    const top = stack[stack.length - 1];
    if (m[1]) {
      stack.pop();
    } else if (m[2]) {
      const el = { tagName: m[2], children: [], attrs: {}, text: "" };
      for (const a of m[3].matchAll(/([\w:-]+)="([^"]*)"/g)) {
        el.attrs[a[1]] = a[2];
      }
      top.children.push(el);
      if (!m[4]) {
        stack.push(el);
      }
    } else if (m[5]) {
      top.text += m[5];
    }
  }
  for (const el of [root]) {
    decorate(el);
  }
  return root.children[0];
}

function decorate(el) {
  el.getAttribute = (name) => (name in el.attrs ? el.attrs[name] : null);
  Object.defineProperty(el, "textContent", {
    get: () => el.text + el.children.map((c) => c.textContent).join(""),
  });
  el.children.forEach(decorate);
}

const xml = fs.readFileSync(path.join(__dirname, "fixtures", "multivoice.musicxml"), "utf8");
const entries = ScoreNotes.buildMusicXmlNoteKeys(parseXml(xml));
const keyOf = (id) => (entries.find((e) => e.id === id) || {}).key;

check(entries.length === 12, "12 pitched notes are keyed; the rest is skipped");
check(keyOf("m1v1c5") === "0|1|1|1|0/1|C0/5", "first note of part 0: part, measure, staff, voice, onset 0, pitch");
check(keyOf("m1v1e5") === "0|1|1|1|0/1|E0/5", "a chord note keeps the onset of the note it joins");
check(keyOf("m1v1d5") === "0|1|1|1|1/4|D0/5", "the next note starts after the chord's duration (divisions 2)");
check(keyOf("tie_a") === "0|1|1|1|1/2|E-1/5", "an alter is part of the pitch");
check(keyOf("m1v2c4") === "0|1|1|2|0/1|C0/4", "<backup> returns to the start of the measure for voice 2");
check(keyOf("m1v2b3") === "0|1|1|2|3/4|B0/3", "a rest advances the onset but is not keyed");
check(keyOf("tie_b") === "0|2|1|1|0/1|E-1/5", "the tied continuation is in measure 2 at onset 0");
check(keyOf("bass_c3") === "1|1|1|1|0/1|C0/3", "part 1 has its own index and divisions (1)");
check(keyOf("bass_c3_late") === "1|2|1|1|3/4|C0/3", "<forward> moves the onset");
check(new Set(entries.map((e) => e.key)).size === 12, "every key is distinct");

/* A GraphicalNote shaped like OSMD 3.10.1's (fields checked in a real
 * render): Pitch.FundamentalNote is a semitone value, Octave is the
 * MusicXML octave minus 3. */
function graphicalNote(part, measure, staff, voice, num, den, fundamental, accidental, octave, rest) {
  const source = {
    Pitch: rest ? undefined : { FundamentalNote: fundamental, AccidentalHalfTones: accidental, Octave: octave - 3 },
    isRest: () => Boolean(rest),
    SourceMeasure: { MeasureNumber: measure },
    ParentStaffEntry: { ParentStaff: { Id: staff, ParentInstrument: { Id: part } } },
    ParentVoiceEntry: {
      Timestamp: { Numerator: num, Denominator: den, WholeValue: 0 },
      ParentVoice: { VoiceId: voice },
    },
  };
  return { sourceNote: source };
}

const graph = {
  m1v1c5: graphicalNote(0, 1, 1, 1, 0, 1, 0, 0, 5),
  m1v1e5: graphicalNote(0, 1, 1, 1, 0, 1, 4, 0, 5),
  m1v2c4: graphicalNote(0, 1, 1, 2, 0, 1, 0, 0, 4),
  m1v1d5: graphicalNote(0, 1, 1, 1, 1, 4, 2, 0, 5),
  tie_a: graphicalNote(0, 1, 1, 1, 1, 2, 4, -1, 5),
  m1v2b3: graphicalNote(0, 1, 1, 2, 3, 4, 11, 0, 3),
  bass_c3: graphicalNote(1, 1, 1, 1, 0, 1, 0, 0, 3),
  tie_b: graphicalNote(0, 2, 1, 1, 0, 1, 4, -1, 5),
  m2v2g4: graphicalNote(0, 2, 1, 2, 0, 1, 7, 0, 4),
  m2v1f5: graphicalNote(0, 2, 1, 1, 1, 2, 5, 0, 5),
  bass_g2: graphicalNote(1, 2, 1, 1, 0, 1, 7, 0, 2),
  bass_c3_late: graphicalNote(1, 2, 1, 1, 3, 4, 0, 0, 3),
};
const rest = graphicalNote(0, 1, 1, 2, 1, 2, 0, 0, 0, true);
const rendered = [rest, ...Object.values(graph)].reverse();

let { index, unmatched } = ScoreNotes.buildNoteIndex(entries, rendered);
check(unmatched === 0, "every rendered note finds its xml:id (rests are not counted)");
check(
  Object.keys(graph).every((id) => index.get(id) && index.get(id).length === 1 && index.get(id)[0] === graph[id]),
  "every xml:id maps to its own graphical note, whatever the rendering order",
);

const stray = graphicalNote(0, 1, 1, 3, 0, 1, 0, 0, 5);
({ unmatched } = ScoreNotes.buildNoteIndex(entries, [...rendered, stray]));
check(unmatched === 1, "a rendered note whose key matches no xml:id is counted");

const sameNoteOtherVoice = graphicalNote(0, 1, 1, 2, 0, 1, 0, 0, 5);
({ unmatched } = ScoreNotes.buildNoteIndex(entries, [sameNoteOtherVoice]));
check(unmatched === 1, "the same pitch and onset in another voice is not the same note");

/* Fragments: ids as Ursatz writes a note split at a barline. */
const fragIndex = new Map([
  ["n1", ["whole"]],
  ["n1_a", ["a"]],
  ["n1_b", ["b"]],
  ["n1_2", ["two"]],
  ["n10", ["ten"]],
  ["n1_ab", ["ab"]],
]);
const found = ScoreNotes.findNotesForHighlightId(fragIndex, "n1");
check(
  found.length === 4 && ["whole", "a", "b", "two"].every((n) => found.includes(n)),
  "a base id matches its own note and its _<letter> and _<number> fragments",
);
check(!found.includes("ten") && !found.includes("ab"), "a longer or unrelated id is not a fragment");
check(ScoreNotes.findNotesForHighlightId(fragIndex, "missing").length === 0, "an id missing from the score matches nothing");

const tie = ScoreNotes.findNotesForHighlightId(ScoreNotes.buildNoteIndex(entries, rendered).index, "tie");
check(tie.length === 2 && tie.includes(graph.tie_a) && tie.includes(graph.tie_b), "a tied note across a barline highlights both halves");

process.exit(failures === 0 ? 0 : 1);
