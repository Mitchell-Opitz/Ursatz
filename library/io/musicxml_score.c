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
    int midi_program; /* 0 to 127, or -1 for none */
} PartInfo;

#define MAX_TUPLET_ACTUAL 15 /* the most notes a written tuplet holds: a hint above it is ignored */

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
    unsigned actual; /* the notated tuplet of the event: actual notes in the time of normal, 0 and 0 for none */
    unsigned normal;
} NoteRec;

/* Something written between notes at a position: a key signature now, markings next. Sorted by part, tick, kind. */
typedef enum {
    DIR_KEY = 0,
    DIR_TEMPO,   /* source: the TempoMap region index */
    DIR_MARKING, /* source: the TempoMarking index */
    DIR_DYNAMIC, /* source: the Dynamics index */
    DIR_PEDAL_STOP,  /* source: the MusicXmlExportPedal index; a stop sorts before a start at one tick */
    DIR_PEDAL_START
} DirKind;

typedef struct {
    DirKind kind;
    size_t part;
    int staff;
    long raw_n; /* the exact position, kept until divisions are final */
    unsigned long raw_d;
    long long tick;
    size_t source; /* the index into the input array of its kind */
    bool written;  /* a pedal stop already written at the end of the last measure */
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
    long long *measure_beat; /* ticks in one beat of the measure: its bottom unit, or three of them in a compound meter */
    bool *measure_four_four; /* a full 4/4 measure, whose second half does not take a note across beat 3 */

    Dir *dirs;
    size_t dir_count;
    size_t *dir_first;     /* part_count + 1 entries: part p's dirs are dirs[dir_first[p]] up to dirs[dir_first[p+1]] */
    long long dir_max_tick; /* -1 when there are no dirs besides pedal stops */
    long long dir_stop_tick; /* the last pedal stop, -1 when there is none: a stop on a barline adds no measure after it */

    long long **cuts;   /* per part: every barline, ascending, where notes and rests are split */
    size_t *cut_count;

    ScoreQuantized quantized; /* the notated times of every event when score->quantize is set, else zeroed */
    ScoreClefChanges clefs;   /* the clef of every staff at measure 1 and each change, from score_clefs_infer */
} Prep;

static void prep_free(Prep *prep) {
    free(prep->parts);
    free(prep->notes);
    free(prep->groups);
    free(prep->dirs);
    free(prep->dir_first);
    free(prep->measure_start);
    free(prep->measure_end);
    free(prep->measure_beat);
    free(prep->measure_four_four);
    if (prep->cuts != NULL) {
        for (size_t p = 0; p < prep->part_count; p++) {
            free(prep->cuts[p]);
        }
    }
    free(prep->cuts);
    free(prep->cut_count);
    score_quantized_destroy(&prep->quantized);
    score_clef_changes_destroy(&prep->clefs);
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
        (score->pedal_count > 0 && score->pedals == NULL) ||
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
            prep->parts[i].midi_program =
                score->parts[i].midi_program >= 0 && score->parts[i].midi_program <= 127 ? score->parts[i].midi_program : -1;
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
    prep->parts[0].name = "";
    prep->parts[0].staff_count = staves;
    prep->parts[0].midi_program = -1;
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

/* Replaces the played start and duration of every note with the notated ones from score_quantize, which stay in
   prep->quantized for the clefs. */
static MusicXmlExportResult apply_quantize(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    ScoreQuantized *quantized = &prep->quantized;
    ScoreQuantizeResult status = score_quantize(score->events, score->count, score->part_index, score->staff,
                                                score->meter_map, score->quantize, quantized);
    if (status != SCORE_QUANTIZE_OK) {
        return status == SCORE_QUANTIZE_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    for (size_t i = 0; i < prep->note_count && result == MUSICXML_EXPORT_OK; i++) {
        Rational start;
        Rational end;
        Rational duration;
        if (quantized->start_num[i] > (unsigned long long)LLONG_MAX || quantized->start_den[i] > (unsigned long long)LLONG_MAX ||
            quantized->end_num[i] > (unsigned long long)LLONG_MAX || quantized->end_den[i] > (unsigned long long)LLONG_MAX ||
            rational_make((long long)quantized->start_num[i], (long long)quantized->start_den[i], &start) != RATIONAL_OK ||
            rational_make((long long)quantized->end_num[i], (long long)quantized->end_den[i], &end) != RATIONAL_OK ||
            rational_cmp(end, start) <= 0 || rational_sub(end, start, &duration) != RATIONAL_OK ||
            start.num > (long long)LONG_MAX || duration.num > (long long)LONG_MAX ||
            (unsigned long long)start.den > ULONG_MAX || (unsigned long long)duration.den > ULONG_MAX) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
            break;
        }
        prep->notes[i].start_n = (long)start.num;
        prep->notes[i].start_d = (unsigned long)start.den;
        prep->notes[i].dur_n = (long)duration.num;
        prep->notes[i].dur_d = (unsigned long)duration.den;
    }
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
        note->start_n = (long)position->value.num;
        note->start_d = (unsigned long)position->value.den;
        note->dur_n = (long)duration->value.num;
        note->dur_d = (unsigned long)duration->value.den;
        if (note->start_n < 0 || note->dur_n <= 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        unsigned actual = 0;
        unsigned normal = 0;
        if (note_event_tuplet(event, &actual, &normal) && actual <= MAX_TUPLET_ACTUAL) {
            note->actual = actual;
            note->normal = normal;
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
        if (key->part_index >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (key->sharps_flats < -7 || key->sharps_flats > 7 || key->position.value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_KEY;
        dir->part = key->part_index;
        dir->staff = 1;
        dir->raw_n = (long)key->position.value.num;
        dir->raw_d = (unsigned long)key->position.value.den;
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
        MusicalTime region_start = tempo_map_region_start(region);
        dir->raw_n = (long)region_start.value.num;
        dir->raw_d = (unsigned long)region_start.value.den;
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
        if (position->value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_MARKING;
        dir->part = part;
        dir->staff = 1;
        dir->raw_n = (long)position->value.num;
        dir->raw_d = (unsigned long)position->value.den;
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

/* Where a Dynamics sounds: the start of its span, or the start of the event it targets. */
static MusicXmlExportResult dynamics_position(const Prep *prep, const Dynamics *dynamics, long *n, unsigned long *d) {
    const MusicalTime *position = NULL;
    if (dynamics_target_kind(dynamics) == DYNAMICS_TARGET_TIME_SPAN) {
        position = &dynamics_target_time_span(dynamics)->start;
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
    if (position->value.num < 0) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    *n = (long)position->value.num;
    *d = (unsigned long)position->value.den;
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

/* The sustain pedals: a start at the span's start and a stop at its end, in the staff of the part they name. */
static MusicXmlExportResult gather_pedals(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->pedal_count; i++) {
        const MusicXmlExportPedal *pedal = &score->pedals[i];
        if (pedal->part_index >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (pedal->staff < 1 || pedal->staff > prep->parts[pedal->part_index].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        if (!time_span_holds(pedal->span) || pedal->span.start.value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        for (int end = 0; end < 2; end++) {
            const MusicalTime *at = end == 0 ? &pedal->span.start : &pedal->span.end;
            Dir *dir = &prep->dirs[prep->dir_count++];
            dir->kind = end == 0 ? DIR_PEDAL_START : DIR_PEDAL_STOP;
            dir->part = pedal->part_index;
            dir->staff = pedal->staff;
            dir->raw_n = (long)at->value.num;
            dir->raw_d = (unsigned long)at->value.den;
            dir->source = i;
        }
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_dirs(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    size_t capacity = score->key_signature_count + score->tempo_marking_count + score->dynamics_count + 2 * score->pedal_count;
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
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_pedals(prep);
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
    prep->dir_stop_tick = -1;
    for (size_t i = 0; i < prep->dir_count; i++) {
        Dir *dir = &prep->dirs[i];
        if (time_ticks(dir->raw_n, dir->raw_d, prep->divisions, &dir->tick) != TIME_OK) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (dir->kind == DIR_PEDAL_STOP) {
            if (dir->tick > prep->dir_stop_tick) {
                prep->dir_stop_tick = dir->tick;
            }
        } else if (dir->tick > prep->dir_max_tick) {
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
    if (prep->dir_stop_tick > 0 && __builtin_mul_overflow(prep->dir_stop_tick, factor, &prep->dir_stop_tick)) {
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
    if (prep->dir_stop_tick > covered) {
        covered = prep->dir_stop_tick; /* a pedal stop on a barline ends the measure before it */
    }
    MusicalTime extent_time;
    if (musical_time_make((long)covered, (long long)(4 * prep->divisions), &extent_time) != MUSICAL_TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    size_t count = 0;
    MeterMapResult status = meter_map_measure_count(prep->meter, &extent_time, &count);
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
    prep->measure_beat = (long long *)calloc(count, sizeof(long long));
    prep->measure_four_four = (bool *)calloc(count, sizeof(bool));
    unsigned long *beat_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    unsigned long *beat_n = (unsigned long *)calloc(count, sizeof(unsigned long));
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    if (start_n == NULL || start_d == NULL || end_n == NULL || end_d == NULL || prep->measure_start == NULL ||
        prep->measure_end == NULL || prep->measure_beat == NULL || prep->measure_four_four == NULL || beat_n == NULL ||
        beat_d == NULL) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }

    long long divisions = prep->divisions;
    for (size_t m = 0; m < count && result == MUSICXML_EXPORT_OK; m++) {
        TimeSpan span;
        status = meter_map_measure_span(prep->meter, prep->first_measure_number + (int)m, &span);
        if (status != METER_MAP_OK) {
            result = from_meter_status(status);
            break;
        }
        start_n[m] = (long)span.start.value.num;
        start_d[m] = (unsigned long)span.start.value.den;
        end_n[m] = (long)span.end.value.num;
        end_d[m] = (unsigned long)span.end.value.den;
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
        if (time_status == TIME_OK) {
            /* the beat, so that a split at a beat falls on a whole tick (the quantizer's beat: see score_quantize.c) */
            const MeterMapRegion *region = NULL;
            status = meter_map_region_of_measure(prep->meter, prep->first_measure_number + (int)m, &region);
            if (status != METER_MAP_OK) {
                result = from_meter_status(status);
                break;
            }
            const TimeSignature *signature = meter_map_region_time_signature(region);
            unsigned top = time_signature_top_number(signature);
            unsigned bottom = time_signature_bottom_number(signature);
            Duration beat_length;
            if (time_signature_beat_length(signature, &beat_length) != DURATION_OK ||
                beat_length.value.num > LONG_MAX || beat_length.value.den > LONG_MAX) {
                result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
                break;
            }
            beat_n[m] = (unsigned long)beat_length.value.num;
            beat_d[m] = (unsigned long)beat_length.value.den;
            prep->measure_four_four[m] = top == 4 && bottom == 4;
            time_status = time_denominator((long)beat_n[m], beat_d[m], &den);
            if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
                time_status = TIME_DIVISIONS_OVERFLOW;
            }
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
        if (result == MUSICXML_EXPORT_OK &&
            (time_ticks((long)beat_n[m], beat_d[m], prep->divisions, &prep->measure_beat[m]) != TIME_OK ||
             prep->measure_beat[m] <= 0)) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (result == MUSICXML_EXPORT_OK) {
            prep->measure_four_four[m] = prep->measure_four_four[m] &&
                                         prep->measure_end[m] - prep->measure_start[m] == 4 * prep->measure_beat[m];
        }
    }
    free(start_n);
    free(start_d);
    free(end_n);
    free(end_d);
    free(beat_n);
    free(beat_d);
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

/* The clef of every staff at measure 1 and each change, from the notes as they are written (the notated times when the
   score is quantized). */
static MusicXmlExportResult infer_clefs(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    int *staff_counts = (int *)calloc(prep->part_count, sizeof(int));
    if (staff_counts == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count; p++) {
        staff_counts[p] = prep->parts[p].staff_count;
    }
    ScoreClefResult status = score_clefs_infer(score->events, score->pitch_values, score->count, score->part_index,
                                               score->staff, staff_counts, prep->part_count, prep->meter,
                                               score->quantize != NULL ? &prep->quantized : NULL, &prep->clefs);
    free(staff_counts);
    if (status == SCORE_CLEFS_OK) {
        return MUSICXML_EXPORT_OK;
    }
    return status == SCORE_CLEFS_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
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
        result = infer_clefs(prep);
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

#define MAX_BEAM_LEVELS 5 /* an eighth has one beam, a 128th five */

enum { BEAM_NONE, BEAM_BEGIN, BEAM_CONTINUE, BEAM_END, BEAM_FORWARD_HOOK, BEAM_BACKWARD_HOOK };
static const char *const BEAM_NAMES[] = {NULL, "begin", "continue", "end", "forward hook", "backward hook"};

typedef struct {
    long long start;
    long long end;
    size_t note;      /* an index into prep->notes, or REST_SEGMENT */
    int fragment;     /* 0 for the first piece of a note */
    bool tie_start;
    bool tie_stop;
    bool chord;       /* written with <chord/>, sharing the previous note's start and duration */
    bool measure_rest; /* a rest that fills its measure, written as <rest measure="yes"/> */
    int tuplet;        /* 1 opens a triplet group and 2 closes it (with <tuplet>), 0 neither */
    int beam_group;    /* 0 when not beamed, else the number of the beat group in the measure (from 1) */
    int beam_levels;   /* beams a beamed note carries: 1 for an eighth, 2 for a 16th, and so on */
    unsigned char beam[MAX_BEAM_LEVELS]; /* the state of beam level n + 1 on this note: a BEAM_* value */
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

/* --- notated values: which lengths a note or rest can be written as --- */

#define VALUE_TYPE_COUNT 8     /* whole to 128th */
#define TRIPLET_TYPE_COUNT 7   /* whole to 64th */
#define MAX_VALUE_DOTS 2

/* A length written as a note: type is 1 (whole) to 8 (128th), dots 0 to 2, tuplet when it is actual/normal of the plain
   value (the inferred triplet is 3:2, a hinted one is the note's own ratio). */
typedef struct {
    int type;
    int dots;
    bool tuplet;
    unsigned actual;
    unsigned normal;
} NotatedValue;

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
static bool classify_piece(const Prep *prep, size_t note, long long length, bool allow_triplet, NotatedValue *out) {
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
static bool add_run(const Prep *prep, size_t part, SegmentList *list, long long from, long long to, size_t note,
                    size_t members) {
    long long position = from;
    int fragment = 0;
    while (position < to) {
        long long next = next_cut(prep->cuts[part], prep->cut_count[part], position);
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
                if (!segment_push(list, segment)) {
                    return false;
                }
            }
            fragment++;
            position = end;
        }
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

/* The one id formatter. With cut set an id longer than out is cut to fit (what the export writes), otherwise it fails. */
static bool format_note_xml_id(const char *entity_value, char *out, size_t out_size, bool cut) {
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
    return format_note_xml_id(entity_value, out, out_size, false);
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
static void mark_tuplets(const Prep *prep, SegmentList *list) {
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
            classify_piece(prep, item->note, item->end - item->start, true, &value) && value.tuplet) {
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
static void mark_beams(const Prep *prep, SegmentList *list) {
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
            classify_piece(prep, item->note, item->end - item->start, true, &value) && value.type >= 4) {
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
static void write_segment(const Prep *prep, XmlBuf *buf, const Segment *segment, size_t part, int staff,
                          int voice_number) {
    bool is_rest = segment->note == REST_SEGMENT;
    const NoteRec *note = is_rest ? NULL : &prep->notes[segment->note];
    NoteEvent *event = is_rest ? NULL : prep->score->events[note->index];

    if (is_rest) {
        xmlbuf_append(buf, segment->measure_rest ? "      <note>\n        <rest measure=\"yes\"/>\n"
                                                 : "      <note>\n        <rest/>\n");
    } else {
        char base[64];
        const char *chord = segment->chord ? "        <chord/>\n" : "";
        format_note_xml_id(entity_id_value(note_event_id(event)), base, sizeof(base), true);
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
    NotatedValue value;
    if (!segment->measure_rest && classify_piece(prep, segment->note, segment->end - segment->start, true, &value)) {
        xmlbuf_append(buf, "        <type>%s</type>\n", VALUE_TYPE_NAMES[value.type - 1]);
        for (int d = 0; d < value.dots; d++) {
            xmlbuf_append(buf, "        <dot/>\n");
        }
        if (value.tuplet) {
            xmlbuf_append(buf, "        <time-modification>\n          <actual-notes>%u</actual-notes>\n"
                               "          <normal-notes>%u</normal-notes>\n        </time-modification>\n",
                          value.actual, value.normal);
        }
    }
    if (prep->parts[part].staff_count > 1) {
        xmlbuf_append(buf, "        <staff>%d</staff>\n", staff);
    }
    for (int level = 0; level < segment->beam_levels && level < MAX_BEAM_LEVELS && !segment->chord; level++) {
        if (segment->beam[level] != BEAM_NONE) {
            xmlbuf_append(buf, "        <beam number=\"%d\">%s</beam>\n", level + 1, BEAM_NAMES[segment->beam[level]]);
        }
    }
    if (segment->tie_stop || segment->tie_start || segment->tuplet != 0) {
        xmlbuf_append(buf, "        <notations>\n");
        if (segment->tie_stop) {
            xmlbuf_append(buf, "          <tied type=\"stop\"/>\n");
        }
        if (segment->tie_start) {
            xmlbuf_append(buf, "          <tied type=\"start\"/>\n");
        }
        if (segment->tuplet != 0) {
            xmlbuf_append(buf, "          <tuplet type=\"%s\"/>\n", segment->tuplet == 1 ? "start" : "stop");
        }
        xmlbuf_append(buf, "        </notations>\n");
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

/* The text of <beats>: the top number, or the beat groups joined by "+" ("3+2") when they differ in size. */
static void write_beats(const TimeSignature *signature, char *out, size_t capacity) {
    size_t count = time_signature_group_count(signature);
    bool equal = true;
    for (size_t i = 1; i < count; i++) {
        equal = equal && time_signature_group_at(signature, i) == time_signature_group_at(signature, 0);
    }
    if (equal) {
        snprintf(out, capacity, "%u", time_signature_top_number(signature));
        return;
    }
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count && used + 1 < capacity; i++) {
        int written = snprintf(out + used, capacity - used, i == 0 ? "%u" : "+%u", time_signature_group_at(signature, i));
        if (written < 0 || (size_t)written >= capacity - used) {
            break;
        }
        used += (size_t)written;
    }
}

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

/* A sustain pedal marking. */
static void write_pedal(const Prep *prep, XmlBuf *buf, const Dir *dir) {
    xmlbuf_append(buf, "      <direction placement=\"below\">\n        <direction-type>\n          <pedal type=\"%s\" line=\"yes\"/>\n        </direction-type>\n",
                  dir->kind == DIR_PEDAL_START ? "start" : "stop");
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
    for (size_t i = 0; i < count; i++) {
        if ((group[i].kind == DIR_PEDAL_STOP || group[i].kind == DIR_PEDAL_START) && !group[i].written) {
            write_pedal(prep, buf, &group[i]);
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

/*
 * The last measure's end: a pedal stop on the final barline has no segment after it to be written before, and adds no measure, so
 * it is written after the measure's first voice, where the writer is at the end of the measure. (Every other time of a marking is
 * a cut of its part, so a segment starts there.) Marks it written.
 */
static void flush_pedals(const DirCtx *ctx, XmlBuf *buf, long long measure_end, bool last_measure) {
    if (!last_measure) {
        return;
    }
    const Prep *prep = ctx->prep;
    for (size_t d = *ctx->pos; d < prep->dir_first[ctx->part + 1]; d++) {
        Dir *dir = &prep->dirs[d];
        if (dir->kind == DIR_PEDAL_STOP && !dir->written && dir->tick == measure_end) {
            write_pedal(prep, buf, dir);
            dir->written = true;
        }
    }
}

/* Appends the rests that fill [from, to): cut at the part's cuts and into notated values. A whole measure with no cut
   inside it is one measure rest. */
static bool rest_pieces(const Prep *prep, size_t part, SegmentList *list, long long from, long long to,
                        bool whole_measure) {
    if (whole_measure && next_cut(prep->cuts[part], prep->cut_count[part], from) >= to) {
        Segment rest = {from, to, REST_SEGMENT, 0, false, false, false, true, 0, 0, 0, {0}};
        return segment_push(list, rest);
    }
    return add_run(prep, part, list, from, to, REST_SEGMENT, 1);
}

/* Writes the segments in order; the part's dirs go in before each one when dirs is given. */
static void write_segments(const Prep *prep, XmlBuf *buf, const SegmentList *list, size_t part, int staff,
                           int voice_number, DirCtx *dirs) {
    for (size_t i = 0; i < list->count; i++) {
        write_dirs_through(dirs, buf, list->items[i].start);
        write_segment(prep, buf, &list->items[i], part, staff, voice_number);
    }
}

/* --- the layout: what each part writes in each measure --- */

/* One voice's segments of one measure of one part, with the rests that pad the first one to the barline and the tuplet and beam marks. */
typedef struct {
    size_t group;
    int staff;
    int voice_number;
    long long backup; /* ticks the document steps back before this voice (0 for the first voice and when it begins where the last ended) */
    SegmentList items;
} LayoutChain;

/* What one part writes in one measure: its voices, or, when it has no note there, the rest that fills the measure. */
typedef struct {
    LayoutChain *chains;
    size_t count;
    SegmentList rests; /* only when count is 0 and the score has notes */
} LayoutCell;

struct ScoreLayoutPrepared {
    Prep prep;
    LayoutCell *cells; /* part_count * measure_count, part-major */
};

static bool chain_push(LayoutCell *cell, const LayoutChain *chain) {
    LayoutChain *grown = (LayoutChain *)realloc(cell->chains, (cell->count + 1) * sizeof(LayoutChain));
    if (grown == NULL) {
        return false;
    }
    cell->chains = grown;
    cell->chains[cell->count++] = *chain;
    return true;
}

static void cell_free(LayoutCell *cell) {
    for (size_t i = 0; i < cell->count; i++) {
        free(cell->chains[i].items.items);
    }
    free(cell->chains);
    free(cell->rests.items);
    memset(cell, 0, sizeof(*cell));
}

/* One part's measure m: every voice that sounds in it, then, for the first, a rest to the barline. false on allocation failure. */
static bool build_cell(const Prep *prep, size_t part, size_t m, const SegmentList *lists, size_t *cursor, LayoutCell *cell) {
    long long measure_start = prep->measure_start[m];
    long long measure_end = prep->measure_end[m];
    long long head = measure_start;
    bool first = true;

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
        LayoutChain chain = {g, group->staff, group->voice_number, 0, {NULL, 0, 0}};
        if (!first && head > measure_start) {
            chain.backup = head - measure_start;
        }
        head = measure_start;
        bool ok = true;
        for (size_t i = cursor[g]; i < last; i++) {
            ok = ok && segment_push(&chain.items, list->items[i]);
            head = list->items[i].end;
        }
        if (first && head < measure_end) {
            ok = ok && rest_pieces(prep, part, &chain.items, head, measure_end, false);
            head = measure_end;
        }
        if (!ok) {
            free(chain.items.items);
            return false;
        }
        mark_tuplets(prep, &chain.items);
        mark_beams(prep, &chain.items);
        if (!chain_push(cell, &chain)) {
            free(chain.items.items);
            return false;
        }
        first = false;
        cursor[g] = last;
    }
    if (first && prep->note_count > 0) {
        return rest_pieces(prep, part, &cell->rests, measure_start, measure_end, true);
    }
    return true;
}

/* One part's measure m, printed from its cell: the voices (the dirs of the part go in with the first), or the rest, or the dirs alone. */
static void write_measure_body(const Prep *prep, XmlBuf *buf, size_t part, size_t m, const LayoutCell *cell, size_t *dir_pos) {
    long long measure_start = prep->measure_start[m];
    long long measure_end = prep->measure_end[m];
    long long head = measure_start;
    DirCtx dir_ctx = {prep, part, dir_pos, measure_start};

    for (size_t i = 0; i < cell->count; i++) {
        const LayoutChain *chain = &cell->chains[i];
        if (chain->backup > 0) {
            xmlbuf_append(buf, "      <backup>\n        <duration>%lld</duration>\n      </backup>\n", chain->backup);
        }
        write_segments(prep, buf, &chain->items, part, chain->staff, chain->voice_number, i == 0 ? &dir_ctx : NULL);
        if (i == 0) {
            flush_pedals(&dir_ctx, buf, measure_end, m + 1 == prep->measure_count);
        }
    }
    if (cell->count == 0 && prep->note_count > 0) {
        write_segments(prep, buf, &cell->rests, part, 1, 1, &dir_ctx);
        flush_pedals(&dir_ctx, buf, measure_end, m + 1 == prep->measure_count);
    } else if (cell->count == 0) {
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
        if (m + 1 == prep->measure_count) {
            write_dirs_through(&dir_ctx, buf, measure_end);
        }
    }
}

/* True when the part has a clef entry (the first measure's or a change) at measure m. */
static bool has_clefs_at(const Prep *prep, size_t part, size_t m) {
    for (size_t i = 0; i < prep->clefs.count; i++) {
        if (prep->clefs.part[i] == part && prep->clefs.measure[i] == m + 1) {
            return true;
        }
    }
    return false;
}

/* The measure's <attributes>: divisions and staves in the first, a <time> where a region of the MeterMap starts, and
   the <clef> of each staff in the first measure and of each staff that changes clef. */
static MusicXmlExportResult write_attributes(const Prep *prep, XmlBuf *buf, size_t part, size_t m, size_t dir_pos) {
    const MeterMapRegion *region = NULL;
    MeterMapResult status = meter_map_region_of_measure(prep->meter, prep->first_measure_number + (int)m, &region);
    if (status != METER_MAP_OK) {
        return from_meter_status(status);
    }
    long long region_start = 0;
    MusicalTime region_time = meter_map_region_start(region);
    if (time_ticks((long)region_time.value.num, (unsigned long)region_time.value.den, prep->divisions,
                   &region_start) != TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    bool has_time = region_start == prep->measure_start[m];
    bool has_divisions = m == 0;
    bool has_staves = m == 0 && prep->parts[part].staff_count > 1;
    bool has_clefs = has_clefs_at(prep, part, m);
    const MusicXmlExportKeySignature *key = NULL;
    for (size_t d = dir_pos; d < prep->dir_first[part + 1] && prep->dirs[d].tick <= prep->measure_start[m]; d++) {
        if (prep->dirs[d].kind == DIR_KEY && prep->dirs[d].tick == prep->measure_start[m]) {
            key = &prep->score->key_signatures[prep->dirs[d].source];
            break;
        }
    }
    if (!has_time && !has_divisions && !has_staves && key == NULL && !has_clefs) {
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
        char beats[TIME_SIGNATURE_MAX_TOP_NUMBER * 3 + 8];
        write_beats(signature, beats, sizeof(beats));
        xmlbuf_append(buf,
                      "        <time>\n          <beats>%s</beats>\n          <beat-type>%u</beat-type>\n"
                      "        </time>\n",
                      beats, time_signature_bottom_number(signature));
    }
    if (has_staves) {
        xmlbuf_append(buf, "        <staves>%d</staves>\n", prep->parts[part].staff_count);
    }
    for (size_t entry = 0; entry < prep->clefs.count; entry++) {
        if (prep->clefs.part[entry] != part || prep->clefs.measure[entry] != m + 1) {
            continue;
        }
        if (prep->parts[part].staff_count > 1) {
            xmlbuf_append(buf, "        <clef number=\"%d\">\n", prep->clefs.staff[entry]);
        } else {
            xmlbuf_append(buf, "        <clef>\n");
        }
        bool treble = prep->clefs.clef[entry] == SCORE_CLEF_TREBLE;
        xmlbuf_append(buf, "          <sign>%s</sign>\n          <line>%d</line>\n        </clef>\n", treble ? "G" : "F",
                      treble ? 2 : 4);
    }
    xmlbuf_append(buf, "      </attributes>\n");
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult write_document(const struct ScoreLayoutPrepared *prepared, XmlBuf *buf) {
    const Prep *prep = &prepared->prep;
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    xmlbuf_append(buf,
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
                  "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                  "<score-partwise version=\"3.1\">\n");
    const MusicXmlExportScore *score = prep->score;
    bool has_title = score->title != NULL && score->title[0] != '\0';
    bool has_composer = score->composer != NULL && score->composer[0] != '\0';
    if (has_title) {
        xmlbuf_append(buf, "  <work>\n    <work-title>");
        xmlbuf_append_escaped(buf, score->title);
        xmlbuf_append(buf, "</work-title>\n  </work>\n");
    }
    if (has_composer || prep->meter_defaulted) {
        xmlbuf_append(buf, "  <identification>\n");
        if (has_composer) {
            xmlbuf_append(buf, "    <creator type=\"composer\">");
            xmlbuf_append_escaped(buf, score->composer);
            xmlbuf_append(buf, "</creator>\n");
        }
        if (prep->meter_defaulted) {
            /* The layout is a default 4/4, not a meter anyone wrote: say so, so a reader can tell. */
            xmlbuf_append(buf,
                          "    <miscellaneous>\n"
                          "      <miscellaneous-field name=\"ursatz:meter-defaulted\">true</miscellaneous-field>\n"
                          "    </miscellaneous>\n");
        }
        xmlbuf_append(buf, "  </identification>\n");
    }
    xmlbuf_append(buf, "  <part-list>\n");
    for (size_t p = 0; p < prep->part_count; p++) {
        xmlbuf_append(buf, "    <score-part id=\"");
        xmlbuf_append_escaped(buf, prep->parts[p].id);
        xmlbuf_append(buf, prep->parts[p].name[0] == '\0' ? "\">\n      <part-name print-object=\"no\">"
                                                          : "\">\n      <part-name>");
        xmlbuf_append_escaped(buf, prep->parts[p].name);
        xmlbuf_append(buf, "</part-name>\n");
        if (prep->parts[p].midi_program >= 0) {
            xmlbuf_append(buf, "      <score-instrument id=\"");
            xmlbuf_append_escaped(buf, prep->parts[p].id);
            xmlbuf_append(buf, "-I1\">\n        <instrument-name>");
            xmlbuf_append_escaped(buf, prep->parts[p].name);
            xmlbuf_append(buf, "</instrument-name>\n      </score-instrument>\n      <midi-instrument id=\"");
            xmlbuf_append_escaped(buf, prep->parts[p].id);
            xmlbuf_append(buf, "-I1\">\n        <midi-program>%d</midi-program>\n      </midi-instrument>\n",
                          prep->parts[p].midi_program + 1);
        }
        xmlbuf_append(buf, "    </score-part>\n");
    }
    xmlbuf_append(buf, "  </part-list>\n");
    size_t *dir_pos = (size_t *)calloc(1, sizeof(size_t));
    if (dir_pos == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count && result == MUSICXML_EXPORT_OK; p++) {
        *dir_pos = prep->dir_first[p];
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
            write_measure_body(prep, buf, p, m, &prepared->cells[p * prep->measure_count + m], dir_pos);
            xmlbuf_append(buf, "    </measure>\n");
        }
        xmlbuf_append(buf, "  </part>\n");
    }
    xmlbuf_append(buf, "</score-partwise>\n");
    free(dir_pos);
    return result;
}

/* --- building and reading the layout --- */

static void prepared_free(struct ScoreLayoutPrepared *prepared) {
    if (prepared == NULL) {
        return;
    }
    if (prepared->cells != NULL) {
        for (size_t i = 0; i < prepared->prep.part_count * prepared->prep.measure_count; i++) {
            cell_free(&prepared->cells[i]);
        }
    }
    free(prepared->cells);
    prep_free(&prepared->prep);
    free(prepared);
}

void score_layout_destroy(ScoreLayout *layout) {
    if (layout == NULL) {
        return;
    }
    for (size_t m = 0; m < layout->measure_count; m++) {
        free(layout->measures[m].clefs);
        free(layout->measures[m].events);
    }
    free(layout->measures);
    prepared_free(layout->prepared);
    memset(layout, 0, sizeof(*layout));
}

/* The event of one segment, as write_segment writes it. */
static void fill_event(const Prep *prep, const Segment *segment, size_t part, int staff, int voice_number, ScoreLayoutEvent *out) {
    memset(out, 0, sizeof(*out));
    out->part = part;
    out->staff = staff;
    out->voice = voice_number;
    out->start = segment->start;
    out->duration = segment->end - segment->start;
    out->chord = segment->chord;
    out->tie_start = segment->tie_start;
    out->tie_stop = segment->tie_stop;
    out->tuplet_mark = segment->tuplet;
    if (segment->note == REST_SEGMENT) {
        out->rest = true;
        out->measure_rest = segment->measure_rest;
    } else {
        const NoteRec *note = &prep->notes[segment->note];
        NoteEvent *event = prep->score->events[note->index];
        char base[64];
        format_note_xml_id(entity_id_value(note_event_id(event)), base, sizeof(base), true);
        if (segment->tie_start || segment->tie_stop) {
            if (segment->fragment < 26) {
                snprintf(out->xml_id, sizeof(out->xml_id), "%s_%c", base, 'a' + segment->fragment);
            } else {
                snprintf(out->xml_id, sizeof(out->xml_id), "%s_%d", base, segment->fragment);
            }
        } else {
            snprintf(out->xml_id, sizeof(out->xml_id), "%s", base);
        }
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(event);
        out->step = PITCH_STEP_NAMES[pitch_spelling_letter(spelling)][0];
        out->alter = pitch_spelling_accidental(spelling);
        out->octave = pitch_spelling_octave(spelling);
    }
    NotatedValue value;
    if (!segment->measure_rest && classify_piece(prep, segment->note, segment->end - segment->start, true, &value)) {
        out->value = value.type;
        out->dots = value.dots;
        if (value.tuplet) {
            out->tuplet_actual = value.actual;
            out->tuplet_normal = value.normal;
        }
    }
}

static size_t cell_event_count(const LayoutCell *cell) {
    size_t count = cell->rests.count;
    for (size_t i = 0; i < cell->count; i++) {
        count += cell->chains[i].items.count;
    }
    return count;
}

/* The measure m's own members: number, span, time signature and clefs. */
static MusicXmlExportResult fill_measure(const Prep *prep, size_t m, ScoreLayoutMeasure *out) {
    out->number = prep->first_measure_number + (int)m;
    out->implicit = m == 0 && meter_map_has_pickup(prep->meter);
    out->start = prep->measure_start[m];
    out->end = prep->measure_end[m];
    const MeterMapRegion *region = NULL;
    MeterMapResult status = meter_map_region_of_measure(prep->meter, out->number, &region);
    if (status != METER_MAP_OK) {
        return from_meter_status(status);
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    out->beats = (int)time_signature_top_number(signature);
    out->beat_type = (int)time_signature_bottom_number(signature);
    MusicalTime region_time = meter_map_region_start(region);
    long long region_start = 0;
    if (time_ticks((long)region_time.value.num, (unsigned long)region_time.value.den, prep->divisions, &region_start) != TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    out->time_written = region_start == out->start;
    size_t clefs = 0;
    for (size_t i = 0; i < prep->clefs.count; i++) {
        clefs += prep->clefs.measure[i] == m + 1;
    }
    if (clefs > 0) {
        out->clefs = (ScoreLayoutClef *)calloc(clefs, sizeof(ScoreLayoutClef));
        if (out->clefs == NULL) {
            return MUSICXML_EXPORT_ERR_ALLOC;
        }
        for (size_t i = 0; i < prep->clefs.count; i++) {
            if (prep->clefs.measure[i] == m + 1) {
                out->clefs[out->clef_count++] = (ScoreLayoutClef){prep->clefs.part[i], prep->clefs.staff[i],
                                                                 prep->clefs.clef[i] != SCORE_CLEF_TREBLE};
            }
        }
    }
    return MUSICXML_EXPORT_OK;
}

MusicXmlExportResult score_layout_build(const MusicXmlExportScore *score, ScoreLayout *out) {
    if (score == NULL || out == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    struct ScoreLayoutPrepared *prepared = (struct ScoreLayoutPrepared *)calloc(1, sizeof(*prepared));
    if (prepared == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    Prep *prep = &prepared->prep;
    prep->score = score;
    MusicXmlExportResult result = prepare(prep);
    SegmentList *lists = NULL;
    size_t *cursor = NULL;
    if (result == MUSICXML_EXPORT_OK) {
        result = build_segments(prep, &lists);
    }
    if (result == MUSICXML_EXPORT_OK) {
        size_t cells = prep->part_count * prep->measure_count;
        prepared->cells = (LayoutCell *)calloc(cells > 0 ? cells : 1, sizeof(LayoutCell));
        cursor = (size_t *)calloc(prep->group_count > 0 ? prep->group_count : 1, sizeof(size_t));
        if (prepared->cells == NULL || cursor == NULL) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
    }
    for (size_t p = 0; p < prep->part_count && result == MUSICXML_EXPORT_OK; p++) {
        memset(cursor, 0, (prep->group_count > 0 ? prep->group_count : 1) * sizeof(size_t));
        for (size_t m = 0; m < prep->measure_count && result == MUSICXML_EXPORT_OK; m++) {
            if (!build_cell(prep, p, m, lists, cursor, &prepared->cells[p * prep->measure_count + m])) {
                result = MUSICXML_EXPORT_ERR_ALLOC;
            }
        }
    }
    if (lists != NULL) {
        segments_free(lists, prep->group_count);
    }
    free(cursor);
    if (result == MUSICXML_EXPORT_OK && prep->measure_count > 0) {
        out->measures = (ScoreLayoutMeasure *)calloc(prep->measure_count, sizeof(ScoreLayoutMeasure));
        if (out->measures == NULL) {
            result = MUSICXML_EXPORT_ERR_ALLOC;
        }
        out->measure_count = prep->measure_count;
    }
    for (size_t m = 0; m < prep->measure_count && result == MUSICXML_EXPORT_OK; m++) {
        ScoreLayoutMeasure *measure = &out->measures[m];
        result = fill_measure(prep, m, measure);
        size_t total = 0;
        for (size_t p = 0; p < prep->part_count; p++) {
            total += cell_event_count(&prepared->cells[p * prep->measure_count + m]);
        }
        if (result == MUSICXML_EXPORT_OK && total > 0) {
            measure->events = (ScoreLayoutEvent *)calloc(total, sizeof(ScoreLayoutEvent));
            result = measure->events == NULL ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_OK;
        }
        for (size_t p = 0; p < prep->part_count && result == MUSICXML_EXPORT_OK; p++) {
            const LayoutCell *cell = &prepared->cells[p * prep->measure_count + m];
            for (size_t c = 0; c < cell->count; c++) {
                const LayoutChain *chain = &cell->chains[c];
                for (size_t i = 0; i < chain->items.count; i++) {
                    fill_event(prep, &chain->items.items[i], p, chain->staff, chain->voice_number,
                               &measure->events[measure->event_count++]);
                }
            }
            for (size_t i = 0; i < cell->rests.count; i++) {
                fill_event(prep, &cell->rests.items[i], p, 1, 1, &measure->events[measure->event_count++]);
            }
        }
    }
    if (result != MUSICXML_EXPORT_OK) {
        out->prepared = prepared;
        score_layout_destroy(out);
        return result;
    }
    out->divisions = prep->divisions;
    out->part_count = prep->part_count;
    out->meter_defaulted = prep->meter_defaulted;
    out->prepared = prepared;
    return MUSICXML_EXPORT_OK;
}

MusicXmlExportResult musicxml_export_write_score(const MusicXmlExportScore *score, char **out_xml) {
    if (score == NULL || out_xml == NULL) {
        return MUSICXML_EXPORT_ERR_NULL_ARG;
    }
    ScoreLayout layout;
    MusicXmlExportResult result = score_layout_build(score, &layout);
    if (result != MUSICXML_EXPORT_OK) {
        return result;
    }
    XmlBuf buf;
    if (!xmlbuf_init(&buf)) {
        score_layout_destroy(&layout);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    result = write_document(layout.prepared, &buf);
    if (result == MUSICXML_EXPORT_OK && buf.failed) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }
    score_layout_destroy(&layout);
    if (result != MUSICXML_EXPORT_OK) {
        xmlbuf_free(&buf);
        return result;
    }
    *out_xml = buf.data;
    return MUSICXML_EXPORT_OK;
}
