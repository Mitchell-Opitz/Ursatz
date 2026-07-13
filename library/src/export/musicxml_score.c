#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "export/musicxml_export.h"
#include "export/musicxml_score_internal.h"
#include "export/score_quantize.h"
#include "identity/entity_id.h"
#include "pitch/pitch_spelling.h"
#include "events/dynamics.h"
#include "time/duration.h"
#include "time/exact_rational.h"
#include "time/metronome_marking.h"
#include "time/tempo_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
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

/* Something written between notes at a position: a key signature now, markings next. Sorted by part, tick, kind. */
typedef enum {
    DIR_KEY = 0,
    DIR_TEMPO,   /* source: the TempoMap region index */
    DIR_MARKING, /* source: the TempoMarking index */
    DIR_DYNAMIC  /* source: the Dynamics index */
} DirKind;

typedef struct {
    DirKind kind;
    size_t part;
    int staff;
    long raw_n; /* the exact position, kept until divisions are final */
    unsigned long raw_d;
    long long tick;
    size_t source; /* the index into the input array of its kind */
} Dir;

/* One voice of one staff of one part: the notes that share a VoiceReference (or have none). */
typedef struct {
    size_t part;
    int staff;
    const char *voice; /* the VoiceReference's target id, NULL for none */
    int voice_number;  /* 1-based within the part */
} Group;

typedef struct {
    const MusicXmlExportScore *score;
    bool strict; /* an overlap in a voice fails instead of being split into lanes */

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

    Dir *dirs;
    size_t dir_count;
    size_t *dir_first;     /* part_count + 1 entries: part p's dirs are dirs[dir_first[p]] up to dirs[dir_first[p+1]] */
    long long dir_max_tick; /* -1 when there are no dirs */

    long long **cuts;   /* per part: every barline, ascending, where notes and rests are split */
    size_t *cut_count;
} Prep;

static void prep_free(Prep *prep) {
    free(prep->parts);
    free(prep->notes);
    free(prep->groups);
    free(prep->dirs);
    free(prep->dir_first);
    free(prep->measure_start);
    free(prep->measure_end);
    if (prep->cuts != NULL) {
        for (size_t p = 0; p < prep->part_count; p++) {
            free(prep->cuts[p]);
        }
    }
    free(prep->cuts);
    free(prep->cut_count);
    if (prep->own_meter != NULL) {
        meter_map_destroy(prep->own_meter);
    }
    memset(prep, 0, sizeof(*prep));
}

static MusicXmlExportResult validate_arrays(const MusicXmlExportScore *score) {
    if (score->count > 0 && score->events == NULL) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    for (size_t i = 0; i < score->count; i++) {
        if (score->events[i] == NULL || (score->pitch_values != NULL && score->pitch_values[i] == NULL)) {
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

/* Replaces the played start and duration of every note with the notated ones from score_quantize. */
static MusicXmlExportResult apply_quantize(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    ScoreQuantized quantized;
    ScoreQuantizeResult status = score_quantize(score->events, score->count, score->part_index, score->staff,
                                                score->meter_map, score->quantize, &quantized);
    if (status != SCORE_QUANTIZE_OK) {
        return status == SCORE_QUANTIZE_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    for (size_t i = 0; i < prep->note_count && result == MUSICXML_EXPORT_OK; i++) {
        ExactRational start = er_make(quantized.start_num[i], quantized.start_den[i]);
        ExactRational end = er_make(quantized.end_num[i], quantized.end_den[i]);
        ExactRational duration;
        if (er_cmp(end, start) <= 0 || er_sub(end, start, &duration) != 0 || start.n > (unsigned long long)LONG_MAX ||
            duration.n > (unsigned long long)LONG_MAX || start.d > ULONG_MAX || duration.d > ULONG_MAX) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
            break;
        }
        prep->notes[i].start_n = (long)start.n;
        prep->notes[i].start_d = (unsigned long)start.d;
        prep->notes[i].dur_n = (long)duration.n;
        prep->notes[i].dur_d = (unsigned long)duration.d;
    }
    score_quantized_destroy(&quantized);
    return result;
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
    return prep->score->quantize != NULL ? apply_quantize(prep) : MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_keys(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->key_signature_count; i++) {
        const MusicXmlExportKeySignature *key = &score->key_signatures[i];
        if (key->position == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
        if (key->part_index >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (key->sharps_flats < -7 || key->sharps_flats > 7 || musical_time_numerator(key->position) < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_KEY;
        dir->part = key->part_index;
        dir->staff = 1;
        dir->raw_n = musical_time_numerator(key->position);
        dir->raw_d = musical_time_denominator(key->position);
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

/* The beat units MusicXML names, as whole-note fractions. */
static const struct {
    const char *name;
    unsigned long long numerator;
    unsigned long long denominator;
} BEAT_UNITS[] = {{"long", 4, 1},   {"breve", 2, 1},  {"whole", 1, 1},   {"half", 1, 2},    {"quarter", 1, 4},
                  {"eighth", 1, 8}, {"16th", 1, 16},  {"32nd", 1, 32},   {"64th", 1, 64},  {"128th", 1, 128}};
#define MAX_BEAT_UNIT_DOTS 4

/* The name and dot count of a beat unit n/d, false when MusicXML has no such note. */
static bool beat_unit_name(unsigned long n, unsigned long d, const char **name, int *dots) {
    for (size_t u = 0; u < sizeof(BEAT_UNITS) / sizeof(BEAT_UNITS[0]); u++) {
        for (int k = 0; k <= MAX_BEAT_UNIT_DOTS; k++) {
            /* base * (2^(k+1) - 1) / 2^k */
            unsigned long long want_n = BEAT_UNITS[u].numerator * ((1ull << (k + 1)) - 1);
            unsigned long long want_d = BEAT_UNITS[u].denominator * (1ull << k);
            if ((unsigned long long)n * want_d == want_n * (unsigned long long)d) {
                *name = BEAT_UNITS[u].name;
                *dots = k;
                return true;
            }
        }
    }
    return false;
}

/* num/den as a decimal with at most 12 places, trailing zeros dropped. *exact is false when it had to be rounded. */
static bool ratio_to_decimal(unsigned long long num, unsigned long long den, char *out, size_t size, bool *exact) {
    if (den == 0 || den > 100000000000000000ull) {
        return false;
    }
    unsigned long long whole = num / den;
    unsigned long long rem = num % den;
    char digits[13];
    int used = 0;
    while (used < 12 && rem != 0) {
        rem *= 10;
        digits[used++] = (char)('0' + rem / den);
        rem %= den;
    }
    *exact = rem == 0;
    if (rem != 0 && rem * 2 >= den) {
        int at = used - 1;
        while (at >= 0 && digits[at] == '9') {
            digits[at--] = '0';
        }
        if (at >= 0) {
            digits[at]++;
        } else {
            whole++;
        }
    }
    while (used > 0 && digits[used - 1] == '0') {
        used--;
    }
    digits[used] = '\0';
    int written = snprintf(out, size, used > 0 ? "%llu.%s" : "%llu", whole, digits);
    return written > 0 && (size_t)written < size;
}

static MusicXmlExportResult gather_tempo(Prep *prep) {
    const TempoMap *map = prep->score->tempo_map;
    if (map == NULL || tempo_map_is_defaulted(map)) {
        return MUSICXML_EXPORT_OK;
    }
    for (size_t i = 0; i < tempo_map_region_count(map); i++) {
        const TempoMapRegion *region = tempo_map_region(map, i);
        if (tempo_map_region_is_defaulted(region)) {
            continue;
        }
        if (tempo_map_region_microseconds_per_quarter(region) == 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_TEMPO;
        dir->part = 0;
        dir->staff = 1;
        dir->raw_n = musical_time_numerator(tempo_map_region_start(region));
        dir->raw_d = musical_time_denominator(tempo_map_region_start(region));
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_markings(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->tempo_marking_count; i++) {
        const TempoMarking *marking = score->tempo_markings[i];
        if (marking == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
        size_t part = score->tempo_marking_part != NULL ? score->tempo_marking_part[i] : 0;
        if (part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (tempo_marking_has_metronome_marking(marking)) {
            const MetronomeMarking *metronome = tempo_marking_metronome_marking(marking);
            const char *name = NULL;
            int dots = 0;
            char text[64];
            bool exact = false;
            if (!beat_unit_name(metronome_marking_beat_unit_numerator(metronome),
                                metronome_marking_beat_unit_denominator(metronome), &name, &dots) ||
                !ratio_to_decimal(metronome_marking_bpm_numerator(metronome),
                                  metronome_marking_bpm_denominator(metronome), text, sizeof(text), &exact) ||
                !exact) {
                return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
            }
        }
        const MusicalTime *position = temporal_anchor_position(tempo_marking_temporal_anchor(marking));
        if (musical_time_numerator(position) < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_MARKING;
        dir->part = part;
        dir->staff = 1;
        dir->raw_n = musical_time_numerator(position);
        dir->raw_d = musical_time_denominator(position);
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

/* Where a Dynamics sounds: the start of its span, or the start of the event it targets. */
static MusicXmlExportResult dynamics_position(const Prep *prep, const Dynamics *dynamics, long *n, unsigned long *d) {
    const MusicalTime *position = NULL;
    if (dynamics_target_kind(dynamics) == DYNAMICS_TARGET_TIME_SPAN) {
        position = time_span_start(dynamics_target_time_span(dynamics));
    } else {
        const char *id = entity_id_value(dynamics_target_event_id(dynamics));
        for (size_t i = 0; i < prep->score->count && position == NULL; i++) {
            if (strcmp(entity_id_value(note_event_id(prep->score->events[i])), id) == 0) {
                position = temporal_anchor_position(note_event_temporal_anchor(prep->score->events[i]));
            }
        }
        if (position == NULL) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_EVENT;
        }
    }
    if (musical_time_numerator(position) < 0) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    *n = musical_time_numerator(position);
    *d = musical_time_denominator(position);
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_dynamics(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->dynamics_count; i++) {
        const Dynamics *dynamics = score->dynamics[i];
        if (dynamics == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
        size_t part = score->dynamics_part != NULL ? score->dynamics_part[i] : 0;
        int staff = score->dynamics_staff != NULL ? score->dynamics_staff[i] : 1;
        if (part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (staff < 1 || staff > prep->parts[part].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        Dir *dir = &prep->dirs[prep->dir_count];
        MusicXmlExportResult result = dynamics_position(prep, dynamics, &dir->raw_n, &dir->raw_d);
        if (result != MUSICXML_EXPORT_OK) {
            return result;
        }
        dir->kind = DIR_DYNAMIC;
        dir->part = part;
        dir->staff = staff;
        dir->source = i;
        prep->dir_count++;
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_dirs(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    size_t capacity = score->key_signature_count + score->tempo_marking_count + score->dynamics_count;
    if (score->tempo_map != NULL) {
        capacity += tempo_map_region_count(score->tempo_map);
    }
    prep->dirs = (Dir *)calloc(capacity > 0 ? capacity : 1, sizeof(Dir));
    if (prep->dirs == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    MusicXmlExportResult result = gather_keys(prep);
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_tempo(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_markings(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_dynamics(prep);
    }
    return result;
}

/* The smallest divisions for the notes and the dirs; false (with the status) when one does not fit. */
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
    for (size_t i = 0; i < prep->dir_count; i++) {
        long long den = 0;
        TimeStatus status = time_denominator(prep->dirs[i].raw_n, prep->dirs[i].raw_d, &den);
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

static MusicXmlExportResult dirs_to_ticks(Prep *prep) {
    prep->dir_max_tick = -1;
    for (size_t i = 0; i < prep->dir_count; i++) {
        Dir *dir = &prep->dirs[i];
        if (time_ticks(dir->raw_n, dir->raw_d, prep->divisions, &dir->tick) != TIME_OK) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (dir->tick > prep->dir_max_tick) {
            prep->dir_max_tick = dir->tick;
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
    for (size_t i = 0; i < prep->dir_count; i++) {
        if (__builtin_mul_overflow(prep->dirs[i].tick, factor, &prep->dirs[i].tick)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
    }
    if (prep->dir_max_tick > 0 && __builtin_mul_overflow(prep->dir_max_tick, factor, &prep->dir_max_tick)) {
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
    /* enough measures for the last note and for the measure that holds the last dir */
    long long covered = prep->extent;
    if (prep->dir_max_tick >= 0 && prep->dir_max_tick + 1 > covered) {
        covered = prep->dir_max_tick + 1;
    }
    MusicalTime *extent_time = NULL;
    if (musical_time_create((long)covered, (unsigned long)(4 * prep->divisions), &extent_time) != MUSICAL_TIME_OK) {
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

static int compare_ticks(const void *left, const void *right) {
    long long a = *(const long long *)left;
    long long b = *(const long long *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Per part, the ticks at which every note and rest is cut: each barline (and the end of the last measure). */
static int compare_dirs(const void *left, const void *right) {
    const Dir *a = (const Dir *)left;
    const Dir *b = (const Dir *)right;
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->tick != b->tick) {
        return a->tick < b->tick ? -1 : 1;
    }
    if (a->kind != b->kind) {
        return a->kind < b->kind ? -1 : 1;
    }
    return a->source < b->source ? -1 : (a->source > b->source ? 1 : 0);
}

/* Orders the dirs and finds where each part's start. */
static MusicXmlExportResult order_dirs(Prep *prep) {
    qsort(prep->dirs, prep->dir_count, sizeof(Dir), compare_dirs);
    prep->dir_first = (size_t *)calloc(prep->part_count + 1, sizeof(size_t));
    if (prep->dir_first == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    size_t at = 0;
    for (size_t p = 0; p < prep->part_count; p++) {
        prep->dir_first[p] = at;
        while (at < prep->dir_count && prep->dirs[at].part == p) {
            at++;
        }
    }
    prep->dir_first[prep->part_count] = at;
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult build_cuts(Prep *prep) {
    prep->cuts = (long long **)calloc(prep->part_count, sizeof(long long *));
    prep->cut_count = (size_t *)calloc(prep->part_count, sizeof(size_t));
    if (prep->cuts == NULL || prep->cut_count == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count; p++) {
        size_t dirs_of_part = prep->dir_first[p + 1] - prep->dir_first[p];
        long long *cuts = (long long *)calloc(prep->measure_count + 1 + dirs_of_part, sizeof(long long));
        if (cuts == NULL) {
            return MUSICXML_EXPORT_ERR_ALLOC;
        }
        size_t n = 0;
        for (size_t m = 0; m < prep->measure_count; m++) {
            cuts[n++] = prep->measure_start[m];
        }
        cuts[n++] = prep->measure_end[prep->measure_count - 1];
        for (size_t d = prep->dir_first[p]; d < prep->dir_first[p + 1]; d++) {
            cuts[n++] = prep->dirs[d].tick;
        }
        qsort(cuts, n, sizeof(long long), compare_ticks);
        size_t unique = 0;
        for (size_t i = 0; i < n; i++) {
            if (unique == 0 || cuts[i] != cuts[unique - 1]) {
                cuts[unique++] = cuts[i];
            }
        }
        prep->cuts[p] = cuts;
        prep->cut_count[p] = unique;
    }
    return MUSICXML_EXPORT_OK;
}

/* One strand of non-overlapping notes of a voice group; its last chord is the notes sharing chord_start/chord_end. */
typedef struct {
    size_t group;
    long long cursor; /* the end of the last note */
    long long chord_start;
    long long chord_end;
} Lane;

/*
 * Splits every voice group into lanes so no group has overlapping notes. The notes must be sorted (group, start,
 * index). A note that starts and ends with a lane's last chord joins that chord; any other goes to the lowest lane
 * that is free at its start, and a new lane is made when none is. Lane 0 stays the group itself; each further lane
 * becomes a group of its own on the same part and staff, with the next voice number unused in the part.
 */
static MusicXmlExportResult split_lanes(Prep *prep) {
    int *next_voice = (int *)calloc(prep->part_count, sizeof(int));
    if (next_voice == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t g = 0; g < prep->group_count; g++) {
        if (prep->groups[g].voice_number >= next_voice[prep->groups[g].part]) {
            next_voice[prep->groups[g].part] = prep->groups[g].voice_number + 1;
        }
    }
    Lane *lanes = NULL;
    size_t lane_count = 0;
    size_t lane_cap = 0;
    size_t home = SIZE_MAX;
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    for (size_t i = 0; i < prep->note_count && result == MUSICXML_EXPORT_OK; i++) {
        NoteRec *note = &prep->notes[i];
        if (note->group != home) {
            home = note->group;
            lane_count = 0;
        }
        if (prep->strict && lane_count > 0 && note->start < lanes[0].cursor) {
            result = MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES;
            break;
        }
        size_t chosen = lane_count;
        for (size_t l = 0; l < lane_count && chosen == lane_count; l++) {
            if (lanes[l].chord_start == note->start && lanes[l].chord_end == note->end) {
                chosen = l;
            }
        }
        for (size_t l = 0; l < lane_count && chosen == lane_count; l++) {
            if (lanes[l].cursor <= note->start) {
                chosen = l;
            }
        }
        if (chosen == lane_count) {
            if (lane_count == lane_cap) {
                size_t cap = lane_cap == 0 ? 4 : lane_cap * 2;
                Lane *grown = (Lane *)realloc(lanes, cap * sizeof(Lane));
                if (grown == NULL) {
                    result = MUSICXML_EXPORT_ERR_ALLOC;
                    break;
                }
                lanes = grown;
                lane_cap = cap;
            }
            lanes[lane_count].group = home;
            if (lane_count > 0) {
                /* at most one lane per note, so the groups array (one slot per note) has room */
                lanes[lane_count].group = prep->group_count++;
                prep->groups[lanes[lane_count].group] = prep->groups[home];
                prep->groups[lanes[lane_count].group].voice_number = next_voice[prep->groups[home].part]++;
            }
            lane_count++;
        }
        lanes[chosen].cursor = note->end;
        lanes[chosen].chord_start = note->start;
        lanes[chosen].chord_end = note->end;
        note->group = lanes[chosen].group;
    }
    free(lanes);
    free(next_voice);
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
        result = gather_dirs(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = choose_divisions(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = notes_to_ticks(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = dirs_to_ticks(prep);
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
        result = order_dirs(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = build_cuts(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        qsort(prep->notes, prep->note_count, sizeof(NoteRec), compare_notes);
        result = split_lanes(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        qsort(prep->notes, prep->note_count, sizeof(NoteRec), compare_notes);
    }
    return result;
}

/* --- segments: the notes and rests of one voice, cut at the part's cuts --- */

#define REST_SEGMENT SIZE_MAX

typedef struct {
    long long start;
    long long end;
    size_t note;      /* an index into prep->notes, or REST_SEGMENT */
    int fragment;     /* 0 for the first piece of a note */
    bool tie_start;
    bool tie_stop;
    bool chord;       /* written with <chord/>, sharing the previous note's start and duration */
} Segment;

typedef struct {
    Segment *items;
    size_t count;
    size_t cap;
} SegmentList;

static bool segment_push(SegmentList *list, Segment segment) {
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
static long long next_cut(const long long *cuts, size_t count, long long tick) {
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

/*
 * Splits [from, to) at the part's cuts: a note's pieces are tied, a rest's are not. A chord is several notes with
 * this same span, prep->notes[first] to prep->notes[first + members - 1]: each piece is written for every member,
 * the first plain and the others as chord members, so the writer finds them back to back.
 */
static bool add_run(const Prep *prep, size_t part, SegmentList *list, long long from, long long to, size_t note,
                    size_t members) {
    long long position = from;
    int fragment = 0;
    while (position < to) {
        long long next = next_cut(prep->cuts[part], prep->cut_count[part], position);
        if (next > to) {
            next = to;
        }
        for (size_t k = 0; k < members; k++) {
            Segment segment;
            segment.start = position;
            segment.end = next;
            segment.note = note == REST_SEGMENT ? REST_SEGMENT : note + k;
            segment.fragment = fragment;
            segment.tie_start = note != REST_SEGMENT && next < to;
            segment.tie_stop = note != REST_SEGMENT && position > from;
            segment.chord = k > 0;
            if (!segment_push(list, segment)) {
                return false;
            }
        }
        fragment++;
        position = next;
    }
    return true;
}

static void segments_free(SegmentList *lists, size_t count) {
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
static MusicXmlExportResult build_segments(const Prep *prep, SegmentList **out) {
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
             !add_run(prep, note->part, &lists[g], cursor[g], note->start, REST_SEGMENT, 1)) ||
            !add_run(prep, note->part, &lists[g], note->start, note->end, i, members)) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
        cursor[g] = note->end;
        i += members;
    }
    free(cursor);
    if (result != MUSICXML_EXPORT_OK) {
        segments_free(lists, prep->group_count);
        return result;
    }
    *out = lists;
    return MUSICXML_EXPORT_OK;
}

/* --- writing --- */

static const char *const PITCH_STEP_NAMES[7] = {"A", "B", "C", "D", "E", "F", "G"};

/* A valid XML ID from an EntityID value: a leading 'n' if it does not start with a letter or underscore, and '_' for
   anything outside [A-Za-z0-9_.-]. */
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

/* One <note> or rest for a segment, in the given voice and staff (staff written only when the part has several). */
static void write_segment(const Prep *prep, XmlBuf *buf, const Segment *segment, size_t part, int staff,
                          int voice_number) {
    bool is_rest = segment->note == REST_SEGMENT;
    const NoteRec *note = is_rest ? NULL : &prep->notes[segment->note];
    NoteEvent *event = is_rest ? NULL : prep->score->events[note->index];

    if (is_rest) {
        xmlbuf_append(buf, "      <note>\n        <rest/>\n");
    } else {
        char base[64];
        const char *chord = segment->chord ? "        <chord/>\n" : "";
        xml_id_from_entity(entity_id_value(note_event_id(event)), base, sizeof(base));
        if (segment->tie_start || segment->tie_stop) {
            char id[96];
            if (segment->fragment < 26) {
                snprintf(id, sizeof(id), "%s_%c", base, 'a' + segment->fragment);
            } else {
                snprintf(id, sizeof(id), "%s_%d", base, segment->fragment);
            }
            xmlbuf_append(buf, "      <note xml:id=\"%s\">\n%s", id, chord);
        } else {
            xmlbuf_append(buf, "      <note xml:id=\"%s\">\n%s", base, chord);
        }
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(event);
        xmlbuf_append(buf, "        <pitch>\n          <step>%s</step>\n",
                      PITCH_STEP_NAMES[pitch_spelling_letter(spelling)]);
        if (pitch_spelling_accidental(spelling) != 0) {
            xmlbuf_append(buf, "          <alter>%d</alter>\n", pitch_spelling_accidental(spelling));
        }
        xmlbuf_append(buf, "          <octave>%d</octave>\n        </pitch>\n", pitch_spelling_octave(spelling));
    }
    xmlbuf_append(buf, "        <duration>%lld</duration>\n", segment->end - segment->start);
    if (segment->tie_stop) {
        xmlbuf_append(buf, "        <tie type=\"stop\"/>\n");
    }
    if (segment->tie_start) {
        xmlbuf_append(buf, "        <tie type=\"start\"/>\n");
    }
    xmlbuf_append(buf, "        <voice>%d</voice>\n", voice_number);
    if (prep->parts[part].staff_count > 1) {
        xmlbuf_append(buf, "        <staff>%d</staff>\n", staff);
    }
    xmlbuf_append(buf, "      </note>\n");
}

/* The dirs of one part still to write, and where its current measure starts. */
typedef struct {
    const Prep *prep;
    size_t part;
    size_t *pos;
    long long measure_start;
} DirCtx;

static void write_key(XmlBuf *buf, const MusicXmlExportKeySignature *key, const char *indent) {
    xmlbuf_append(buf, "%s<key>\n%s  <fifths>%d</fifths>\n", indent, indent, key->sharps_flats);
    if (key->mode == MUSICXML_EXPORT_MODE_MAJOR) {
        xmlbuf_append(buf, "%s  <mode>major</mode>\n", indent);
    } else if (key->mode == MUSICXML_EXPORT_MODE_MINOR) {
        xmlbuf_append(buf, "%s  <mode>minor</mode>\n", indent);
    }
    xmlbuf_append(buf, "%s</key>\n", indent);
}

/* 60,000,000 / microseconds-per-quarter as a decimal: the quarter notes per minute of a <sound tempo>. */
static void tempo_text(const Prep *prep, size_t region, char *out, size_t size) {
    bool exact = false;
    unsigned long micro = tempo_map_region_microseconds_per_quarter(tempo_map_region(prep->score->tempo_map, region));
    if (!ratio_to_decimal(60000000ull, micro, out, size, &exact)) {
        snprintf(out, size, "0");
    }
}

static void write_marking(const Prep *prep, XmlBuf *buf, const TempoMarking *marking, const char *sound_tempo) {
    xmlbuf_append(buf, "      <direction placement=\"above\">\n");
    if (tempo_marking_has_text(marking)) {
        xmlbuf_append(buf, "        <direction-type>\n          <words>");
        xmlbuf_append_escaped(buf, tempo_marking_text(marking));
        xmlbuf_append(buf, "</words>\n        </direction-type>\n");
    }
    if (tempo_marking_has_metronome_marking(marking)) {
        const MetronomeMarking *metronome = tempo_marking_metronome_marking(marking);
        const char *name = "quarter";
        int dots = 0;
        char per_minute[64];
        bool exact = false;
        beat_unit_name(metronome_marking_beat_unit_numerator(metronome),
                       metronome_marking_beat_unit_denominator(metronome), &name, &dots);
        ratio_to_decimal(metronome_marking_bpm_numerator(metronome), metronome_marking_bpm_denominator(metronome),
                         per_minute, sizeof(per_minute), &exact);
        xmlbuf_append(buf, "        <direction-type>\n          <metronome>\n            <beat-unit>%s</beat-unit>\n",
                      name);
        for (int k = 0; k < dots; k++) {
            xmlbuf_append(buf, "            <beat-unit-dot/>\n");
        }
        xmlbuf_append(buf, "            <per-minute>%s</per-minute>\n          </metronome>\n        </direction-type>\n",
                      per_minute);
    }
    if (sound_tempo != NULL) {
        xmlbuf_append(buf, "        <sound tempo=\"%s\"/>\n", sound_tempo);
    }
    xmlbuf_append(buf, "      </direction>\n");
    (void)prep;
}

/* The MusicXML dynamics elements; any other marking is written as <other-dynamics>. */
static const char *const DYNAMICS_ELEMENTS[] = {"p",  "pp", "ppp", "pppp", "ppppp", "pppppp", "f",  "ff", "fff", "ffff",
                                                "fffff", "ffffff", "mp", "mf", "sf", "sfp", "sfpp", "fp", "rf", "rfz",
                                                "sfz", "sffz", "fz", "n", "pf", "sfzp"};

static void write_dynamic(const Prep *prep, XmlBuf *buf, const Dir *dir) {
    const char *marking = dynamics_marking(prep->score->dynamics[dir->source]);
    bool named = false;
    for (size_t i = 0; i < sizeof(DYNAMICS_ELEMENTS) / sizeof(DYNAMICS_ELEMENTS[0]); i++) {
        named = named || strcmp(marking, DYNAMICS_ELEMENTS[i]) == 0;
    }
    xmlbuf_append(buf, "      <direction placement=\"below\">\n        <direction-type>\n          <dynamics>");
    if (named) {
        xmlbuf_append(buf, "<%s/>", marking);
    } else {
        xmlbuf_append(buf, "<other-dynamics>");
        xmlbuf_append_escaped(buf, marking);
        xmlbuf_append(buf, "</other-dynamics>");
    }
    xmlbuf_append(buf, "</dynamics>\n        </direction-type>\n");
    if (prep->parts[dir->part].staff_count > 1) {
        xmlbuf_append(buf, "        <staff>%d</staff>\n", dir->staff);
    }
    xmlbuf_append(buf, "      </direction>\n");
}

/* The dirs that share one tick, in kind order. A key at the start of a measure is already in its <attributes>. */
static void write_dir_group(const Prep *prep, XmlBuf *buf, const Dir *group, size_t count, long long measure_start) {
    bool key_written = false;
    const Dir *tempo = NULL;
    for (size_t i = 0; i < count; i++) {
        if (group[i].kind == DIR_KEY && !key_written) {
            key_written = true;
            if (group[i].tick != measure_start) {
                xmlbuf_append(buf, "      <attributes>\n");
                write_key(buf, &prep->score->key_signatures[group[i].source], "        ");
                xmlbuf_append(buf, "      </attributes>\n");
            }
        } else if (group[i].kind == DIR_TEMPO && tempo == NULL) {
            tempo = &group[i];
        }
    }
    /* The tempo goes in the first marking's <direction> when there is one, so a reader finds one tempo marking. */
    char tempo_value[64];
    bool tempo_pending = tempo != NULL;
    if (tempo_pending) {
        tempo_text(prep, tempo->source, tempo_value, sizeof(tempo_value));
    }
    for (size_t i = 0; i < count; i++) {
        if (group[i].kind == DIR_MARKING) {
            write_marking(prep, buf, prep->score->tempo_markings[group[i].source], tempo_pending ? tempo_value : NULL);
            tempo_pending = false;
        }
    }
    if (tempo_pending) {
        xmlbuf_append(buf, "      <sound tempo=\"%s\"/>\n", tempo_value);
    }
    for (size_t i = 0; i < count; i++) {
        if (group[i].kind == DIR_DYNAMIC) {
            write_dynamic(prep, buf, &group[i]);
        }
    }
}

/* Writes every dir of the part at or before tick that has not been written. */
static void write_dirs_through(DirCtx *ctx, XmlBuf *buf, long long tick) {
    if (ctx == NULL) {
        return;
    }
    const Prep *prep = ctx->prep;
    size_t end = prep->dir_first[ctx->part + 1];
    while (*ctx->pos < end && prep->dirs[*ctx->pos].tick <= tick) {
        size_t begin = *ctx->pos;
        size_t stop = begin;
        while (stop < end && prep->dirs[stop].tick == prep->dirs[begin].tick) {
            stop++;
        }
        write_dir_group(prep, buf, &prep->dirs[begin], stop - begin, ctx->measure_start);
        *ctx->pos = stop;
    }
}

/* A rest from from to to, cut at the part's cuts; the part's dirs are written before each piece when dirs is given. */
static void write_rest_run(const Prep *prep, XmlBuf *buf, size_t part, long long from, long long to, int staff,
                           int voice_number, DirCtx *dirs) {
    long long position = from;
    while (position < to) {
        long long next = next_cut(prep->cuts[part], prep->cut_count[part], position);
        if (next > to) {
            next = to;
        }
        write_dirs_through(dirs, buf, position);
        Segment rest = {position, next, REST_SEGMENT, 0, false, false, false};
        write_segment(prep, buf, &rest, part, staff, voice_number);
        position = next;
    }
}

/* One part's measure m: every voice that sounds in it, then, for the first, a rest to the barline. */
static void write_measure_body(const Prep *prep, XmlBuf *buf, size_t part, size_t m, const SegmentList *lists,
                               size_t *cursor, size_t *dir_pos) {
    long long measure_start = prep->measure_start[m];
    long long measure_end = prep->measure_end[m];
    long long head = measure_start;
    bool first = true;
    DirCtx dir_ctx = {prep, part, dir_pos, measure_start};

    for (size_t g = 0; g < prep->group_count; g++) {
        const Group *group = &prep->groups[g];
        if (group->part != part) {
            continue;
        }
        const SegmentList *list = &lists[g];
        while (cursor[g] < list->count && list->items[cursor[g]].end <= measure_start) {
            cursor[g]++;
        }
        size_t last = cursor[g];
        bool has_note = false;
        while (last < list->count && list->items[last].start < measure_end) {
            has_note = has_note || list->items[last].note != REST_SEGMENT;
            last++;
        }
        if (!has_note) {
            continue;
        }
        if (!first && head > measure_start) {
            xmlbuf_append(buf, "      <backup>\n        <duration>%lld</duration>\n      </backup>\n",
                          head - measure_start);
        }
        head = measure_start;
        for (size_t i = cursor[g]; i < last; i++) {
            if (first) {
                write_dirs_through(&dir_ctx, buf, list->items[i].start);
            }
            write_segment(prep, buf, &list->items[i], part, group->staff, group->voice_number);
            head = list->items[i].end;
        }
        if (first && head < measure_end) {
            write_rest_run(prep, buf, part, head, measure_end, group->staff, group->voice_number, &dir_ctx);
            head = measure_end;
        }
        first = false;
        cursor[g] = last;
    }
    if (first && prep->note_count > 0) {
        write_rest_run(prep, buf, part, measure_start, measure_end, 1, 1, &dir_ctx);
    } else if (first) {
        /* No notes at all: the dirs alone, with <forward> between them. */
        size_t end = prep->dir_first[part + 1];
        while (*dir_pos < end && prep->dirs[*dir_pos].tick < measure_end) {
            long long tick = prep->dirs[*dir_pos].tick;
            if (tick > head) {
                xmlbuf_append(buf, "      <forward>\n        <duration>%lld</duration>\n      </forward>\n",
                              tick - head);
                head = tick;
            }
            write_dirs_through(&dir_ctx, buf, tick);
        }
    }
}

/* The measure's <attributes>: divisions and staves in the first, a <time> where a region of the MeterMap starts. */
static MusicXmlExportResult write_attributes(const Prep *prep, XmlBuf *buf, size_t part, size_t m, size_t dir_pos) {
    const MeterMapRegion *region = NULL;
    MeterMapResult status = meter_map_region_of_measure(prep->meter, prep->first_measure_number + (int)m, &region);
    if (status != METER_MAP_OK) {
        return from_meter_status(status);
    }
    long long region_start = 0;
    if (time_ticks(musical_time_numerator(meter_map_region_start(region)),
                   musical_time_denominator(meter_map_region_start(region)), prep->divisions,
                   &region_start) != TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    bool has_time = region_start == prep->measure_start[m];
    bool has_divisions = m == 0;
    bool has_staves = m == 0 && prep->parts[part].staff_count > 1;
    const MusicXmlExportKeySignature *key = NULL;
    for (size_t d = dir_pos; d < prep->dir_first[part + 1] && prep->dirs[d].tick <= prep->measure_start[m]; d++) {
        if (prep->dirs[d].kind == DIR_KEY && prep->dirs[d].tick == prep->measure_start[m]) {
            key = &prep->score->key_signatures[prep->dirs[d].source];
            break;
        }
    }
    if (!has_time && !has_divisions && !has_staves && key == NULL) {
        return MUSICXML_EXPORT_OK;
    }
    xmlbuf_append(buf, "      <attributes>\n");
    if (has_divisions) {
        xmlbuf_append(buf, "        <divisions>%lld</divisions>\n", prep->divisions);
    }
    if (key != NULL) {
        write_key(buf, key, "        ");
    }
    if (has_time) {
        const TimeSignature *signature = meter_map_region_time_signature(region);
        xmlbuf_append(buf,
                      "        <time>\n          <beats>%u</beats>\n          <beat-type>%u</beat-type>\n"
                      "        </time>\n",
                      time_signature_top_number(signature), time_signature_bottom_number(signature));
    }
    if (has_staves) {
        xmlbuf_append(buf, "        <staves>%d</staves>\n", prep->parts[part].staff_count);
    }
    xmlbuf_append(buf, "      </attributes>\n");
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult write_document(const Prep *prep, XmlBuf *buf) {
    SegmentList *lists = NULL;
    MusicXmlExportResult result = build_segments(prep, &lists);
    if (result != MUSICXML_EXPORT_OK) {
        return result;
    }
    size_t *cursor = (size_t *)calloc(prep->group_count > 0 ? prep->group_count : 1, sizeof(size_t));
    if (cursor == NULL) {
        segments_free(lists, prep->group_count);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }

    xmlbuf_append(buf,
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
                  "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                  "<score-partwise version=\"3.1\">\n");
    if (prep->meter_defaulted) {
        /* The layout is a default 4/4, not a meter anyone wrote: say so, so a reader can tell. */
        xmlbuf_append(buf,
                      "  <identification>\n    <miscellaneous>\n"
                      "      <miscellaneous-field name=\"ursatz:meter-defaulted\">true</miscellaneous-field>\n"
                      "    </miscellaneous>\n  </identification>\n");
    }
    xmlbuf_append(buf, "  <part-list>\n");
    for (size_t p = 0; p < prep->part_count; p++) {
        xmlbuf_append(buf, "    <score-part id=\"");
        xmlbuf_append_escaped(buf, prep->parts[p].id);
        xmlbuf_append(buf, "\">\n      <part-name>");
        xmlbuf_append_escaped(buf, prep->parts[p].name);
        xmlbuf_append(buf, "</part-name>\n    </score-part>\n");
    }
    xmlbuf_append(buf, "  </part-list>\n");
    size_t *dir_pos = (size_t *)calloc(1, sizeof(size_t));
    if (dir_pos == NULL) {
        free(cursor);
        segments_free(lists, prep->group_count);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count && result == MUSICXML_EXPORT_OK; p++) {
        *dir_pos = prep->dir_first[p];
        memset(cursor, 0, (prep->group_count > 0 ? prep->group_count : 1) * sizeof(size_t));
        xmlbuf_append(buf, "  <part id=\"");
        xmlbuf_append_escaped(buf, prep->parts[p].id);
        xmlbuf_append(buf, "\">\n");
        for (size_t m = 0; m < prep->measure_count && result == MUSICXML_EXPORT_OK; m++) {
            bool pickup = m == 0 && meter_map_has_pickup(prep->meter);
            xmlbuf_append(buf, "    <measure number=\"%d\"%s>\n", prep->first_measure_number + (int)m,
                          pickup ? " implicit=\"yes\"" : "");
            result = write_attributes(prep, buf, p, m, *dir_pos);
            if (result != MUSICXML_EXPORT_OK) {
                break;
            }
            write_measure_body(prep, buf, p, m, lists, cursor, dir_pos);
            xmlbuf_append(buf, "    </measure>\n");
        }
        xmlbuf_append(buf, "  </part>\n");
    }
    xmlbuf_append(buf, "</score-partwise>\n");
    free(dir_pos);
    free(cursor);
    segments_free(lists, prep->group_count);
    return result;
}

static MusicXmlExportResult write_score(const MusicXmlExportScore *score, char **out_xml, bool strict) {
    if (score == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }
    Prep prep;
    memset(&prep, 0, sizeof(prep));
    prep.score = score;
    prep.strict = strict;
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
        result = write_document(&prep, &buf);
        if (result == MUSICXML_EXPORT_OK && buf.failed) {
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

MusicXmlExportResult musicxml_export_write_score(const MusicXmlExportScore *score, char **out_xml) {
    return write_score(score, out_xml, false);
}

MusicXmlExportResult musicxml_export_write_score_strict(const MusicXmlExportScore *score, char **out_xml) {
    return write_score(score, out_xml, true);
}
