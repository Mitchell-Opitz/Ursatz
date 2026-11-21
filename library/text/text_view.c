#include "text/text_view.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/json.h>
#include <common/strings.h>

#include <model/core/rational.h>
#include <model/pitch/midi_pitch.h>
#include <model/pitch/pitch_arithmetic.h>

#include "plan/plan_json.h"
#include "reading/reading_json.h"
#include "realization/realization_json.h"
#include "text/text_tree.h"

/* ---- rationals ------------------------------------------------------------------------------------------------------------------ */

typedef Rational Rat;

static Rat rat(long long n, long long d) {
    return rational_ratio(n, d);
}

static Rat rat_add(Rat a, Rat b) {
    Rat r = rat(0, 1);
    rational_add(a, b, &r);
    return r;
}

static Rat rat_sub(Rat a, Rat b) {
    Rat r = rat(0, 1);
    rational_sub(a, b, &r);
    return r;
}

static Rat rat_mul(Rat a, Rat b) {
    Rat r = rat(0, 1);
    rational_mul(a, b, &r);
    return r;
}

static Rat rat_div(Rat a, Rat b) {
    Rat r = rat(0, 1);
    rational_div(a, b, &r);
    return r;
}

static int rat_cmp(Rat a, Rat b) {
    return rational_cmp(a, b);
}

static bool rat_eq(Rat a, Rat b) {
    return rational_equals(a, b);
}

static long long rat_floor(Rat a) {
    long long q = 0;
    rational_floor(a, &q);
    return q;
}

static long long rat_ceil(Rat a) {
    long long q = 0;
    rational_ceil(a, &q);
    return q;
}

static bool rat_read(const cJSON *o, Rat *out) {
    long long num;
    long long den;
    if (!common_json_rational_read(o, COMMON_RATIONAL_OBJECT, &num, &den)) {
        return false;
    }
    *out = rat(num, den);
    return true;
}

static Rat rat_member(const cJSON *o, const char *name) {
    Rat r = rat(0, 1);
    rat_read(cJSON_GetObjectItemCaseSensitive(o, name), &r);
    return r;
}

/* ---- bars ----------------------------------------------------------------------------------------------------------------------- */

#define MAX_SEGS 64

typedef struct {
    Rat start; /* where the segment begins, in whole notes */
    Rat bar;   /* the length of a bar */
    Rat beat;  /* the length of a beat */
    int first; /* the number of its first bar */
} Seg;

typedef struct {
    Seg segs[MAX_SEGS];
    size_t count;
} BarMap;

static void bars_add(BarMap *m, Rat start, long long beats, long long unit) {
    if (m->count >= MAX_SEGS || beats <= 0 || unit <= 0) {
        return;
    }
    Seg s;
    s.start = start;
    s.bar = rat(beats, unit);
    s.beat = rat(1, unit);
    s.first = 1;
    if (m->count > 0) {
        Seg *p = &m->segs[m->count - 1];
        if (rat_cmp(start, p->start) <= 0) {
            return;
        }
        s.first = p->first + (int)rat_ceil(rat_div(rat_sub(start, p->start), p->bar));
    }
    m->segs[m->count++] = s;
}

static const Seg *seg_at(const BarMap *m, Rat t) {
    const Seg *s = &m->segs[0];
    for (size_t i = 1; i < m->count; i++) {
        if (rat_cmp(m->segs[i].start, t) <= 0) {
            s = &m->segs[i];
        }
    }
    return s;
}

/* The bar that holds time t (bar 1 for a time before the first). */
static int bar_of(const BarMap *m, Rat t) {
    const Seg *s = seg_at(m, t);
    if (rat_cmp(t, s->start) < 0) {
        return 1;
    }
    return s->first + (int)rat_floor(rat_div(rat_sub(t, s->start), s->bar));
}

/* The bar that holds the last instant before `end`. */
static int last_bar(const BarMap *m, Rat end) {
    const Seg *s = &m->segs[0];
    for (size_t i = 1; i < m->count; i++) {
        if (rat_cmp(m->segs[i].start, end) < 0) {
            s = &m->segs[i];
        }
    }
    if (rat_cmp(end, s->start) <= 0) {
        return s->first;
    }
    return s->first + (int)rat_ceil(rat_div(rat_sub(end, s->start), s->bar)) - 1;
}

static Rat bar_start(const BarMap *m, int bar) {
    const Seg *s = &m->segs[0];
    for (size_t i = 1; i < m->count; i++) {
        if (m->segs[i].first <= bar) {
            s = &m->segs[i];
        }
    }
    return rat_add(s->start, rat_mul(s->bar, rat(bar - s->first, 1)));
}

static Rat bar_length(const BarMap *m, int bar) {
    return seg_at(m, bar_start(m, bar))->bar;
}

/* The beat of time t in its bar, as "3" or "2.5". */
static void beat_text(const BarMap *m, Rat t, char *out, size_t size) {
    const Seg *s = seg_at(m, t);
    Rat within = rat_sub(t, bar_start(m, bar_of(m, t)));
    Rat beat = rat_add(rat(1, 1), rat_div(within, s->beat));
    if (beat.den == 1) {
        snprintf(out, size, "%lld", beat.num);
        return;
    }
    snprintf(out, size, "%.2f", rational_to_double(beat));
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '0') {
        out[--len] = '\0';
    }
    if (len > 0 && out[len - 1] == '.') {
        out[len - 1] = '\0';
    }
}

/* ---- context -------------------------------------------------------------------------------------------------------------------- */

typedef struct {
    Rat start, end;
    char text[64];
} Chord;

typedef struct {
    Rat start, end;
    char text[96];
} KeyRegion;

typedef struct {
    Rat start, end;
    char label[64];
    char id[64];
} Section;

typedef struct {
    const TextViewSource *src;
    cJSON *plan, *real, *read;
    const cJSON *movement;
    BarMap map;
    Rat end; /* the end of the piece */
    int bars;
    int lo, hi; /* the bars shown */
    bool limited;
    Chord *chords;
    size_t chord_count;
    KeyRegion *regions;
    size_t region_count;
    Section *sections;
    size_t section_count;
    char meter[16];
    TextError *error;
} Ctx;

static void fail(TextError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    error->line = 0;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof error->message, format, args);
    va_end(args);
}

static const cJSON *member(const cJSON *o, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(o, name);
}

static const char *str_member(const cJSON *o, const char *name) {
    const cJSON *m = member(o, name);
    return cJSON_IsString(m) && m->valuestring ? m->valuestring : "";
}

static double num_member(const cJSON *o, const char *name, double fallback) {
    const cJSON *m = member(o, name);
    return cJSON_IsNumber(m) ? m->valuedouble : fallback;
}

static void material_name(const Ctx *c, const char *id, char *out, size_t size) {
    const KnowledgeEntry *entry = c->src->knowledge && id[0] ? knowledge_snapshot_entry(c->src->knowledge, id) : NULL;
    if (entry && entry->name) {
        snprintf(out, size, "%s", entry->name);
        return;
    }
    const char *dot = strrchr(id, '.');
    snprintf(out, size, "%s", dot ? dot + 1 : id);
}

/* A region as text: the tonic and the material's name, or a relation of its anchor. */
static void region_text(const Ctx *c, const char *tonic, const char *material, const char *relation, const char *anchor, char *out, size_t size) {
    char name[48];
    if (relation && relation[0]) {
        material_name(c, relation, name, sizeof name);
        snprintf(out, size, "%s of %s", name, anchor && anchor[0] ? anchor : "home");
        return;
    }
    material_name(c, material, name, sizeof name);
    snprintf(out, size, "%s %s", tonic, name);
}

static void pc_region_text(const Ctx *c, int center, const char *material, char *out, size_t size) {
    region_text(c, pitch_class_name(pitch_class_of_midi(center), false), material, NULL, NULL, out, size);
}

static const cJSON *first_movement(const cJSON *real) {
    const cJSON *work = member(real, "work");
    const cJSON *movements = member(work, "movements");
    return cJSON_IsArray(movements) ? movements->child : NULL;
}

static cJSON *parse_text(char *text) {
    if (!text) {
        return NULL;
    }
    cJSON *out = cJSON_Parse(text);
    free(text);
    return out;
}

static void ctx_free(Ctx *c) {
    cJSON_Delete(c->plan);
    cJSON_Delete(c->real);
    cJSON_Delete(c->read);
    free(c->chords);
    free(c->regions);
    free(c->sections);
}

static bool add_chord(Ctx *c, Rat start, Rat end, const char *text) {
    Chord *grown = realloc(c->chords, (c->chord_count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    c->chords = grown;
    Chord *x = &c->chords[c->chord_count++];
    x->start = start;
    x->end = end;
    snprintf(x->text, sizeof x->text, "%s", text);
    return true;
}

static bool add_region(Ctx *c, Rat start, Rat end, const char *text) {
    KeyRegion *grown = realloc(c->regions, (c->region_count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    c->regions = grown;
    KeyRegion *x = &c->regions[c->region_count++];
    x->start = start;
    x->end = end;
    snprintf(x->text, sizeof x->text, "%s", text);
    return true;
}

static bool add_section(Ctx *c, Rat start, Rat end, const char *label, const char *id) {
    Section *grown = realloc(c->sections, (c->section_count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    c->sections = grown;
    Section *x = &c->sections[c->section_count++];
    x->start = start;
    x->end = end;
    snprintf(x->label, sizeof x->label, "%s", label);
    snprintf(x->id, sizeof x->id, "%s", id);
    return true;
}

static void scan_meters(Ctx *c) {
    const cJSON *spans = c->plan ? member(member(c->plan, "meter"), "spans") : NULL;
    if (cJSON_IsArray(spans) && spans->child) {
        const cJSON *s;
        cJSON_ArrayForEach(s, spans) {
            const cJSON *payload = member(member(s, "composer"), "payload");
            long long beats = (long long)num_member(payload, "beats_per_bar", 0);
            long long unit = (long long)num_member(payload, "beat_unit", 0);
            bars_add(&c->map, rat_member(member(s, "span"), "start"), beats, unit);
            if (c->meter[0] == '\0') {
                snprintf(c->meter, sizeof c->meter, "%lld/%lld", beats, unit);
            }
        }
    }
    if (c->map.count == 0 && c->movement) {
        const cJSON *meter = member(c->movement, "meter");
        const cJSON *m;
        cJSON_ArrayForEach(m, meter) {
            int beats = 0, unit = 0;
            if (sscanf(str_member(m, "time_signature"), "%d/%d", &beats, &unit) == 2) {
                bars_add(&c->map, rat_member(m, "position"), beats, unit);
                if (c->meter[0] == '\0') {
                    snprintf(c->meter, sizeof c->meter, "%d/%d", beats, unit);
                }
            }
        }
    }
    if (c->map.count == 0 && c->read) {
        const cJSON *m;
        cJSON_ArrayForEach(m, member(c->read, "meters")) {
            int beats = 0, unit = 0;
            if (sscanf(str_member(m, "time_signature"), "%d/%d", &beats, &unit) == 2) {
                bars_add(&c->map, rat_member(m, "start"), beats, unit);
                if (c->meter[0] == '\0') {
                    snprintf(c->meter, sizeof c->meter, "%d/%d", beats, unit);
                }
            }
        }
    }
}

static void max_end(Rat *end, Rat t) {
    if (rat_cmp(t, *end) > 0) {
        *end = t;
    }
}

static void scan_end(Ctx *c) {
    c->end = rat(0, 1);
    if (c->plan) {
        max_end(&c->end, rat_member(member(member(c->plan, "form"), "span"), "end"));
        return;
    }
    if (c->read) {
        static const char *const KINDS[] = {"regions", "chords", "cadences", "groups", "motive_occurrences", "meters", "lines", "voice_faults"};
        for (size_t i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++) {
            const cJSON *x;
            cJSON_ArrayForEach(x, member(c->read, KINDS[i])) {
                max_end(&c->end, rat_member(x, "end"));
            }
        }
    }
    if (c->movement && rat_cmp(c->end, rat(0, 1)) == 0) {
        static const char *const SETS[] = {"lines", "parts"};
        for (size_t i = 0; i < 2; i++) {
            const cJSON *set;
            cJSON_ArrayForEach(set, member(c->movement, SETS[i])) {
                const cJSON *n;
                cJSON_ArrayForEach(n, member(set, "notes")) {
                    max_end(&c->end, rat_add(rat_member(n, "onset"), rat_member(n, "duration")));
                }
            }
        }
    }
}

static void scan_plan(Ctx *c) {
    const cJSON *l;
    cJSON_ArrayForEach(l, member(c->plan, "regions")) {
        const cJSON *p = member(member(l, "composer"), "payload");
        char text[96];
        const char *label = str_member(l, "label");
        if (str_member(p, "relation")[0] || str_member(p, "tonic")[0]) {
            region_text(c, str_member(p, "tonic"), str_member(p, "material"), str_member(p, "relation"), str_member(p, "anchor"), text, sizeof text);
        } else {
            snprintf(text, sizeof text, "%s", label);
        }
        add_region(c, rat_member(member(l, "span"), "start"), rat_member(member(l, "span"), "end"), text);
    }
    cJSON_ArrayForEach(l, member(c->plan, "chords")) {
        const cJSON *p = member(member(l, "composer"), "payload");
        const char *label = str_member(p, "roman_numeral")[0] ? str_member(p, "roman_numeral") : str_member(l, "label");
        add_chord(c, rat_member(member(l, "span"), "start"), rat_member(member(l, "span"), "end"), label);
    }
}

static void scan_sections_plan(Ctx *c, const cJSON *node) {
    const cJSON *child;
    cJSON_ArrayForEach(child, member(node, "children")) {
        if (strcmp(str_member(child, "kind"), "section") == 0) {
            const cJSON *desc = member(member(child, "composer"), "section");
            add_section(c, rat_member(member(child, "span"), "start"), rat_member(member(child, "span"), "end"), str_member(child, "label"),
                        str_member(desc, "id"));
        }
    }
}

static void scan_read(Ctx *c) {
    const cJSON *x;
    cJSON_ArrayForEach(x, member(c->read, "regions")) {
        char text[96];
        pc_region_text(c, (int)num_member(x, "center", 0), str_member(x, "material"), text, sizeof text);
        add_region(c, rat_member(x, "start"), rat_member(x, "end"), text);
    }
    cJSON_ArrayForEach(x, member(c->read, "chords")) {
        const char *label = str_member(x, "numeral")[0] ? str_member(x, "numeral") : str_member(x, "label");
        add_chord(c, rat_member(x, "start"), rat_member(x, "end"), label);
    }
    cJSON_ArrayForEach(x, member(c->read, "groups")) {
        if (strcmp(str_member(x, "kind"), "section") == 0) {
            add_section(c, rat_member(x, "start"), rat_member(x, "end"), str_member(x, "label"), "");
        }
    }
}

/* The home region as text, "" when no layer has one. */
static void home_text(const Ctx *c, char *out, size_t size) {
    out[0] = '\0';
    if (c->region_count > 0) {
        snprintf(out, size, "%s", c->regions[0].text);
        return;
    }
    const cJSON *home = c->movement ? member(c->movement, "home") : NULL;
    if (cJSON_IsObject(home)) {
        pc_region_text(c, (int)num_member(home, "center", 0), str_member(home, "material"), out, size);
    }
}

static void region_at(const Ctx *c, Rat t, char *out, size_t size) {
    out[0] = '\0';
    for (size_t i = 0; i < c->region_count; i++) {
        if (rat_cmp(c->regions[i].start, t) <= 0 && rat_cmp(t, c->regions[i].end) < 0) {
            snprintf(out, size, "%s", c->regions[i].text);
            return;
        }
    }
}

/* ---- limits --------------------------------------------------------------------------------------------------------------------- */

static bool in_limit(const Ctx *c, Rat start, Rat end) {
    if (!c->limited) {
        return true;
    }
    int a = bar_of(&c->map, start);
    int b = last_bar(&c->map, rat_cmp(end, start) > 0 ? end : rat_add(start, rat(1, 1000000)));
    return b >= c->lo && a <= c->hi;
}

static bool bar_shown(const Ctx *c, int bar) {
    return bar >= c->lo && bar <= c->hi;
}

static bool set_limits(Ctx *c, const TextViewLimits *limits) {
    c->lo = 1;
    c->hi = c->bars;
    c->limited = false;
    if (!limits) {
        return true;
    }
    if (limits->first_measure > 0 || limits->last_measure > 0) {
        int a = limits->first_measure > 0 ? limits->first_measure : 1;
        int b = limits->last_measure > 0 ? limits->last_measure : c->bars;
        if (a > b || a < 1 || b > c->bars) {
            fail(c->error, "measures %d-%d are outside the piece (bars 1-%d)", a, b, c->bars);
            return false;
        }
        c->lo = a;
        c->hi = b;
        c->limited = true;
    }
    if (limits->section && limits->section[0]) {
        const Section *found = NULL;
        for (size_t i = 0; i < c->section_count && !found; i++) {
            if (strcmp(c->sections[i].label, limits->section) == 0 || (c->sections[i].id[0] && strcmp(c->sections[i].id, limits->section) == 0)) {
                found = &c->sections[i];
            }
        }
        if (!found) {
            fail(c->error, "unknown section '%s'", limits->section);
            return false;
        }
        int a = bar_of(&c->map, found->start);
        int b = last_bar(&c->map, found->end);
        c->lo = c->limited && c->lo > a ? c->lo : a;
        c->hi = c->limited && c->hi < b ? c->hi : b;
        if (c->lo > c->hi) {
            fail(c->error, "measures %d-%d are outside the piece (bars 1-%d)", c->lo, c->hi, c->bars);
            return false;
        }
        c->limited = true;
    }
    return true;
}

/* ---- notes ---------------------------------------------------------------------------------------------------------------------- */

typedef struct {
    Rat on, dur;
    int midi;
    char spelling[8];
    int staff, voice;
} Note;

typedef struct {
    Note *items;
    size_t count;
} Notes;

static bool notes_add(Notes *n, Note note) {
    Note *grown = realloc(n->items, (n->count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    n->items = grown;
    n->items[n->count++] = note;
    return true;
}

static int note_cmp(const void *a, const void *b) {
    const Note *x = a, *y = b;
    int c = rat_cmp(x->on, y->on);
    if (c) {
        return c;
    }
    return x->midi - y->midi;
}

/* The name of a note: its spelling when it has one, else the sharp name, and the octave (MIDI 60 is C4). */
static void note_name(const Note *n, char *out, size_t size) {
    PitchLetter letter;
    int accidental = 0;
    if (n->spelling[0] && pitch_spelling_parse_name(n->spelling, &letter, &accidental, NULL)) {
        int pitch_class_offset = pitch_letter_semitone(letter) + accidental;
        snprintf(out, size, "%s%d", n->spelling, (n->midi - pitch_class_offset) / 12 - 1);
        return;
    }
    pitch_name_of_midi(n->midi, false, out, size);
}

/* A duration: w h q e s t, with one or two dots or a triplet 3, else n/d of a whole note. */
static void duration_text(Rat d, char *out, size_t size) {
    static const char LETTERS[] = {'w', 'h', 'q', 'e', 's', 't'};
    Rat base = rat(1, 1);
    for (size_t i = 0; i < sizeof LETTERS; i++, base = rat_div(base, rat(2, 1))) {
        if (rat_eq(d, base)) {
            snprintf(out, size, "%c", LETTERS[i]);
            return;
        }
        if (rat_eq(d, rat_mul(base, rat(3, 2)))) {
            snprintf(out, size, "%c.", LETTERS[i]);
            return;
        }
        if (rat_eq(d, rat_mul(base, rat(7, 4)))) {
            snprintf(out, size, "%c..", LETTERS[i]);
            return;
        }
        if (rat_eq(d, rat_mul(base, rat(2, 3)))) {
            snprintf(out, size, "%c3", LETTERS[i]);
            return;
        }
    }
    snprintf(out, size, "%lld/%lld", d.num, d.den);
}

static void append_rest(CommonText *out, Rat d) {
    char dur[32];
    duration_text(d, dur, sizeof dur);
    common_text_appendf(out, "r:%s", dur);
}

/* The notes of one bar of a track, in the notes syntax: onsets in the bar, rests between, a chord's notes joined by '+'. */
static void bar_tokens(const BarMap *m, const Notes *notes, int bar, CommonText *out) {
    Rat start = bar_start(m, bar);
    Rat end = rat_add(start, bar_length(m, bar));
    Rat cursor = start;
    bool any = false;
    size_t i = 0;
    while (i < notes->count && rat_cmp(notes->items[i].on, start) < 0) {
        Rat e = rat_add(notes->items[i].on, notes->items[i].dur);
        if (rat_cmp(e, cursor) > 0) {
            cursor = e;
        }
        i++;
    }
    bool through = rat_cmp(cursor, start) > 0;
    for (; i < notes->count && rat_cmp(notes->items[i].on, end) < 0;) {
        size_t j = i;
        Rat on = notes->items[i].on;
        while (j < notes->count && rat_eq(notes->items[j].on, on)) {
            j++;
        }
        if (any) {
            common_text_append(out, " ");
        }
        if (rat_cmp(on, cursor) > 0) {
            append_rest(out, rat_sub(on, cursor));
            common_text_append(out, " ");
        }
        bool same = true;
        for (size_t k = i + 1; k < j; k++) {
            same = same && rat_eq(notes->items[k].dur, notes->items[i].dur);
        }
        Rat slot_end = on;
        for (size_t k = i; k < j; k++) {
            char name[24], dur[32];
            note_name(&notes->items[k], name, sizeof name);
            duration_text(notes->items[k].dur, dur, sizeof dur);
            if (k > i) {
                common_text_append(out, "+");
            }
            common_text_append(out, name);
            if (!same) {
                common_text_appendf(out, ":%s", dur);
            }
            Rat e = rat_add(notes->items[k].on, notes->items[k].dur);
            if (rat_cmp(e, slot_end) > 0) {
                slot_end = e;
            }
        }
        if (same) {
            char dur[32];
            duration_text(notes->items[i].dur, dur, sizeof dur);
            common_text_appendf(out, ":%s", dur);
        }
        if (rat_cmp(slot_end, cursor) > 0) {
            cursor = slot_end;
        }
        any = true;
        i = j;
    }
    if (!any) {
        if (through) {
            common_text_append(out, "-");
        } else {
            append_rest(out, rat_sub(end, start));
        }
        return;
    }
    if (rat_cmp(cursor, end) < 0) {
        common_text_append(out, " ");
        append_rest(out, rat_sub(end, cursor));
    }
}

static bool load_notes(const cJSON *list, bool with_spelling, Notes *out) {
    const cJSON *n;
    cJSON_ArrayForEach(n, list) {
        Note note;
        memset(&note, 0, sizeof note);
        note.on = rat_member(n, "onset");
        note.dur = rat_member(n, "duration");
        note.midi = (int)num_member(n, "midi", 60);
        if (with_spelling) {
            snprintf(note.spelling, sizeof note.spelling, "%s", str_member(n, "spelling"));
        }
        note.staff = (int)num_member(n, "staff", 1);
        note.voice = (int)num_member(n, "voice", 1);
        if (!notes_add(out, note)) {
            return false;
        }
    }
    return true;
}

/* ---- the views ------------------------------------------------------------------------------------------------------------------ */

static const char *tail_of(const char *address) {
    return address ? address : "";
}

static char *frame_for(const Ctx *c, const char *view, CommonText *body, const char *leaves, const char *expand) {
    char leaves_out[256];
    if (c->limited) {
        snprintf(leaves_out, sizeof leaves_out, "%s, bars outside %d-%d", leaves, c->lo, c->hi);
    } else {
        snprintf(leaves_out, sizeof leaves_out, "%s", leaves);
    }
    char *text = common_text_take(body);
    char *framed = text ? text_view_frame(view, tail_of(c->src->address), text, leaves_out, expand) : NULL;
    free(text);
    return framed;
}

static char *view_summary(Ctx *c) {
    CommonText out;
    common_text_init(&out);
    char home[96];
    home_text(c, home, sizeof home);
    common_text_appendf(&out, "%s", tail_of(c->src->address));
    if (c->plan) {
        const cJSON *form = member(member(member(c->plan, "form"), "composer"), "form");
        if (str_member(form, "name")[0]) {
            common_text_appendf(&out, "  form %s", str_member(form, "name"));
        }
    }
    if (home[0]) {
        common_text_appendf(&out, "  home %s", home);
    }
    if (c->meter[0]) {
        common_text_appendf(&out, "  %s", c->meter);
    }
    common_text_appendf(&out, "  bars 1-%d\n", c->bars);

    for (size_t i = 0; i < c->section_count; i++) {
        const Section *s = &c->sections[i];
        if (!in_limit(c, s->start, s->end)) {
            continue;
        }
        const cJSON *desc = NULL;
        if (c->plan) {
            const cJSON *node;
            cJSON_ArrayForEach(node, member(member(c->plan, "form"), "children")) {
                if (strcmp(str_member(node, "kind"), "section") == 0 && rat_eq(rat_member(member(node, "span"), "start"), s->start) &&
                    strcmp(str_member(node, "label"), s->label) == 0) {
                    desc = member(member(node, "composer"), "section");
                    break;
                }
            }
        }
        int a = bar_of(&c->map, s->start), b = last_bar(&c->map, s->end);
        common_text_appendf(&out, "  %s  bars %d-%d", s->label, a, b);
        if (str_member(desc, "role")[0]) {
            common_text_appendf(&out, "  role %s", str_member(desc, "role"));
        }
        if (str_member(desc, "letter")[0]) {
            common_text_appendf(&out, "  letter %s", str_member(desc, "letter"));
        }
        if (str_member(desc, "function")[0] && strcmp(str_member(desc, "function"), "none") != 0) {
            common_text_appendf(&out, "  function %s", str_member(desc, "function"));
        }
        char region[96];
        region_at(c, s->start, region, sizeof region);
        if (region[0] && strcmp(region, home) != 0) {
            common_text_appendf(&out, "  region %s", region);
        }
        /* the numerals of the section's bars */
        CommonText numerals;
        common_text_init(&numerals);
        bool any = false;
        for (int bar = a; bar <= b; bar++) {
            Rat bs = bar_start(&c->map, bar), be = rat_add(bs, bar_length(&c->map, bar));
            bool first = true;
            if (bar > a) {
                common_text_append(&numerals, " | ");
            }
            for (size_t k = 0; k < c->chord_count; k++) {
                if (rat_cmp(c->chords[k].start, bs) >= 0 && rat_cmp(c->chords[k].start, be) < 0) {
                    common_text_appendf(&numerals, "%s%s", first ? "" : " ", c->chords[k].text);
                    first = false;
                    any = true;
                }
            }
            if (first) {
                common_text_append(&numerals, "-");
            }
        }
        if (any) {
            char *t = common_text_take(&numerals);
            common_text_appendf(&out, "  harmony %s", t ? t : "");
            free(t);
        } else {
            common_text_free(&numerals);
        }
        common_text_append(&out, "\n");
    }

    if (c->movement) {
        const cJSON *lines = member(c->movement, "lines");
        if (cJSON_IsArray(lines) && lines->child) {
            common_text_append(&out, "lines");
            const cJSON *l;
            cJSON_ArrayForEach(l, lines) {
                common_text_appendf(&out, "%s%s", l == lines->child ? " " : " | ", str_member(l, "id"));
            }
            common_text_append(&out, "\n");
        }
        const cJSON *parts = member(c->movement, "parts");
        const cJSON *p;
        cJSON_ArrayForEach(p, parts) {
            common_text_appendf(&out, "parts %s", str_member(p, "id"));
            bool first = true;
            const cJSON *a;
            cJSON_ArrayForEach(a, member(c->movement, "assignments")) {
                if (strcmp(str_member(a, "part"), str_member(p, "id")) == 0) {
                    common_text_appendf(&out, "%s %s", first ? ":" : "", str_member(a, "line"));
                    first = false;
                }
            }
            common_text_append(&out, "\n");
        }
    }
    return frame_for(c, "summary", &out, "notes, constraints, ids, spans (--view full)", "ursatz show <address>/<section> --view structure");
}

static void structure_node(Ctx *c, const cJSON *node, int depth, CommonText *out) {
    Rat start = rat_member(member(node, "span"), "start"), end = rat_member(member(node, "span"), "end");
    if (!in_limit(c, start, end)) {
        return;
    }
    for (int i = 0; i < depth; i++) {
        common_text_append(out, "  ");
    }
    common_text_appendf(out, "%s", str_member(node, "kind"));
    if (str_member(node, "label")[0]) {
        common_text_appendf(out, " %s", str_member(node, "label"));
    }
    common_text_appendf(out, "  bars %d-%d", bar_of(&c->map, start), last_bar(&c->map, end));
    const cJSON *desc = member(member(node, "composer"), "section");
    const cJSON *m;
    cJSON_ArrayForEach(m, desc) {
        if (!m->string || strcmp(m->string, "id") == 0) {
            continue;
        }
        char *v = text_scalar_print(m);
        if (v) {
            common_text_appendf(out, "  %s %s", m->string, v);
            free(v);
        }
    }
    common_text_append(out, "\n");
    const cJSON *child;
    cJSON_ArrayForEach(child, member(node, "children")) {
        structure_node(c, child, depth + 1, out);
    }
}

typedef struct {
    int parent;
    int index;
} GroupLink;

static void structure_group(Ctx *c, const cJSON *groups, int index, int depth, CommonText *out) {
    const cJSON *g = cJSON_GetArrayItem(groups, index);
    Rat start = rat_member(g, "start"), end = rat_member(g, "end");
    if (!in_limit(c, start, end)) {
        return;
    }
    for (int i = 0; i < depth; i++) {
        common_text_append(out, "  ");
    }
    common_text_appendf(out, "%s", str_member(g, "kind"));
    if (str_member(g, "label")[0]) {
        common_text_appendf(out, " %s", str_member(g, "label"));
    }
    common_text_appendf(out, "  bars %d-%d", bar_of(&c->map, start), last_bar(&c->map, end));
    common_text_append(out, "\n");
    int n = cJSON_GetArraySize(groups);
    for (int k = index + 1; k < n; k++) {
        const cJSON *child = cJSON_GetArrayItem(groups, k);
        const cJSON *parent = member(child, "parent");
        if (cJSON_IsNumber(parent) && (int)parent->valuedouble == index) {
            structure_group(c, groups, k, depth + 1, out);
        }
    }
}

static char *view_structure(Ctx *c) {
    CommonText out;
    common_text_init(&out);
    if (c->plan) {
        structure_node(c, member(c->plan, "form"), 0, &out);
    } else {
        const cJSON *groups = member(c->read, "groups");
        int n = cJSON_GetArraySize(groups);
        for (int i = 0; i < n; i++) {
            const cJSON *parent = member(cJSON_GetArrayItem(groups, i), "parent");
            if (!cJSON_IsNumber(parent)) {
                structure_group(c, groups, i, 0, &out);
            }
        }
    }
    return frame_for(c, "structure", &out, "lanes, notes, constraints, ids (--view full)", "ursatz show <address> --view harmony");
}

static char *view_harmony(Ctx *c) {
    CommonText out;
    common_text_init(&out);
    for (int bar = c->lo; bar <= c->hi; bar++) {
        Rat bs = bar_start(&c->map, bar), be = rat_add(bs, bar_length(&c->map, bar));
        for (size_t r = 0; r < c->region_count; r++) {
            int rb = bar_of(&c->map, c->regions[r].start);
            if (rb == bar || (bar == c->lo && rb < c->lo && rat_cmp(c->regions[r].end, bs) > 0)) {
                common_text_appendf(&out, "key %s from bar %d\n", c->regions[r].text, rb);
            }
        }
        common_text_appendf(&out, "bar %d ", bar);
        bool first = true;
        for (size_t k = 0; k < c->chord_count; k++) {
            if (rat_cmp(c->chords[k].start, bs) >= 0 && rat_cmp(c->chords[k].start, be) < 0) {
                char beat[24] = "";
                if (rat_cmp(c->chords[k].start, bs) > 0) {
                    char b[16];
                    beat_text(&c->map, c->chords[k].start, b, sizeof b);
                    snprintf(beat, sizeof beat, "@%s", b);
                }
                common_text_appendf(&out, "%s%s%s", first ? " " : " ", c->chords[k].text, beat);
                first = false;
            }
        }
        if (first) {
            common_text_append(&out, " -");
        }
        common_text_append(&out, "\n");
    }
    return frame_for(c, "harmony", &out, "notes, chord spellings, functions (--view full)", "ursatz show <address> --view line:melody");
}

/* The notes of a plan lane named "melody", "bass" or "inner-<n>". */
static bool plan_lane_notes(const cJSON *plan, const char *name, Notes *out, bool *known) {
    const char *lane = name;
    int voice = 0;
    if (strncmp(name, "inner-", 6) == 0) {
        lane = "inner_voice";
        voice = atoi(name + 6);
    } else if (strcmp(name, "melody") != 0 && strcmp(name, "bass") != 0) {
        return true;
    }
    static const char *const SETS[] = {"melody", "texture"};
    for (size_t i = 0; i < 2; i++) {
        const cJSON *span;
        cJSON_ArrayForEach(span, member(plan, SETS[i])) {
            const cJSON *composer = member(span, "composer");
            if (strcmp(str_member(composer, "lane"), lane) != 0 || (voice && (int)num_member(composer, "voice", 0) != voice)) {
                continue;
            }
            *known = true;
            Rat start = rat_member(member(span, "span"), "start");
            const cJSON *n;
            cJSON_ArrayForEach(n, member(member(composer, "payload"), "notes")) {
                Note note;
                memset(&note, 0, sizeof note);
                note.on = rat_add(start, rat_member(n, "offset"));
                note.dur = rat_member(n, "duration");
                note.midi = (int)num_member(n, "midi_pitch", 60);
                note.staff = note.voice = 1;
                if (!notes_add(out, note)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static char *view_line(Ctx *c, const char *name) {
    Notes notes = {NULL, 0};
    bool known = false;
    const cJSON *line = NULL;
    if (c->movement) {
        const cJSON *l;
        cJSON_ArrayForEach(l, member(c->movement, "lines")) {
            if (strcmp(str_member(l, "id"), name) == 0) {
                line = l;
                break;
            }
        }
    }
    if (line) {
        known = true;
        load_notes(member(line, "notes"), true, &notes);
    } else if (c->plan) {
        plan_lane_notes(c->plan, name, &notes, &known);
    }
    if (!known) {
        fail(c->error, "unknown line '%s'", name);
        free(notes.items);
        return NULL;
    }
    qsort(notes.items, notes.count, sizeof *notes.items, note_cmp);
    CommonText out;
    common_text_init(&out);
    for (int bar = c->lo; bar <= c->hi; bar++) {
        common_text_appendf(&out, "bar %d  ", bar);
        bar_tokens(&c->map, &notes, bar, &out);
        common_text_append(&out, "\n");
    }
    free(notes.items);
    char view[96];
    snprintf(view, sizeof view, "line:%s", name);
    return frame_for(c, view, &out, "ids, derivations, constraints (--view full)", "ursatz show <address> --view part:<name>");
}

static char *view_part(Ctx *c, const char *name) {
    const cJSON *part = NULL;
    const cJSON *parts = c->movement ? member(c->movement, "parts") : NULL;
    const cJSON *p;
    cJSON_ArrayForEach(p, parts) {
        if (strcmp(str_member(p, "id"), name) == 0) {
            part = p;
            break;
        }
    }
    if (!part) {
        fail(c->error, "unknown part '%s'", name);
        return NULL;
    }
    Notes all = {NULL, 0};
    load_notes(member(part, "notes"), true, &all);
    qsort(all.items, all.count, sizeof *all.items, note_cmp);
    /* the staff and voice pairs the part holds, in order */
    int pairs[256][2];
    size_t pair_count = 0;
    for (size_t i = 0; i < all.count; i++) {
        bool seen = false;
        for (size_t k = 0; k < pair_count; k++) {
            seen = seen || (pairs[k][0] == all.items[i].staff && pairs[k][1] == all.items[i].voice);
        }
        if (!seen && pair_count < 256) {
            pairs[pair_count][0] = all.items[i].staff;
            pairs[pair_count][1] = all.items[i].voice;
            pair_count++;
        }
    }
    for (size_t a = 0; a + 1 < pair_count; a++) {
        for (size_t b = a + 1; b < pair_count; b++) {
            if (pairs[b][0] < pairs[a][0] || (pairs[b][0] == pairs[a][0] && pairs[b][1] < pairs[a][1])) {
                int t0 = pairs[a][0], t1 = pairs[a][1];
                pairs[a][0] = pairs[b][0];
                pairs[a][1] = pairs[b][1];
                pairs[b][0] = t0;
                pairs[b][1] = t1;
            }
        }
    }
    CommonText out;
    common_text_init(&out);
    for (int bar = c->lo; bar <= c->hi; bar++) {
        common_text_appendf(&out, "bar %d\n", bar);
        Rat bs = bar_start(&c->map, bar), be = rat_add(bs, bar_length(&c->map, bar));
        for (size_t k = 0; k < pair_count; k++) {
            Notes track = {NULL, 0};
            bool onset = false;
            for (size_t i = 0; i < all.count; i++) {
                if (all.items[i].staff == pairs[k][0] && all.items[i].voice == pairs[k][1]) {
                    notes_add(&track, all.items[i]);
                    onset = onset || (rat_cmp(all.items[i].on, bs) >= 0 && rat_cmp(all.items[i].on, be) < 0);
                }
            }
            if (onset) {
                common_text_appendf(&out, "  S%d V%d  ", pairs[k][0], pairs[k][1]);
                bar_tokens(&c->map, &track, bar, &out);
                common_text_append(&out, "\n");
            }
            free(track.items);
        }
    }
    free(all.items);
    char view[96];
    snprintf(view, sizeof view, "part:%s", name);
    return frame_for(c, view, &out, "ids, derivations, flags, pedals (--view full)", "ursatz show <address> --view reading");
}

/* ---- the reading view ----------------------------------------------------------------------------------------------------------- */

typedef struct {
    int bar;
    int order; /* region, group, cadence, chord, motive, meter, line, fault */
    int seq;   /* document order within its kind (groups: outer first) */
    char text[160];
} Claim;

static bool claim_add(Claim **claims, size_t *count, Claim c) {
    Claim *grown = realloc(*claims, (*count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    *claims = grown;
    (*claims)[(*count)++] = c;
    return true;
}

static int claim_cmp(const void *a, const void *b) {
    const Claim *x = a, *y = b;
    if (x->bar != y->bar) {
        return x->bar - y->bar;
    }
    if (x->order != y->order) {
        return x->order - y->order;
    }
    return x->seq - y->seq;
}

static void confidence_text(const cJSON *claim, char *out, size_t size) {
    out[0] = '\0';
    Rat r;
    if (rat_read(member(claim, "confidence"), &r)) {
        snprintf(out, size, "  c=%.2f", rational_to_double(r));
    }
}

static int group_depth(const cJSON *groups, const cJSON *g) {
    int depth = 0;
    const cJSON *parent = member(g, "parent");
    while (cJSON_IsNumber(parent) && depth < 32) {
        depth++;
        parent = member(cJSON_GetArrayItem(groups, (int)parent->valuedouble), "parent");
    }
    return depth;
}

static char *view_reading(Ctx *c) {
    Claim *claims = NULL;
    size_t count = 0;
    const cJSON *x;
    int seq = 0;
    char conf[24], b1[16], b2[16];
    cJSON_ArrayForEach(x, member(c->read, "regions")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 0, seq++, ""};
        char region[96];
        pc_region_text(c, (int)num_member(x, "center", 0), str_member(x, "material"), region, sizeof region);
        confidence_text(x, conf, sizeof conf);
        snprintf(k.text, sizeof k.text, "key %s  bars %d-%d%s", region, k.bar, last_bar(&c->map, rat_member(x, "end")), conf);
        claim_add(&claims, &count, k);
    }
    const cJSON *groups = member(c->read, "groups");
    cJSON_ArrayForEach(x, groups) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 1, group_depth(groups, x) * 1000 + seq++, ""};
        confidence_text(x, conf, sizeof conf);
        snprintf(k.text, sizeof k.text, "%s%s%s  bars %d-%d%s", str_member(x, "kind"), str_member(x, "label")[0] ? " " : "", str_member(x, "label"),
                 k.bar, last_bar(&c->map, rat_member(x, "end")), conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "cadences")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 2, seq++, ""};
        confidence_text(x, conf, sizeof conf);
        beat_text(&c->map, rat_member(x, "arrival"), b1, sizeof b1);
        snprintf(k.text, sizeof k.text, "cadence %s  @%s%s", str_member(x, "type"), b1, conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "chords")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 3, seq++, ""};
        const char *label = str_member(x, "numeral")[0] ? str_member(x, "numeral") : str_member(x, "label");
        confidence_text(x, conf, sizeof conf);
        beat_text(&c->map, rat_member(x, "start"), b1, sizeof b1);
        snprintf(k.text, sizeof k.text, "chord %s  @%s%s", label, b1, conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "motive_occurrences")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 4, seq++, ""};
        confidence_text(x, conf, sizeof conf);
        beat_text(&c->map, rat_member(x, "start"), b1, sizeof b1);
        beat_text(&c->map, rat_member(x, "end"), b2, sizeof b2);
        snprintf(k.text, sizeof k.text, "motive %d %s  @%s-@%s%s", (int)num_member(x, "motive", 0), str_member(x, "kind"), b1, b2, conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "meters")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 5, seq++, ""};
        confidence_text(x, conf, sizeof conf);
        snprintf(k.text, sizeof k.text, "meter %s%s", str_member(x, "time_signature"), conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "lines")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 6, seq++, ""};
        confidence_text(x, conf, sizeof conf);
        snprintf(k.text, sizeof k.text, "line %s  bars %d-%d%s", str_member(x, "role"), k.bar, last_bar(&c->map, rat_member(x, "end")), conf);
        claim_add(&claims, &count, k);
    }
    cJSON_ArrayForEach(x, member(c->read, "voice_faults")) {
        Claim k = {bar_of(&c->map, rat_member(x, "start")), 7, seq++, ""};
        confidence_text(x, conf, sizeof conf);
        beat_text(&c->map, rat_member(x, "start"), b1, sizeof b1);
        snprintf(k.text, sizeof k.text, "fault %s  @%s%s", str_member(x, "rule"), b1, conf);
        claim_add(&claims, &count, k);
    }
    qsort(claims, count, sizeof *claims, claim_cmp);
    CommonText out;
    common_text_init(&out);
    int shown = 0;
    for (size_t i = 0; i < count; i++) {
        if (!bar_shown(c, claims[i].bar)) {
            continue;
        }
        if (shown != claims[i].bar) {
            common_text_appendf(&out, "bar %d\n", claims[i].bar);
            shown = claims[i].bar;
        }
        common_text_appendf(&out, "  %s\n", claims[i].text);
    }
    free(claims);
    return frame_for(c, "reading", &out, "evidence, ids, chord detail (--view full)", "ursatz show <address> --view harmony");
}

/* ---- entry points --------------------------------------------------------------------------------------------------------------- */

char *text_view_frame(const char *view, const char *address, const char *body, const char *leaves_out, const char *expand) {
    CommonText out;
    common_text_init(&out);
    common_text_appendf(&out, "ursatz-text %d view %s %s\n", URSATZ_TEXT_GRAMMAR_VERSION, view ? view : "", address ? address : "");
    if (body && body[0]) {
        common_text_append(&out, body);
        if (body[strlen(body) - 1] != '\n') {
            common_text_append(&out, "\n");
        }
    }
    common_text_appendf(&out, "# leaves out: %s\n", leaves_out && leaves_out[0] ? leaves_out : "nothing");
    if (expand && expand[0]) {
        common_text_appendf(&out, "# expand: %s\n", expand);
    } else {
        common_text_appendf(&out, "# expand: ursatz show %s --view full\n", address ? address : "<address>");
    }
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}

static bool load_layers(Ctx *c) {
    const TextViewSource *s = c->src;
    if (s->plan) {
        PlanProducer producer;
        memset(&producer, 0, sizeof producer);
        PlanJsonError error = {{0}, {0}, 0};
        c->plan = parse_text(plan_to_json(s->plan, &producer, &error));
        if (!c->plan) {
            fail(c->error, "the plan cannot be read: %s", error.message);
            return false;
        }
    }
    if (s->realization) {
        RealizationJsonError error = {{0}, {0}};
        c->real = parse_text(realization_work_to_json(s->realization, &error));
        if (!c->real) {
            fail(c->error, "the realization cannot be read: %s", error.message);
            return false;
        }
        c->movement = first_movement(c->real);
    }
    if (s->reading) {
        ReadingJsonError error = {{0}, {0}};
        c->read = parse_text(reading_to_json(s->reading, &error));
        if (!c->read) {
            fail(c->error, "the reading cannot be read: %s", error.message);
            return false;
        }
    }
    return true;
}

char *text_view_print(const TextViewSource *source, const char *view, const TextViewLimits *limits, TextError *error) {
    if (!source || !view) {
        fail(error, "unknown view ''");
        return NULL;
    }
    bool is_line = strncmp(view, "line:", 5) == 0, is_part = strncmp(view, "part:", 5) == 0;
    bool plan_or_reading = strcmp(view, "summary") == 0 || strcmp(view, "structure") == 0 || strcmp(view, "harmony") == 0;
    if (!plan_or_reading && strcmp(view, "reading") != 0 && !is_line && !is_part) {
        fail(error, "unknown view '%s'", view);
        return NULL;
    }
    if (plan_or_reading && !source->plan && !source->reading) {
        fail(error, "%s needs a plan or a reading", view);
        return NULL;
    }
    if (is_line && !source->realization && !source->plan) {
        fail(error, "%s needs a realization or a plan", view);
        return NULL;
    }
    if (is_part && !source->realization) {
        fail(error, "%s needs a realization", view);
        return NULL;
    }
    if (strcmp(view, "reading") == 0 && !source->reading) {
        fail(error, "reading needs a reading");
        return NULL;
    }
    Ctx c;
    memset(&c, 0, sizeof c);
    c.src = source;
    c.error = error;
    char *out = NULL;
    if (load_layers(&c)) {
        scan_meters(&c);
        if (c.map.count == 0) {
            bars_add(&c.map, rat(0, 1), 4, 4);
        }
        scan_end(&c);
        c.bars = last_bar(&c.map, c.end);
        if (c.bars < 1) {
            c.bars = 1;
        }
        if (c.plan) {
            scan_plan(&c);
            scan_sections_plan(&c, member(c.plan, "form"));
        } else if (c.read) {
            scan_read(&c);
        }
        if (!c.plan && c.read) {
            /* a reading with a plan's lanes absent: the chords and regions are the reading's (scan_read) */
        }
        if (set_limits(&c, limits)) {
            if (strcmp(view, "summary") == 0) {
                out = view_summary(&c);
            } else if (strcmp(view, "structure") == 0) {
                out = view_structure(&c);
            } else if (strcmp(view, "harmony") == 0) {
                out = view_harmony(&c);
            } else if (is_line) {
                out = view_line(&c, view + 5);
            } else if (is_part) {
                out = view_part(&c, view + 5);
            } else {
                out = view_reading(&c);
            }
        }
    }
    ctx_free(&c);
    return out;
}
