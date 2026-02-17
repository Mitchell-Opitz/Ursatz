#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "export/musicxml_export.h"
#include "identity/entity_id.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/*
 * musicxml_export_write_score: lays a score out on the measures of a MeterMap and writes it as MusicXML 3.1.
 *
 * All time is whole notes, exactly. <divisions> D (per quarter note) is the smallest whole number for which every
 * position and duration, times 4 * D, is an integer; from then on the code works in integer ticks (1/(4 * D) of a
 * whole note), so nothing is rounded and no fraction is compared. The stages:
 *   1. validate the input and gather every note;
 *   2. choose D for the notes (and the markings), then fetch the measure spans from the MeterMap (which may need a
 *      finer D, to which the ticks are rescaled);
 *   3. cut each part at every barline (and every marking position), split what crosses a cut with ties, fill gaps
 *      with rests;
 *   4. write each measure.
 */

#define MAX_DIVISIONS ((long long)INT32_MAX)

/* --- output buffer --- */

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
        char *grown = (char *)realloc(buf->data, new_cap);
        if (grown == NULL) {
            buf->failed = true;
            return;
        }
        buf->data = grown;
        buf->cap = new_cap;
    }
}

/* Text or attribute content with the five XML specials escaped. */
static void xmlbuf_append_escaped(XmlBuf *buf, const char *text) {
    for (const char *c = text; *c != '\0'; c++) {
        switch (*c) {
        case '&':
            xmlbuf_append(buf, "&amp;");
            break;
        case '<':
            xmlbuf_append(buf, "&lt;");
            break;
        case '>':
            xmlbuf_append(buf, "&gt;");
            break;
        case '"':
            xmlbuf_append(buf, "&quot;");
            break;
        case '\'':
            xmlbuf_append(buf, "&apos;");
            break;
        default:
            xmlbuf_append(buf, "%c", *c);
        }
    }
}

/* --- exact integer helpers --- */

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
    return a;
}

/* *divisions = lcm(*divisions, den), false when that is above MAX_DIVISIONS. */
static bool lcm_into(long long *divisions, long long den) {
    long long g = gcd_ll(*divisions, den);
    long long product = 0;
    if (g == 0 || __builtin_mul_overflow(*divisions / g, den, &product) || product > MAX_DIVISIONS) {
        return false;
    }
    *divisions = product;
    return true;
}

typedef enum {
    TIME_OK = 0,
    TIME_NOT_REPRESENTABLE,
    TIME_DIVISIONS_OVERFLOW
} TimeStatus;

/* The denominator of 4 * (n / d) in lowest terms: the <divisions> that makes this one time an integer. */
static TimeStatus time_denominator(long n, unsigned long d, long long *out) {
    long long four_n = 0;
    if (d == 0 || d > (unsigned long)LLONG_MAX || __builtin_mul_overflow((long long)n, 4LL, &four_n)) {
        return TIME_NOT_REPRESENTABLE;
    }
    *out = (long long)d / gcd_ll(four_n, (long long)d);
    return TIME_OK;
}

/* 4 * D * (n / d), which the caller has made an integer by choosing D. */
static TimeStatus time_ticks(long n, unsigned long d, long long divisions, long long *out) {
    long long four_n = 0;
    if (d == 0 || d > (unsigned long)LLONG_MAX || __builtin_mul_overflow((long long)n, 4LL, &four_n)) {
        return TIME_NOT_REPRESENTABLE;
    }
    long long g = gcd_ll(four_n, (long long)d);
    long long den = (long long)d / g;
    if (divisions % den != 0 || __builtin_mul_overflow(four_n / g, divisions / den, out)) {
        return TIME_NOT_REPRESENTABLE;
    }
    return TIME_OK;
}

static MusicXmlExportResult from_time_status(TimeStatus status) {
    return status == TIME_DIVISIONS_OVERFLOW ? MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW
                                             : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
}

/* --- the prepared score --- */

typedef struct {
    const char *id;
    const char *name;
    int staff_count;
} PartInfo;

typedef struct {
    size_t index;  /* the input index of the event */
    size_t part;
    int staff;
    size_t group;
    long long start; /* ticks */
    long long end;
    long start_n;    /* the exact start and duration, kept until divisions are final */
    unsigned long start_d;
    long dur_n;
    unsigned long dur_d;
} NoteRec;

/* One voice of one staff of one part: the notes that share a VoiceReference (or have none). */
typedef struct {
    size_t part;
    int staff;
    const char *voice; /* the VoiceReference's target id, NULL for none */
    int voice_number;  /* 1-based within the part */
} Group;

typedef struct {
    const MusicXmlExportScore *score;

    PartInfo *parts;
    size_t part_count;

    NoteRec *notes;
    size_t note_count;
    Group *groups;
    size_t group_count;

    MeterMap *own_meter; /* the defaulted map made when none was supplied */
    const MeterMap *meter;
    bool meter_defaulted;

    long long divisions;
    long long extent; /* ticks: the end of the last note */
    size_t measure_count;
    int first_measure_number;
    long long *measure_start; /* ticks, measure_count entries */
    long long *measure_end;
} Prep;

static void prep_free(Prep *prep) {
    free(prep->parts);
    free(prep->notes);
    free(prep->groups);
    free(prep->measure_start);
    free(prep->measure_end);
    if (prep->own_meter != NULL) {
        meter_map_destroy(prep->own_meter);
    }
    memset(prep, 0, sizeof(*prep));
}

static MusicXmlExportResult validate_arrays(const MusicXmlExportScore *score) {
    if (score->count > 0 && (score->events == NULL || score->pitch_values == NULL)) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    for (size_t i = 0; i < score->count; i++) {
        if (score->events[i] == NULL || score->pitch_values[i] == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
    }
    if ((score->part_count > 0) != (score->parts != NULL)) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    for (size_t i = 0; i < score->part_count; i++) {
        if (score->parts[i].id == NULL || score->parts[i].staff_count < 1) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
    }
    if ((score->tempo_marking_count > 0 && score->tempo_markings == NULL) ||
        (score->dynamics_count > 0 && score->dynamics == NULL) ||
        (score->key_signature_count > 0 && score->key_signatures == NULL)) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    return MUSICXML_EXPORT_OK;
}

/* The effective parts: the supplied ones, or one default part wide enough for every staff used. */
static MusicXmlExportResult build_parts(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    prep->part_count = score->part_count > 0 ? score->part_count : 1;
    prep->parts = (PartInfo *)calloc(prep->part_count, sizeof(PartInfo));
    if (prep->parts == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    if (score->part_count > 0) {
        for (size_t i = 0; i < score->part_count; i++) {
            prep->parts[i].id = score->parts[i].id;
            prep->parts[i].name = score->parts[i].name != NULL ? score->parts[i].name : "";
            prep->parts[i].staff_count = score->parts[i].staff_count;
        }
        return MUSICXML_EXPORT_OK;
    }
    int staves = 1;
    for (size_t i = 0; i < score->count && score->staff != NULL; i++) {
        if (score->staff[i] > staves) {
            staves = score->staff[i];
        }
    }
    prep->parts[0].id = "P1";
    prep->parts[0].name = "Ursatz Export";
    prep->parts[0].staff_count = staves;
    return MUSICXML_EXPORT_OK;
}

static int compare_notes(const void *left, const void *right) {
    const NoteRec *a = (const NoteRec *)left;
    const NoteRec *b = (const NoteRec *)right;
    if (a->group != b->group) {
        return a->group < b->group ? -1 : 1;
    }
    if (a->start != b->start) {
        return a->start < b->start ? -1 : 1;
    }
    return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
}

/* The notes: part, staff, spelling present, exact start and duration, and their voice groups. */
static MusicXmlExportResult gather_notes(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    prep->note_count = score->count;
    prep->notes = (NoteRec *)calloc(score->count > 0 ? score->count : 1, sizeof(NoteRec));
    prep->groups = (Group *)calloc(score->count > 0 ? score->count : 1, sizeof(Group));
    if (prep->notes == NULL || prep->groups == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t i = 0; i < score->count; i++) {
        NoteEvent *event = score->events[i];
        NoteRec *note = &prep->notes[i];
        note->index = i;
        note->part = score->part_index != NULL ? score->part_index[i] : 0;
        note->staff = score->staff != NULL ? score->staff[i] : 1;
        if (note->part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (note->staff < 1 || note->staff > prep->parts[note->part].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        if (note_event_optional_pitch_spelling(event) == NULL) {
            return MUSICXML_EXPORT_ERR_MISSING_SPELLING;
        }
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
        const Duration *duration = note_event_duration(event);
        note->start_n = musical_time_numerator(position);
        note->start_d = musical_time_denominator(position);
        note->dur_n = duration_numerator(duration);
        note->dur_d = duration_denominator(duration);
        if (note->start_n < 0 || note->dur_n <= 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }

        const VoiceReference *voice = note_event_optional_voice(event);
        const char *key = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
        size_t found = prep->group_count;
        int in_part = 0;
        for (size_t g = 0; g < prep->group_count; g++) {
            if (prep->groups[g].part != note->part) {
                continue;
            }
            in_part++;
            const Group *candidate = &prep->groups[g];
            bool same_voice = (key == NULL && candidate->voice == NULL) ||
                              (key != NULL && candidate->voice != NULL && strcmp(key, candidate->voice) == 0);
            if (candidate->staff == note->staff && same_voice) {
                found = g;
                break;
            }
        }
        if (found == prep->group_count) {
            prep->groups[found].part = note->part;
            prep->groups[found].staff = note->staff;
            prep->groups[found].voice = key;
            prep->groups[found].voice_number = in_part + 1;
            prep->group_count++;
        }
        note->group = found;
    }
    return MUSICXML_EXPORT_OK;
}

/* The smallest divisions for the notes; false (with the status) when one does not fit. */
static MusicXmlExportResult choose_divisions(Prep *prep) {
    long long divisions = 1;
    for (size_t i = 0; i < prep->note_count; i++) {
        const NoteRec *note = &prep->notes[i];
        long long den = 0;
        TimeStatus status = time_denominator(note->start_n, note->start_d, &den);
        if (status == TIME_OK && !lcm_into(&divisions, den)) {
            status = TIME_DIVISIONS_OVERFLOW;
        }
        if (status == TIME_OK) {
            status = time_denominator(note->dur_n, note->dur_d, &den);
        }
        if (status == TIME_OK && !lcm_into(&divisions, den)) {
            status = TIME_DIVISIONS_OVERFLOW;
        }
        if (status != TIME_OK) {
            return from_time_status(status);
        }
    }
    prep->divisions = divisions;
    return MUSICXML_EXPORT_OK;
}

/* Every note's start and end in ticks at the current divisions. */
static MusicXmlExportResult notes_to_ticks(Prep *prep) {
    prep->extent = 0;
    for (size_t i = 0; i < prep->note_count; i++) {
        NoteRec *note = &prep->notes[i];
        long long length = 0;
        if (time_ticks(note->start_n, note->start_d, prep->divisions, &note->start) != TIME_OK ||
            time_ticks(note->dur_n, note->dur_d, prep->divisions, &length) != TIME_OK ||
            __builtin_add_overflow(note->start, length, &note->end)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (note->end > prep->extent) {
            prep->extent = note->end;
        }
    }
    return MUSICXML_EXPORT_OK;
}

/* Multiplies every tick by factor (the divisions grew by that much). */
static MusicXmlExportResult rescale_ticks(Prep *prep, long long factor) {
    for (size_t i = 0; i < prep->note_count; i++) {
        if (__builtin_mul_overflow(prep->notes[i].start, factor, &prep->notes[i].start) ||
            __builtin_mul_overflow(prep->notes[i].end, factor, &prep->notes[i].end)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
    }
    if (__builtin_mul_overflow(prep->extent, factor, &prep->extent)) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult from_meter_status(MeterMapResult status) {
    return status == METER_MAP_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
}

/*
 * The measures: how many cover the content, and where each starts and ends, from the MeterMap alone. The spans'
 * denominators may need a finer divisions; if so every tick is rescaled.
 */
static MusicXmlExportResult build_measures(Prep *prep) {
    MusicalTime *extent_time = NULL;
    if (musical_time_create((long)prep->extent, (unsigned long)(4 * prep->divisions), &extent_time) != MUSICAL_TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    size_t count = 0;
    MeterMapResult status = meter_map_measure_count(prep->meter, extent_time, &count);
    musical_time_destroy(extent_time);
    if (status != METER_MAP_OK) {
        return from_meter_status(status);
    }
    if (count == 0) {
        count = 1;
    }
    prep->measure_count = count;
    prep->first_measure_number = meter_map_first_measure_number(prep->meter);

    long *start_n = (long *)calloc(count, sizeof(long));
    unsigned long *start_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    long *end_n = (long *)calloc(count, sizeof(long));
    unsigned long *end_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    prep->measure_start = (long long *)calloc(count, sizeof(long long));
    prep->measure_end = (long long *)calloc(count, sizeof(long long));
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    if (start_n == NULL || start_d == NULL || end_n == NULL || end_d == NULL || prep->measure_start == NULL ||
        prep->measure_end == NULL) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }

    long long divisions = prep->divisions;
    for (size_t m = 0; m < count && result == MUSICXML_EXPORT_OK; m++) {
        TimeSpan *span = NULL;
        status = meter_map_measure_span(prep->meter, prep->first_measure_number + (int)m, &span);
        if (status != METER_MAP_OK) {
            result = from_meter_status(status);
            break;
        }
        const MusicalTime *start = time_span_start(span);
        const MusicalTime *end = time_span_end(span);
        start_n[m] = musical_time_numerator(start);
        start_d[m] = musical_time_denominator(start);
        end_n[m] = musical_time_numerator(end);
        end_d[m] = musical_time_denominator(end);
        time_span_destroy(span);
        long long den = 0;
        TimeStatus time_status = time_denominator(start_n[m], start_d[m], &den);
        if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
            time_status = TIME_DIVISIONS_OVERFLOW;
        }
        if (time_status == TIME_OK) {
            time_status = time_denominator(end_n[m], end_d[m], &den);
        }
        if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
            time_status = TIME_DIVISIONS_OVERFLOW;
        }
        if (time_status != TIME_OK) {
            result = from_time_status(time_status);
        }
    }

    if (result == MUSICXML_EXPORT_OK && divisions != prep->divisions) {
        result = rescale_ticks(prep, divisions / prep->divisions);
        prep->divisions = divisions;
    }
    for (size_t m = 0; m < count && result == MUSICXML_EXPORT_OK; m++) {
        if (time_ticks(start_n[m], start_d[m], prep->divisions, &prep->measure_start[m]) != TIME_OK ||
            time_ticks(end_n[m], end_d[m], prep->divisions, &prep->measure_end[m]) != TIME_OK) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
    }
    free(start_n);
    free(start_d);
    free(end_n);
    free(end_d);
    return result;
}

static MusicXmlExportResult prepare(Prep *prep) {
    MusicXmlExportResult result = validate_arrays(prep->score);
    if (result == MUSICXML_EXPORT_OK) {
        result = build_parts(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_notes(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = choose_divisions(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = notes_to_ticks(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        if (prep->score->meter_map != NULL) {
            prep->meter = prep->score->meter_map;
        } else {
            EntityID *id = NULL;
            if (entity_id_create("musicxml-export-meter-map", &id) != ENTITY_ID_OK) {
                return MUSICXML_EXPORT_ERR_ALLOC;
            }
            if (meter_map_create_defaulted(id, &prep->own_meter) != METER_MAP_OK) {
                entity_id_destroy(id);
                return MUSICXML_EXPORT_ERR_ALLOC;
            }
            prep->meter = prep->own_meter;
        }
        prep->meter_defaulted = meter_map_is_defaulted(prep->meter);
        result = build_measures(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        qsort(prep->notes, prep->note_count, sizeof(NoteRec), compare_notes);
    }
    return result;
}

/* --- writing --- */

static void write_document(const Prep *prep, XmlBuf *buf) {
    xmlbuf_append(buf,
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
                  "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                  "<score-partwise version=\"3.1\">\n"
                  "  <part-list>\n");
    for (size_t p = 0; p < prep->part_count; p++) {
        xmlbuf_append(buf, "    <score-part id=\"");
        xmlbuf_append_escaped(buf, prep->parts[p].id);
        xmlbuf_append(buf, "\">\n      <part-name>");
        xmlbuf_append_escaped(buf, prep->parts[p].name);
        xmlbuf_append(buf, "</part-name>\n    </score-part>\n");
    }
    xmlbuf_append(buf, "  </part-list>\n");
    for (size_t p = 0; p < prep->part_count; p++) {
        xmlbuf_append(buf, "  <part id=\"");
        xmlbuf_append_escaped(buf, prep->parts[p].id);
        xmlbuf_append(buf, "\">\n");
        for (size_t m = 0; m < prep->measure_count; m++) {
            xmlbuf_append(buf, "    <measure number=\"%d\">\n", prep->first_measure_number + (int)m);
            if (m == 0) {
                xmlbuf_append(buf, "      <attributes>\n        <divisions>%lld</divisions>\n      </attributes>\n",
                              prep->divisions);
            }
            xmlbuf_append(buf, "    </measure>\n");
        }
        xmlbuf_append(buf, "  </part>\n");
    }
    xmlbuf_append(buf, "</score-partwise>\n");
}

MusicXmlExportResult musicxml_export_write_score(const MusicXmlExportScore *score, char **out_xml) {
    if (score == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }
    Prep prep;
    memset(&prep, 0, sizeof(prep));
    prep.score = score;
    MusicXmlExportResult result = prepare(&prep);

    XmlBuf buf;
    bool buf_ok = false;
    if (result == MUSICXML_EXPORT_OK) {
        buf_ok = xmlbuf_init(&buf);
        if (!buf_ok) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
    }
    if (result == MUSICXML_EXPORT_OK) {
        write_document(&prep, &buf);
        if (buf.failed) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
    }
    prep_free(&prep);
    if (result != MUSICXML_EXPORT_OK) {
        if (buf_ok) {
            xmlbuf_free(&buf);
        }
        return result;
    }
    *out_xml = buf.data;
    return MUSICXML_EXPORT_OK;
}
