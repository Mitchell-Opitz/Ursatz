#include "export/musicxml_export.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
                                     int octave, long long duration_ticks, bool tie_start, bool tie_stop) {
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
    xmlbuf_append(buf, "      </note>\n");
}

/*
 * Advances cursor by length (a rest if is_rest, otherwise a pitched
 * note), splitting into one <note>/<rest> per synthetic-4/4-measure
 * fragment and tying consecutive note fragments across barlines
 * (rests are never tied -- MusicXML has no rest-tie concept). Opens and
 * closes <measure> elements on *open_measure_idx as the synthetic grid
 * is crossed.
 */
static void write_run(XmlBuf *buf, Rational *cursor, Rational length, const char *base_xml_id, bool is_rest,
                       PitchLetter letter, int accidental, int octave, long long ticks_per_whole,
                       long long *open_measure_idx) {
    bool first_fragment = true;
    int fragment_index = 0;

    while (length.num > 0) {
        long long idx = rational_floor(*cursor);
        if (idx != *open_measure_idx) {
            if (*open_measure_idx >= 0) {
                xmlbuf_append(buf, "    </measure>\n");
            }
            xmlbuf_append(buf, "    <measure number=\"%lld\">\n", idx + 1);
            if (idx == 0) {
                xmlbuf_append(buf,
                              "      <attributes>\n"
                              "        <divisions>%lld</divisions>\n"
                              "        <time>\n"
                              "          <beats>4</beats>\n"
                              "          <beat-type>4</beat-type>\n"
                              "        </time>\n"
                              "      </attributes>\n",
                              ticks_per_whole / 4);
            }
            *open_measure_idx = idx;
        }

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

        xml_append_note_or_rest(buf, xml_id, is_rest, letter, accidental, octave,
                                 rational_ticks(chunk, ticks_per_whole), tie_start, tie_stop);

        *cursor = rational_add(*cursor, chunk);
        length = remaining_after;
        first_fragment = false;
        fragment_index++;
    }
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

MusicXmlExportResult musicxml_export_write(PersistenceHandle *handle, char **out_xml) {
    if (handle == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }

    NoteEvent **events = NULL;
    size_t count = 0;
    if (persistence_load_note_events(handle, &events, &count) != PERSISTENCE_NOTE_EVENTS_OK) {
        return MUSICXML_EXPORT_ERR_LOAD_FAILED;
    }

    XmlBuf buf;
    if (!xmlbuf_init(&buf)) {
        free_note_events(events, count);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }

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

    Rational cursor = rational_make(0, 1);
    long long open_measure_idx = -1;
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;

    for (size_t i = 0; i < count; i++) {
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(events[i]);
        if (spelling == NULL) {
            result = MUSICXML_EXPORT_ERR_MISSING_SPELLING;
            break;
        }

        const MusicalTime *pos = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        Rational start = rational_make(musical_time_numerator(pos), (long long)musical_time_denominator(pos));

        if (rational_lt(start, cursor)) {
            result = MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES;
            break;
        }

        Rational gap = rational_sub(start, cursor);
        if (gap.num > 0) {
            write_run(&buf, &cursor, gap, NULL, true, PITCH_LETTER_A, 0, 0, TICKS_PER_WHOLE_NOTE, &open_measure_idx);
        }

        const Duration *duration = note_event_duration(events[i]);
        Rational length = rational_make(duration_numerator(duration), (long long)duration_denominator(duration));

        char xml_id[64];
        xml_id_from_entity(entity_id_value(note_event_id(events[i])), xml_id, sizeof(xml_id));

        write_run(&buf, &cursor, length, xml_id, false, pitch_spelling_letter(spelling),
                  pitch_spelling_accidental(spelling), pitch_spelling_octave(spelling), TICKS_PER_WHOLE_NOTE,
                  &open_measure_idx);
    }

    if (result == MUSICXML_EXPORT_OK) {
        if (open_measure_idx < 0) {
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
            xmlbuf_append(&buf, "    </measure>\n");
        }
        xmlbuf_append(&buf, "  </part>\n</score-partwise>\n");
    }

    free_note_events(events, count);

    if (result != MUSICXML_EXPORT_OK || buf.failed) {
        MusicXmlExportResult final_result = (result != MUSICXML_EXPORT_OK) ? result : MUSICXML_EXPORT_ERR_ALLOC;
        xmlbuf_free(&buf);
        return final_result;
    }

    *out_xml = buf.data;
    return MUSICXML_EXPORT_OK;
}
