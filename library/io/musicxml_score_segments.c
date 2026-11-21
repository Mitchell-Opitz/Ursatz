#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/events/note_event.h"
#include "io/musicxml_export.h"
#include "io/score_layout.h"
#include "io/score_clefs.h"
#include "io/score_quantize.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_spelling.h"
#include "model/events/dynamics.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/metronome_marking.h"
#include "model/time/tempo_map.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"
#include "io/musicxml_score_internal.h"

static const char *const BEAM_NAMES[] = {NULL, "begin", "continue", "end", "forward hook", "backward hook"};

bool musicxml_score_segment_push(SegmentList *list, Segment segment) {
    if (list->count == list->cap) {
        size_t cap = list->cap == 0 ? 16 : list->cap * 2;
        Segment *grown = (Segment *)realloc(list->items, cap * sizeof(Segment));
        if (grown == NULL) {
            return false;
        }
        list->items = grown;
        list->cap = cap;
    }
    list->items[list->count++] = segment;
    return true;
}

/* The first cut strictly after tick, or LLONG_MAX when there is none. */
long long musicxml_score_next_cut(const long long *cuts, size_t count, long long tick) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (cuts[mid] > tick) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return low < count ? cuts[low] : LLONG_MAX;
}

/* --- notated values: which lengths a note or rest can be written as --- */

#define VALUE_TYPE_COUNT 8     /* whole to 128th */

#define TRIPLET_TYPE_COUNT 7   /* whole to 64th */

#define MAX_VALUE_DOTS 2

static const char *const VALUE_TYPE_NAMES[VALUE_TYPE_COUNT] = {"whole", "half", "quarter", "eighth",
                                                               "16th",  "32nd", "64th",    "128th"};

/* A dotted value is base * (2^(d+1) - 1) / 2^d. */
static const long long DOT_NUMERATOR[MAX_VALUE_DOTS + 1] = {1, 3, 7};

static const long long DOT_DENOMINATOR[MAX_VALUE_DOTS + 1] = {1, 2, 4};

/* The value a length of ticks is written as, false when it cannot be written alone. A plain value wins over a triplet
   one of the same length; triplets only when allowed (a triplet value never crosses a beat). */
static bool classify_value(long long length, long long divisions, bool allow_triplet, NotatedValue *out) {
    long long whole = 4 * divisions;
    if (length <= 0 || length > 2 * whole) {
        return false;
    }
    for (int k = 0; k < VALUE_TYPE_COUNT; k++) {
        for (int d = 0; d <= MAX_VALUE_DOTS; d++) {
            if (length * (1LL << k) * DOT_DENOMINATOR[d] == whole * DOT_NUMERATOR[d]) {
                out->type = k + 1;
                out->dots = d;
                out->tuplet = false;
                out->actual = 0;
                out->normal = 0;
                return true;
            }
        }
    }
    if (allow_triplet) {
        for (int k = 0; k < TRIPLET_TYPE_COUNT; k++) {
            if (length * 3 * (1LL << k) == 2 * whole) {
                out->type = k + 1;
                out->dots = 0;
                out->tuplet = true;
                out->actual = 3;
                out->normal = 2;
                return true;
            }
        }
    }
    return false;
}

/* The value a length of ticks is written as under a notated ratio: its value is length * actual / normal, plain or
   dotted. False when that is not one. */
static bool classify_tuplet_value(long long length, long long divisions, unsigned actual, unsigned normal,
                                  NotatedValue *out) {
    long long whole = 4 * divisions;
    if (length <= 0 || actual == 0 || normal == 0) {
        return false;
    }
    for (int k = 0; k < VALUE_TYPE_COUNT; k++) {
        for (int d = 0; d <= MAX_VALUE_DOTS; d++) {
            __int128 left = (__int128)length * actual * (1LL << k) * DOT_DENOMINATOR[d];
            __int128 right = (__int128)whole * DOT_NUMERATOR[d] * normal;
            if (left == right) {
                out->type = k + 1;
                out->dots = d;
                out->tuplet = true;
                out->actual = actual;
                out->normal = normal;
                return true;
            }
        }
    }
    return false;
}

/* The value of a note's or rest's piece: by the note's notated tuplet when it has one and the piece fits it, else as
   classify_value does (allow_triplet is its beat rule, which a notated tuplet does not need). */
bool musicxml_score_classify_piece(const Prep *prep, size_t note, long long length, bool allow_triplet, NotatedValue *out) {
    if (note != REST_SEGMENT && prep->notes[note].actual != 0 &&
        classify_tuplet_value(length, prep->divisions, prep->notes[note].actual, prep->notes[note].normal, out)) {
        return true;
    }
    return classify_value(length, prep->divisions, allow_triplet, out);
}

/* The longest writable length of at most max ticks, 0 when there is none that is a whole number of ticks. */
static long long largest_value(long long max, long long divisions, bool allow_triplet) {
    long long whole = 4 * divisions;
    long long best = 0;
    for (int k = 0; k < VALUE_TYPE_COUNT; k++) {
        for (int d = 0; d <= MAX_VALUE_DOTS; d++) {
            long long numerator = whole * DOT_NUMERATOR[d];
            long long denominator = (1LL << k) * DOT_DENOMINATOR[d];
            if (numerator % denominator == 0 && numerator / denominator <= max && numerator / denominator > best) {
                best = numerator / denominator;
            }
        }
    }
    for (int k = 0; allow_triplet && k < TRIPLET_TYPE_COUNT; k++) {
        long long numerator = 2 * whole;
        long long denominator = 3 * (1LL << k);
        if (numerator % denominator == 0 && numerator / denominator <= max && numerator / denominator > best) {
            best = numerator / denominator;
        }
    }
    return best;
}

/* The measure that holds tick (the last one starting at or before it). */
static size_t measure_at(const Prep *prep, long long tick) {
    size_t low = 0;
    size_t high = prep->measure_count;
    while (high - low > 1) {
        size_t mid = low + (high - low) / 2;
        if (prep->measure_start[mid] <= tick) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return low;
}

/*
 * The end of the next written value of a note or rest that starts at pos and runs to at most to (inside one measure
 * and between two cuts). The rules, with the beat of the measure as the unit:
 *   - a quarter-note triplet (2 * beat / 3, in a simple meter with a quarter beat) that starts two thirds into a
 *     beat and is that long (a longer or cut note keeps the next rule) crosses the next beat whole, so three of
 *     them from a beat are one group;
 *   - a value that starts off the beat stops at the next beat;
 *   - one that starts on a beat may cover several beats when its whole length is writable, but in a full 4/4 measure
 *     one that starts on beat 2 stops at beat 3 (one that starts on beat 1 may go through it);
 *   - when the whole is not writable, the longest run of whole beats that is, else the first beat;
 *   - when even that is not writable, the longest writable value from pos.
 * A length nothing can write (below a 128th, or not a whole number of ticks) is returned as it is, and written
 * without a <type>.
 */
static long long next_value_end(const Prep *prep, long long pos, long long to, size_t note) {
    size_t m = measure_at(prep, pos);
    long long start = prep->measure_start[m];
    long long beat = prep->measure_beat[m];
    long long offset = pos - start;
    long long next_beat = start + (offset / beat + 1) * beat;
    long long cap = to;
    NotatedValue crossing;
    if (note != REST_SEGMENT && prep->notes[note].actual != 0 &&
        classify_tuplet_value(to - pos, prep->divisions, prep->notes[note].actual, prep->notes[note].normal, &crossing)) {
        return to; /* written under its own notated tuplet, whole */
    }
    if ((offset % beat) * 3 == 2 * beat && to - pos == 2 * beat / 3 &&
        classify_value(2 * beat / 3, prep->divisions, true, &crossing) && crossing.tuplet) {
        return pos + 2 * beat / 3;
    }
    if (offset % beat != 0) {
        cap = cap < next_beat ? cap : next_beat;
    } else if (prep->measure_four_four[m] && offset / beat == 1 && cap > start + 2 * beat) {
        cap = start + 2 * beat;
    }
    NotatedValue value;
    /* from the far end back to the first beat: the longest span of whole beats (or the cap itself) that is writable */
    long long end = cap;
    for (;;) {
        if (classify_value(end - pos, prep->divisions, end <= next_beat, &value)) {
            return end;
        }
        if (end <= next_beat) {
            break;
        }
        long long previous = start + ((end - 1 - start) / beat) * beat;
        end = previous > next_beat ? previous : next_beat;
    }
    long long take = largest_value(end - pos, prep->divisions, end <= next_beat);
    return take > 0 ? pos + take : end;
}

/*
 * Splits [from, to) at the part's cuts and then into notated values: a note's pieces are tied, a rest's are not. A
 * chord is several notes with this same span, prep->notes[first] to prep->notes[first + members - 1]: each piece is
 * written for every member, the first plain and the others as chord members, so the writer finds them back to back.
 */
bool musicxml_score_add_run(const Prep *prep, size_t part, SegmentList *list, long long from, long long to, size_t note,
                    size_t members) {
    long long position = from;
    int fragment = 0;
    while (position < to) {
        long long next = musicxml_score_next_cut(prep->cuts[part], prep->cut_count[part], position);
        if (next > to) {
            next = to;
        }
        /* each piece between two cuts is cut again into values a note can be written as */
        while (position < next) {
            long long end = next_value_end(prep, position, next, note);
            for (size_t k = 0; k < members; k++) {
                Segment segment;
                segment.start = position;
                segment.end = end;
                segment.note = note == REST_SEGMENT ? REST_SEGMENT : note + k;
                segment.fragment = fragment;
                segment.tie_start = note != REST_SEGMENT && end < to;
                segment.tie_stop = note != REST_SEGMENT && position > from;
                segment.chord = k > 0;
                segment.measure_rest = false;
                segment.tuplet = 0;
                segment.beam_group = 0;
                segment.beam_levels = 0;
                memset(segment.beam, 0, sizeof(segment.beam));
                if (!musicxml_score_segment_push(list, segment)) {
                    return false;
                }
            }
            fragment++;
            position = end;
        }
    }
    return true;
}

void musicxml_score_segments_free(SegmentList *lists, size_t count) {
    if (lists == NULL) {
        return;
    }
    for (size_t g = 0; g < count; g++) {
        free(lists[g].items);
    }
    free(lists);
}

/*
 * Each group's notes in order, the gaps between them as rests, all cut. Split_lanes has already made every group
 * free of overlaps, so the only notes that share a start are chords (the same start and end).
 */
MusicXmlExportResult musicxml_score_build_segments(const Prep *prep, SegmentList **out) {
    SegmentList *lists = (SegmentList *)calloc(prep->group_count > 0 ? prep->group_count : 1, sizeof(SegmentList));
    long long *cursor = (long long *)calloc(prep->group_count > 0 ? prep->group_count : 1, sizeof(long long));
    if (lists == NULL || cursor == NULL) {
        free(lists);
        free(cursor);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    size_t i = 0;
    while (i < prep->note_count && result == MUSICXML_EXPORT_OK) {
        const NoteRec *note = &prep->notes[i];
        size_t g = note->group;
        size_t members = 1;
        while (i + members < prep->note_count && prep->notes[i + members].group == g &&
               prep->notes[i + members].start == note->start && prep->notes[i + members].end == note->end) {
            members++;
        }
        if ((note->start > cursor[g] &&
             !musicxml_score_add_run(prep, note->part, &lists[g], cursor[g], note->start, REST_SEGMENT, 1)) ||
            !musicxml_score_add_run(prep, note->part, &lists[g], note->start, note->end, i, members)) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
        cursor[g] = note->end;
        i += members;
    }
    free(cursor);
    if (result != MUSICXML_EXPORT_OK) {
        musicxml_score_segments_free(lists, prep->group_count);
        return result;
    }
    *out = lists;
    return MUSICXML_EXPORT_OK;
}

/* --- writing --- */

const char *const musicxml_score_PITCH_STEP_NAMES[7] = {"A", "B", "C", "D", "E", "F", "G"};

/* The one id formatter. With cut set an id longer than out is cut to fit (what the export writes), otherwise it fails. */
bool musicxml_score_format_note_xml_id(const char *entity_value, char *out, size_t out_size, bool cut) {
    if (entity_value == NULL || out == NULL || out_size == 0) {
        return false;
    }
    bool needs_prefix = !((entity_value[0] >= 'A' && entity_value[0] <= 'Z') ||
                          (entity_value[0] >= 'a' && entity_value[0] <= 'z') || entity_value[0] == '_');
    size_t pos = 0;
    if (needs_prefix) {
        if (pos + 1 >= out_size && !cut) {
            return false;
        }
        if (pos + 1 < out_size) {
            out[pos++] = 'n';
        }
    }
    for (size_t i = 0; entity_value[i] != '\0'; i++) {
        if (pos + 1 >= out_size) {
            if (cut) {
                break;
            }
            return false;
        }
        char c = entity_value[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '.' || c == '-';
        out[pos++] = ok ? c : '_';
    }
    out[pos] = '\0';
    return true;
}

/* See musicxml_export.h. */
bool musicxml_export_note_xml_id(const char *entity_value, char *out, size_t out_size) {
    return musicxml_score_format_note_xml_id(entity_value, out, out_size, false);
}

/*
 * Marks the tuplet groups of one voice's segments in a measure: a run of two or more tuplet-valued notes and rests of
 * one ratio, back to back inside one beat, opens at its first and closes at its last (chord members follow their first
 * note). A group also closes as soon as its notes and rests fill the span of their ratio, `actual` units of the
 * shortest value in it (three sixteenth-triplet units are an eighth, three eighth-triplet units a beat, five
 * sixteenth-quintuplet units a beat), so six sixteenth triplets in one beat are two groups. A run that does not fill a
 * span closes where it stops. A run whose written value is a beat (a quarter-note triplet, `actual` units of
 * `normal` / `actual` of a beat) may continue into the next beats while it has not filled its units; every other run
 * stays inside the beat it started in.
 */
void musicxml_score_mark_tuplets(const Prep *prep, SegmentList *list) {
    size_t first = 0;
    size_t last = 0;
    size_t length = 0;
    long long run_beat = 0;
    long long filled = 0; /* ticks of the open run */
    long long unit = 0;   /* the shortest tuplet value in the open run, in ticks */
    unsigned run_actual = 0; /* the ratio of the open run */
    unsigned run_normal = 0;
    for (size_t i = 0; i <= list->count; i++) {
        Segment *item = i < list->count ? &list->items[i] : NULL;
        if (item != NULL && item->chord) {
            continue;
        }
        bool tuplet = false;
        long long beat_start = 0;
        long long beat_ticks = 0;
        NotatedValue value;
        if (item != NULL && !item->measure_rest &&
            musicxml_score_classify_piece(prep, item->note, item->end - item->start, true, &value) && value.tuplet) {
            size_t m = measure_at(prep, item->start);
            tuplet = true;
            beat_ticks = prep->measure_beat[m];
            beat_start = prep->measure_start[m] +
                         (item->start - prep->measure_start[m]) / beat_ticks * beat_ticks;
        }
        bool next_beat_ok = beat_start == run_beat ||
                            (unit * run_actual == run_normal * beat_ticks && beat_start > run_beat &&
                             beat_start < run_beat + run_normal * beat_ticks);
        bool continues = tuplet && length > 0 && list->items[last].end == item->start && next_beat_ok &&
                         value.actual == run_actual && value.normal == run_normal;
        if (!continues) {
            if (length >= 2) {
                list->items[first].tuplet = 1;
                list->items[last].tuplet = 2;
            }
            length = 0;
        }
        if (tuplet) {
            long long ticks = item->end - item->start;
            if (length == 0) {
                first = i;
                run_beat = beat_start;
                filled = 0;
                unit = ticks;
                run_actual = value.actual;
                run_normal = value.normal;
            }
            last = i;
            length++;
            filled += ticks;
            unit = ticks < unit ? ticks : unit;
            if (filled % (run_actual * unit) == 0) {
                if (length >= 2) {
                    list->items[first].tuplet = 1;
                    list->items[last].tuplet = 2;
                }
                length = 0;
            }
        }
    }
}

/* The beam states of each level on the notes of every beat group of a chain (chord members are left out). */
static void set_beam_states(SegmentList *list) {
    size_t previous = SIZE_MAX; /* the last beamed unit seen, its group is the one still open */
    for (size_t i = 0; i < list->count; i++) {
        Segment *item = &list->items[i];
        if (item->chord || item->beam_group == 0) {
            continue;
        }
        size_t next = i + 1;
        while (next < list->count && list->items[next].chord) {
            next++;
        }
        bool has_previous = previous != SIZE_MAX && list->items[previous].beam_group == item->beam_group &&
                            list->items[previous].end == item->start;
        bool has_next = next < list->count && list->items[next].beam_group == item->beam_group;
        for (int level = 1; level <= item->beam_levels; level++) {
            bool before = has_previous && list->items[previous].beam_levels >= level;
            bool after = has_next && list->items[next].beam_levels >= level;
            unsigned char state = before && after   ? BEAM_CONTINUE
                                  : after           ? BEAM_BEGIN
                                  : before          ? BEAM_END
                                  : has_previous    ? BEAM_BACKWARD_HOOK
                                                    : BEAM_FORWARD_HOOK;
            item->beam[level - 1] = state;
        }
        previous = i;
    }
}

/*
 * Groups the short notes of one voice's segments in a measure for beaming: consecutive notes written as an eighth or
 * shorter, back to back inside one beat group (a beat in a simple meter, the dotted beat in a compound one), form a
 * group of two or more. A rest, a longer note, a value nothing can write or the next beat ends a group; a chord counts
 * once, as its first note. Triplet runs sit inside one beat, so they are one group.
 */
void musicxml_score_mark_beams(const Prep *prep, SegmentList *list) {
    int group = 0;
    long long group_beat = 0;
    long long group_end = 0;
    size_t group_first = 0;
    size_t members = 0;
    for (size_t i = 0; i <= list->count; i++) {
        Segment *item = i < list->count ? &list->items[i] : NULL;
        if (item != NULL && item->chord) {
            continue;
        }
        int levels = 0;
        long long beat_start = 0;
        NotatedValue value;
        if (item != NULL && item->note != REST_SEGMENT && !item->measure_rest &&
            musicxml_score_classify_piece(prep, item->note, item->end - item->start, true, &value) && value.type >= 4) {
            size_t m = measure_at(prep, item->start);
            levels = value.type - 3;
            beat_start = prep->measure_start[m] +
                         (item->start - prep->measure_start[m]) / prep->measure_beat[m] * prep->measure_beat[m];
        }
        bool continues = levels > 0 && members > 0 && group_end == item->start && beat_start == group_beat;
        if (!continues && members == 1) {
            list->items[group_first].beam_group = 0;
            list->items[group_first].beam_levels = 0;
            group--;
        }
        if (!continues) {
            members = 0;
        }
        if (levels > 0) {
            if (members == 0) {
                group++;
                group_first = i;
                group_beat = beat_start;
            }
            item->beam_group = group;
            item->beam_levels = levels;
            group_end = item->end;
            members++;
        }
    }
    set_beam_states(list);
}

/* One <note> or rest for a segment, in the given voice and staff (staff written only when the part has several). */
void musicxml_score_write_segment(const Prep *prep, XmlBuf *buf, const Segment *segment, size_t part, int staff,
                          int voice_number) {
    bool is_rest = segment->note == REST_SEGMENT;
    const NoteRec *note = is_rest ? NULL : &prep->notes[segment->note];
    NoteEvent *event = is_rest ? NULL : prep->score->events[note->index];

    if (is_rest) {
        musicxml_score_xmlbuf_append(buf, segment->measure_rest ? "      <note>\n        <rest measure=\"yes\"/>\n"
                                                 : "      <note>\n        <rest/>\n");
    } else {
        char base[64];
        const char *chord = segment->chord ? "        <chord/>\n" : "";
        musicxml_score_format_note_xml_id(entity_id_value(note_event_id(event)), base, sizeof(base), true);
        if (segment->tie_start || segment->tie_stop) {
            char id[96];
            if (segment->fragment < 26) {
                snprintf(id, sizeof(id), "%s_%c", base, 'a' + segment->fragment);
            } else {
                snprintf(id, sizeof(id), "%s_%d", base, segment->fragment);
            }
            musicxml_score_xmlbuf_append(buf, "      <note xml:id=\"%s\">\n%s", id, chord);
        } else {
            musicxml_score_xmlbuf_append(buf, "      <note xml:id=\"%s\">\n%s", base, chord);
        }
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(event);
        musicxml_score_xmlbuf_append(buf, "        <pitch>\n          <step>%s</step>\n",
                      musicxml_score_PITCH_STEP_NAMES[pitch_spelling_letter(spelling)]);
        if (pitch_spelling_accidental(spelling) != 0) {
            musicxml_score_xmlbuf_append(buf, "          <alter>%d</alter>\n", pitch_spelling_accidental(spelling));
        }
        musicxml_score_xmlbuf_append(buf, "          <octave>%d</octave>\n        </pitch>\n", pitch_spelling_octave(spelling));
    }
    musicxml_score_xmlbuf_append(buf, "        <duration>%lld</duration>\n", segment->end - segment->start);
    if (segment->tie_stop) {
        musicxml_score_xmlbuf_append(buf, "        <tie type=\"stop\"/>\n");
    }
    if (segment->tie_start) {
        musicxml_score_xmlbuf_append(buf, "        <tie type=\"start\"/>\n");
    }
    musicxml_score_xmlbuf_append(buf, "        <voice>%d</voice>\n", voice_number);
    NotatedValue value;
    if (!segment->measure_rest && musicxml_score_classify_piece(prep, segment->note, segment->end - segment->start, true, &value)) {
        musicxml_score_xmlbuf_append(buf, "        <type>%s</type>\n", VALUE_TYPE_NAMES[value.type - 1]);
        for (int d = 0; d < value.dots; d++) {
            musicxml_score_xmlbuf_append(buf, "        <dot/>\n");
        }
        if (value.tuplet) {
            musicxml_score_xmlbuf_append(buf, "        <time-modification>\n          <actual-notes>%u</actual-notes>\n"
                               "          <normal-notes>%u</normal-notes>\n        </time-modification>\n",
                          value.actual, value.normal);
        }
    }
    if (prep->parts[part].staff_count > 1) {
        musicxml_score_xmlbuf_append(buf, "        <staff>%d</staff>\n", staff);
    }
    for (int level = 0; level < segment->beam_levels && level < MAX_BEAM_LEVELS && !segment->chord; level++) {
        if (segment->beam[level] != BEAM_NONE) {
            musicxml_score_xmlbuf_append(buf, "        <beam number=\"%d\">%s</beam>\n", level + 1, BEAM_NAMES[segment->beam[level]]);
        }
    }
    if (segment->tie_stop || segment->tie_start || segment->tuplet != 0) {
        musicxml_score_xmlbuf_append(buf, "        <notations>\n");
        if (segment->tie_stop) {
            musicxml_score_xmlbuf_append(buf, "          <tied type=\"stop\"/>\n");
        }
        if (segment->tie_start) {
            musicxml_score_xmlbuf_append(buf, "          <tied type=\"start\"/>\n");
        }
        if (segment->tuplet != 0) {
            musicxml_score_xmlbuf_append(buf, "          <tuplet type=\"%s\"/>\n", segment->tuplet == 1 ? "start" : "stop");
        }
        musicxml_score_xmlbuf_append(buf, "        </notations>\n");
    }
    musicxml_score_xmlbuf_append(buf, "      </note>\n");
}
