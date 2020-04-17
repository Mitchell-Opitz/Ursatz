#include "export/musicxml_export.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "identity/entity_id.h"
#include "persistence/persistence_note_events.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Fixed ticks-per-whole-note used for every exported <duration>. 3072 =
 * 2^10 * 3, exactly divisible by every common binary subdivision (down
 * to 1/1024 of a whole note) and by triplet thirds -- the two families
 * of Duration denominators this codebase's Duration values are expected
 * to carry. An unsupported denominator still produces a value (integer
 * division), just not exact.
 *
 * <type> (whole/half/quarter/...) is deliberately never emitted:
 * arbitrary tied fragments across the synthetic barlines would require
 * decomposing every fragment into a dotted-note/tuplet representation,
 * which MusicXML does not require -- <duration> alone is sufficient for
 * a valid, timing-correct document. Note-type decomposition is left to
 * whatever renders this file (feat/notation-rendering), out of scope
 * here.
 */
static const long long TICKS_PER_WHOLE_NOTE = 3072;

static const char *const PITCH_STEP_NAMES[7] = {"A", "B", "C", "D", "E", "F", "G"};

/*
 * Derives a valid XML ID (NCName) token from a NoteEvent's EntityID
 * value: prefixes with 'n' if the value doesn't already start with a
 * letter or underscore (XML IDs can't start with a digit or most other
 * characters), and replaces any character outside [A-Za-z0-9_.-] with
 * '_'. Deterministic and reversible-enough for debugging -- most
 * existing EntityID values (e.g. "midi-note-3") pass through unchanged.
 * Writes into out (must be at least len(raw) + 2 bytes).
 */
static void xml_id_from_entity(const char *raw, char *out, size_t out_size) {
    size_t pos = 0;
    bool needs_prefix = !((raw[0] >= 'A' && raw[0] <= 'Z') || (raw[0] >= 'a' && raw[0] <= 'z') || raw[0] == '_');
    if (needs_prefix && pos + 1 < out_size) {
        out[pos++] = 'n';
    }
    for (size_t i = 0; raw[i] != '\0' && pos + 1 < out_size; i++) {
        char c = raw[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '.' || c == '-';
        out[pos++] = ok ? c : '_';
    }
    out[pos] = '\0';
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool failed;
} XmlBuf;

static bool xmlbuf_init(XmlBuf *buf) {
    buf->len = 0;
    buf->cap = 4096;
    buf->failed = false;
    buf->data = (char *)malloc(buf->cap);
    if (buf->data == NULL) {
        buf->failed = true;
        return false;
    }
    buf->data[0] = '\0';
    return true;
}

static void xmlbuf_free(XmlBuf *buf) {
    free(buf->data);
    buf->data = NULL;
}

static void xmlbuf_append(XmlBuf *buf, const char *fmt, ...) {
    if (buf->failed) {
        return;
    }

    for (;;) {
        va_list args;
        va_start(args, fmt);
        int needed = vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
        va_end(args);

        if (needed < 0) {
            buf->failed = true;
            return;
        }
        if ((size_t)needed < buf->cap - buf->len) {
            buf->len += (size_t)needed;
            return;
        }

        size_t new_cap = buf->cap * 2;
        while (new_cap < buf->len + (size_t)needed + 1) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(buf->data, new_cap);
        if (new_data == NULL) {
            buf->failed = true;
            return;
        }
        buf->data = new_data;
        buf->cap = new_cap;
    }
}

/* Exact rational arithmetic, in whole-note units. Denominator is always
   kept positive by rational_make. */
typedef struct {
    long long num;
    long long den;
} Rational;

static long long gcd_ll(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        long long t = b;
        b = a % b;
        a = t;
    }
    return a == 0 ? 1 : a;
}

static Rational rational_make(long long num, long long den) {
    if (den < 0) {
        num = -num;
        den = -den;
    }
    long long g = gcd_ll(num, den);
    Rational r;
    r.num = num / g;
    r.den = den / g;
    return r;
}

/* Floor toward negative infinity (den is always positive here). */
static long long rational_floor(Rational r) {
    long long q = r.num / r.den;
    long long rem = r.num % r.den;
    if (rem != 0 && r.num < 0) {
        q -= 1;
    }
    return q;
}

static Rational rational_add(Rational a, Rational b) {
    return rational_make(a.num * b.den + b.num * a.den, a.den * b.den);
}

static Rational rational_sub(Rational a, Rational b) {
    return rational_make(a.num * b.den - b.num * a.den, a.den * b.den);
}

static bool rational_lt(Rational a, Rational b) {
    return a.num * b.den < b.num * a.den;
}

static long long rational_ticks(Rational r, long long ticks_per_whole) {
    return (r.num * ticks_per_whole) / r.den;
}

static void xml_append_note_or_rest(XmlBuf *buf, const char *xml_id, bool is_rest, PitchLetter letter, int accidental,
                                     int octave, long long duration_ticks, bool tie_start, bool tie_stop,
                                     int voice_number) {
    if (xml_id != NULL) {
        xmlbuf_append(buf, "      <note xml:id=\"%s\">\n", xml_id);
    } else {
        xmlbuf_append(buf, "      <note>\n");
    }
    if (is_rest) {
        xmlbuf_append(buf, "        <rest/>\n");
    } else {
        xmlbuf_append(buf, "        <pitch>\n");
        xmlbuf_append(buf, "          <step>%s</step>\n", PITCH_STEP_NAMES[letter]);
        if (accidental != 0) {
            xmlbuf_append(buf, "          <alter>%d</alter>\n", accidental);
        }
        xmlbuf_append(buf, "          <octave>%d</octave>\n", octave);
        xmlbuf_append(buf, "        </pitch>\n");
    }
    xmlbuf_append(buf, "        <duration>%lld</duration>\n", duration_ticks);
    if (tie_stop) {
        xmlbuf_append(buf, "        <tie type=\"stop\"/>\n");
    }
    if (tie_start) {
        xmlbuf_append(buf, "        <tie type=\"start\"/>\n");
    }
    xmlbuf_append(buf, "        <voice>%d</voice>\n", voice_number);
    xmlbuf_append(buf, "      </note>\n");
}

/* One <note>/<rest> block, already fully rendered, tagged with the
   synthetic-4/4-measure index it belongs to so the final assembly pass
   can group fragments by measure across voices. */
typedef struct {
    long long measure_idx;
    char *xml;
    long long duration_ticks;
} Fragment;

typedef struct {
    Fragment *items;
    size_t count;
    size_t cap;
} FragmentList;

static bool fragment_list_push(FragmentList *list, long long measure_idx, char *xml, long long duration_ticks) {
    if (list->count == list->cap) {
        size_t new_cap = list->cap == 0 ? 8 : list->cap * 2;
        Fragment *grown = (Fragment *)realloc(list->items, sizeof(Fragment) * new_cap);
        if (grown == NULL) {
            return false;
        }
        list->items = grown;
        list->cap = new_cap;
    }
    list->items[list->count].measure_idx = measure_idx;
    list->items[list->count].xml = xml;
    list->items[list->count].duration_ticks = duration_ticks;
    list->count++;
    return true;
}

static void fragment_list_free(FragmentList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].xml);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

/*
 * Advances cursor by length (a rest if is_rest, otherwise a pitched
 * note), splitting into one <note>/<rest> Fragment per synthetic-4/4-
 * measure fragment and tying consecutive note fragments across
 * barlines (rests are never tied -- MusicXML has no rest-tie concept).
 * Fragments are pushed onto list, one per voice, for the final
 * measure-by-measure assembly pass to interleave with <backup>
 * elements. Returns false on allocation failure.
 */
static bool collect_run_fragments(FragmentList *list, Rational *cursor, Rational length, const char *base_xml_id,
                                   bool is_rest, PitchLetter letter, int accidental, int octave,
                                   long long ticks_per_whole, int voice_number) {
    bool first_fragment = true;
    int fragment_index = 0;

    while (length.num > 0) {
        long long idx = rational_floor(*cursor);
        Rational measure_end = rational_make(idx + 1, 1);
        Rational room = rational_sub(measure_end, *cursor);
        Rational chunk = rational_lt(length, room) ? length : room;
        Rational remaining_after = rational_sub(length, chunk);

        bool tie_start = !is_rest && remaining_after.num > 0;
        bool tie_stop = !is_rest && !first_fragment;

        char fragment_id[64];
        const char *xml_id = NULL;
        if (base_xml_id != NULL) {
            if (tie_start || tie_stop) {
                snprintf(fragment_id, sizeof(fragment_id), "%s_%c", base_xml_id, 'a' + fragment_index);
                xml_id = fragment_id;
            } else {
                xml_id = base_xml_id;
            }
        }

        long long duration_ticks = rational_ticks(chunk, ticks_per_whole);

        XmlBuf fragment_buf;
        if (!xmlbuf_init(&fragment_buf)) {
            return false;
        }
        xml_append_note_or_rest(&fragment_buf, xml_id, is_rest, letter, accidental, octave, duration_ticks, tie_start,
                                 tie_stop, voice_number);
        if (fragment_buf.failed) {
            xmlbuf_free(&fragment_buf);
            return false;
        }
        if (!fragment_list_push(list, idx, fragment_buf.data, duration_ticks)) {
            xmlbuf_free(&fragment_buf);
            return false;
        }

        *cursor = rational_add(*cursor, chunk);
        length = remaining_after;
        first_fragment = false;
        fragment_index++;
    }
    return true;
}

static void free_note_events(NoteEvent **events, size_t count) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

/*
 * Groups NoteEvents by VoiceReference (NoteEvents with no voice share
 * one implicit group, so a piece with no voice information at all
 * behaves exactly as before -- one voice, numbered 1). Voice numbers
 * are assigned in order of first appearance among events, 1-based.
 * group_of_event[i] receives the group index (not voice number) for
 * events[i]. Returns the number of distinct groups, or SIZE_MAX on
 * allocation failure.
 */
static size_t group_events_by_voice(NoteEvent **events, size_t count, size_t *group_of_event) {
    const char **group_keys = (count > 0) ? (const char **)malloc(sizeof(const char *) * count) : NULL;
    if (count > 0 && group_keys == NULL) {
        return SIZE_MAX;
    }
    size_t group_count = 0;

    for (size_t i = 0; i < count; i++) {
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        const char *key = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;

        size_t found = group_count;
        for (size_t g = 0; g < group_count; g++) {
            bool same = (key == NULL && group_keys[g] == NULL) ||
                        (key != NULL && group_keys[g] != NULL && strcmp(key, group_keys[g]) == 0);
            if (same) {
                found = g;
                break;
            }
        }
        if (found == group_count) {
            group_keys[group_count] = key;
            group_count++;
        }
        group_of_event[i] = found;
    }

    free(group_keys);
    return group_count;
}

MusicXmlExportResult musicxml_export_write(PersistenceHandle *handle, char **out_xml) {
    if (handle == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }

    NoteEvent **events = NULL;
    size_t count = 0;
    if (persistence_load_note_events(handle, &events, &count) != PERSISTENCE_NOTE_EVENTS_OK) {
        return MUSICXML_EXPORT_ERR_LOAD_FAILED;
    }

    size_t *group_of_event = (count > 0) ? (size_t *)malloc(sizeof(size_t) * count) : NULL;
    if (count > 0 && group_of_event == NULL) {
        free_note_events(events, count);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    size_t group_count = group_events_by_voice(events, count, group_of_event);
    if (group_count == SIZE_MAX) {
        free(group_of_event);
        free_note_events(events, count);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }

    Rational *group_cursor = NULL;
    FragmentList *group_fragments = NULL;
    if (group_count > 0) {
        group_cursor = (Rational *)malloc(sizeof(Rational) * group_count);
        group_fragments = (FragmentList *)calloc(group_count, sizeof(FragmentList));
        if (group_cursor == NULL || group_fragments == NULL) {
            free(group_cursor);
            free(group_fragments);
            free(group_of_event);
            free_note_events(events, count);
            return MUSICXML_EXPORT_ERR_ALLOC;
        }
        for (size_t g = 0; g < group_count; g++) {
            group_cursor[g] = rational_make(0, 1);
        }
    }

    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    long long max_measure_idx = -1;

    for (size_t i = 0; i < count && result == MUSICXML_EXPORT_OK; i++) {
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(events[i]);
        if (spelling == NULL) {
            result = MUSICXML_EXPORT_ERR_MISSING_SPELLING;
            break;
        }

        size_t g = group_of_event[i];
        int voice_number = (int)g + 1;

        const MusicalTime *pos = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        Rational start = rational_make(musical_time_numerator(pos), (long long)musical_time_denominator(pos));

        if (rational_lt(start, group_cursor[g])) {
            result = MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES;
            break;
        }

        Rational gap = rational_sub(start, group_cursor[g]);
        if (gap.num > 0) {
            if (!collect_run_fragments(&group_fragments[g], &group_cursor[g], gap, NULL, true, PITCH_LETTER_A, 0, 0,
                                        TICKS_PER_WHOLE_NOTE, voice_number)) {
                result = MUSICXML_EXPORT_ERR_ALLOC;
                break;
            }
        }

        const Duration *duration = note_event_duration(events[i]);
        Rational length = rational_make(duration_numerator(duration), (long long)duration_denominator(duration));

        char xml_id[64];
        xml_id_from_entity(entity_id_value(note_event_id(events[i])), xml_id, sizeof(xml_id));

        if (!collect_run_fragments(&group_fragments[g], &group_cursor[g], length, xml_id, false,
                                    pitch_spelling_letter(spelling), pitch_spelling_accidental(spelling),
                                    pitch_spelling_octave(spelling), TICKS_PER_WHOLE_NOTE, voice_number)) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
            break;
        }
    }

    if (result == MUSICXML_EXPORT_OK) {
        for (size_t g = 0; g < group_count; g++) {
            for (size_t f = 0; f < group_fragments[g].count; f++) {
                if (group_fragments[g].items[f].measure_idx > max_measure_idx) {
                    max_measure_idx = group_fragments[g].items[f].measure_idx;
                }
            }
        }
    }

    XmlBuf buf;
    bool buf_ok = xmlbuf_init(&buf);
    if (result == MUSICXML_EXPORT_OK && !buf_ok) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }

    if (result == MUSICXML_EXPORT_OK) {
        xmlbuf_append(&buf,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
                      "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                      "<score-partwise version=\"3.1\">\n"
                      "  <identification>\n"
                      "    <miscellaneous>\n"
                      "      <miscellaneous-field name=\"ursatz:synthetic-meter\">true</miscellaneous-field>\n"
                      "    </miscellaneous>\n"
                      "  </identification>\n"
                      "  <part-list>\n"
                      "    <score-part id=\"P1\">\n"
                      "      <part-name>Ursatz Export</part-name>\n"
                      "    </score-part>\n"
                      "  </part-list>\n"
                      "  <part id=\"P1\">\n");

        if (max_measure_idx < 0) {
            /* Empty piece: still a valid part, with one empty measure. */
            xmlbuf_append(&buf,
                          "    <measure number=\"1\">\n"
                          "      <attributes>\n"
                          "        <divisions>%lld</divisions>\n"
                          "        <time>\n"
                          "          <beats>4</beats>\n"
                          "          <beat-type>4</beat-type>\n"
                          "        </time>\n"
                          "      </attributes>\n"
                          "    </measure>\n",
                          TICKS_PER_WHOLE_NOTE / 4);
        } else {
            /* next_index[g] walks group g's fragment list forward as
               measures are consumed in order -- each group's fragments
               are already produced in non-decreasing measure_idx order. */
            size_t *next_index = (size_t *)calloc(group_count, sizeof(size_t));
            if (next_index == NULL) {
                result = MUSICXML_EXPORT_ERR_ALLOC;
            } else {
                for (long long m = 0; m <= max_measure_idx && result == MUSICXML_EXPORT_OK; m++) {
                    xmlbuf_append(&buf, "    <measure number=\"%lld\">\n", m + 1);
                    if (m == 0) {
                        xmlbuf_append(&buf,
                                      "      <attributes>\n"
                                      "        <divisions>%lld</divisions>\n"
                                      "        <time>\n"
                                      "          <beats>4</beats>\n"
                                      "          <beat-type>4</beat-type>\n"
                                      "        </time>\n"
                                      "      </attributes>\n",
                                      TICKS_PER_WHOLE_NOTE / 4);
                    }

                    long long ticks_in_measure = 0;
                    for (size_t g = 0; g < group_count; g++) {
                        FragmentList *fragments = &group_fragments[g];
                        size_t start_idx = next_index[g];
                        size_t end_idx = start_idx;
                        while (end_idx < fragments->count && fragments->items[end_idx].measure_idx == m) {
                            end_idx++;
                        }
                        if (end_idx == start_idx) {
                            continue;
                        }
                        if (ticks_in_measure > 0) {
                            xmlbuf_append(&buf, "      <backup>\n        <duration>%lld</duration>\n      </backup>\n",
                                          ticks_in_measure);
                            ticks_in_measure = 0;
                        }
                        for (size_t f = start_idx; f < end_idx; f++) {
                            xmlbuf_append(&buf, "%s", fragments->items[f].xml);
                            ticks_in_measure += fragments->items[f].duration_ticks;
                        }
                        next_index[g] = end_idx;
                    }

                    xmlbuf_append(&buf, "    </measure>\n");
                }
                free(next_index);
            }
        }

        if (result == MUSICXML_EXPORT_OK) {
            xmlbuf_append(&buf, "  </part>\n</score-partwise>\n");
        }
    }

    for (size_t g = 0; g < group_count; g++) {
        fragment_list_free(&group_fragments[g]);
    }
    free(group_fragments);
    free(group_cursor);
    free(group_of_event);
    free_note_events(events, count);

    if (result != MUSICXML_EXPORT_OK || (buf_ok && buf.failed)) {
        MusicXmlExportResult final_result = (result != MUSICXML_EXPORT_OK) ? result : MUSICXML_EXPORT_ERR_ALLOC;
        if (buf_ok) {
            xmlbuf_free(&buf);
        }
        return final_result;
    }

    *out_xml = buf.data;
    return MUSICXML_EXPORT_OK;
}
