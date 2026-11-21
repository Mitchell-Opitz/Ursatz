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
    musicxml_score_xmlbuf_append(buf, "%s<key>\n%s  <fifths>%d</fifths>\n", indent, indent, key->sharps_flats);
    if (key->mode == MUSICXML_EXPORT_MODE_MAJOR) {
        musicxml_score_xmlbuf_append(buf, "%s  <mode>major</mode>\n", indent);
    } else if (key->mode == MUSICXML_EXPORT_MODE_MINOR) {
        musicxml_score_xmlbuf_append(buf, "%s  <mode>minor</mode>\n", indent);
    }
    musicxml_score_xmlbuf_append(buf, "%s</key>\n", indent);
}

/* 60,000,000 / microseconds-per-quarter as a decimal: the quarter notes per minute of a <sound tempo>. */
static void tempo_text(const Prep *prep, size_t region, char *out, size_t size) {
    bool exact = false;
    unsigned long micro = tempo_map_region_microseconds_per_quarter(tempo_map_region(prep->score->tempo_map, region));
    if (!musicxml_score_ratio_to_decimal(60000000ull, micro, out, size, &exact)) {
        snprintf(out, size, "0");
    }
}

static void write_marking(const Prep *prep, XmlBuf *buf, const TempoMarking *marking, const char *sound_tempo) {
    musicxml_score_xmlbuf_append(buf, "      <direction placement=\"above\">\n");
    if (tempo_marking_has_text(marking)) {
        musicxml_score_xmlbuf_append(buf, "        <direction-type>\n          <words>");
        musicxml_score_xmlbuf_append_escaped(buf, tempo_marking_text(marking));
        musicxml_score_xmlbuf_append(buf, "</words>\n        </direction-type>\n");
    }
    if (tempo_marking_has_metronome_marking(marking)) {
        const MetronomeMarking *metronome = tempo_marking_metronome_marking(marking);
        const char *name = "quarter";
        int dots = 0;
        char per_minute[64];
        bool exact = false;
        musicxml_score_beat_unit_name(metronome_marking_beat_unit_numerator(metronome),
                       metronome_marking_beat_unit_denominator(metronome), &name, &dots);
        musicxml_score_ratio_to_decimal(metronome_marking_bpm_numerator(metronome), metronome_marking_bpm_denominator(metronome),
                         per_minute, sizeof(per_minute), &exact);
        musicxml_score_xmlbuf_append(buf, "        <direction-type>\n          <metronome>\n            <beat-unit>%s</beat-unit>\n",
                      name);
        for (int k = 0; k < dots; k++) {
            musicxml_score_xmlbuf_append(buf, "            <beat-unit-dot/>\n");
        }
        musicxml_score_xmlbuf_append(buf, "            <per-minute>%s</per-minute>\n          </metronome>\n        </direction-type>\n",
                      per_minute);
    }
    if (sound_tempo != NULL) {
        musicxml_score_xmlbuf_append(buf, "        <sound tempo=\"%s\"/>\n", sound_tempo);
    }
    musicxml_score_xmlbuf_append(buf, "      </direction>\n");
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
    musicxml_score_xmlbuf_append(buf, "      <direction placement=\"below\">\n        <direction-type>\n          <dynamics>");
    if (named) {
        musicxml_score_xmlbuf_append(buf, "<%s/>", marking);
    } else {
        musicxml_score_xmlbuf_append(buf, "<other-dynamics>");
        musicxml_score_xmlbuf_append_escaped(buf, marking);
        musicxml_score_xmlbuf_append(buf, "</other-dynamics>");
    }
    musicxml_score_xmlbuf_append(buf, "</dynamics>\n        </direction-type>\n");
    if (prep->parts[dir->part].staff_count > 1) {
        musicxml_score_xmlbuf_append(buf, "        <staff>%d</staff>\n", dir->staff);
    }
    musicxml_score_xmlbuf_append(buf, "      </direction>\n");
}

/* A sustain pedal marking. */
static void write_pedal(const Prep *prep, XmlBuf *buf, const Dir *dir) {
    musicxml_score_xmlbuf_append(buf, "      <direction placement=\"below\">\n        <direction-type>\n          <pedal type=\"%s\" line=\"yes\"/>\n        </direction-type>\n",
                  dir->kind == DIR_PEDAL_START ? "start" : "stop");
    if (prep->parts[dir->part].staff_count > 1) {
        musicxml_score_xmlbuf_append(buf, "        <staff>%d</staff>\n", dir->staff);
    }
    musicxml_score_xmlbuf_append(buf, "      </direction>\n");
}

/* The dirs that share one tick, in kind order. A key at the start of a measure is already in its <attributes>. */
static void write_dir_group(const Prep *prep, XmlBuf *buf, const Dir *group, size_t count, long long measure_start) {
    bool key_written = false;
    const Dir *tempo = NULL;
    for (size_t i = 0; i < count; i++) {
        if (group[i].kind == DIR_KEY && !key_written) {
            key_written = true;
            if (group[i].tick != measure_start) {
                musicxml_score_xmlbuf_append(buf, "      <attributes>\n");
                write_key(buf, &prep->score->key_signatures[group[i].source], "        ");
                musicxml_score_xmlbuf_append(buf, "      </attributes>\n");
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
        musicxml_score_xmlbuf_append(buf, "      <sound tempo=\"%s\"/>\n", tempo_value);
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
    if (whole_measure && musicxml_score_next_cut(prep->cuts[part], prep->cut_count[part], from) >= to) {
        Segment rest = {from, to, REST_SEGMENT, 0, false, false, false, true, 0, 0, 0, {0}};
        return musicxml_score_segment_push(list, rest);
    }
    return musicxml_score_add_run(prep, part, list, from, to, REST_SEGMENT, 1);
}

/* Writes the segments in order; the part's dirs go in before each one when dirs is given. */
static void write_segments(const Prep *prep, XmlBuf *buf, const SegmentList *list, size_t part, int staff,
                           int voice_number, DirCtx *dirs) {
    for (size_t i = 0; i < list->count; i++) {
        write_dirs_through(dirs, buf, list->items[i].start);
        musicxml_score_write_segment(prep, buf, &list->items[i], part, staff, voice_number);
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
            ok = ok && musicxml_score_segment_push(&chain.items, list->items[i]);
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
        musicxml_score_mark_tuplets(prep, &chain.items);
        musicxml_score_mark_beams(prep, &chain.items);
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
            musicxml_score_xmlbuf_append(buf, "      <backup>\n        <duration>%lld</duration>\n      </backup>\n", chain->backup);
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
                musicxml_score_xmlbuf_append(buf, "      <forward>\n        <duration>%lld</duration>\n      </forward>\n",
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
        return musicxml_score_from_meter_status(status);
    }
    long long region_start = 0;
    MusicalTime region_time = meter_map_region_start(region);
    if (musicxml_score_time_ticks((long)region_time.value.num, (unsigned long)region_time.value.den, prep->divisions,
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
    musicxml_score_xmlbuf_append(buf, "      <attributes>\n");
    if (has_divisions) {
        musicxml_score_xmlbuf_append(buf, "        <divisions>%lld</divisions>\n", prep->divisions);
    }
    if (key != NULL) {
        write_key(buf, key, "        ");
    }
    if (has_time) {
        const TimeSignature *signature = meter_map_region_time_signature(region);
        char beats[TIME_SIGNATURE_MAX_TOP_NUMBER * 3 + 8];
        write_beats(signature, beats, sizeof(beats));
        musicxml_score_xmlbuf_append(buf,
                      "        <time>\n          <beats>%s</beats>\n          <beat-type>%u</beat-type>\n"
                      "        </time>\n",
                      beats, time_signature_bottom_number(signature));
    }
    if (has_staves) {
        musicxml_score_xmlbuf_append(buf, "        <staves>%d</staves>\n", prep->parts[part].staff_count);
    }
    for (size_t entry = 0; entry < prep->clefs.count; entry++) {
        if (prep->clefs.part[entry] != part || prep->clefs.measure[entry] != m + 1) {
            continue;
        }
        if (prep->parts[part].staff_count > 1) {
            musicxml_score_xmlbuf_append(buf, "        <clef number=\"%d\">\n", prep->clefs.staff[entry]);
        } else {
            musicxml_score_xmlbuf_append(buf, "        <clef>\n");
        }
        bool treble = prep->clefs.clef[entry] == SCORE_CLEF_TREBLE;
        musicxml_score_xmlbuf_append(buf, "          <sign>%s</sign>\n          <line>%d</line>\n        </clef>\n", treble ? "G" : "F",
                      treble ? 2 : 4);
    }
    musicxml_score_xmlbuf_append(buf, "      </attributes>\n");
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult write_document(const struct ScoreLayoutPrepared *prepared, XmlBuf *buf) {
    const Prep *prep = &prepared->prep;
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    musicxml_score_xmlbuf_append(buf,
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
                  "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                  "<score-partwise version=\"3.1\">\n");
    const MusicXmlExportScore *score = prep->score;
    bool has_title = score->title != NULL && score->title[0] != '\0';
    bool has_composer = score->composer != NULL && score->composer[0] != '\0';
    if (has_title) {
        musicxml_score_xmlbuf_append(buf, "  <work>\n    <work-title>");
        musicxml_score_xmlbuf_append_escaped(buf, score->title);
        musicxml_score_xmlbuf_append(buf, "</work-title>\n  </work>\n");
    }
    if (has_composer || prep->meter_defaulted) {
        musicxml_score_xmlbuf_append(buf, "  <identification>\n");
        if (has_composer) {
            musicxml_score_xmlbuf_append(buf, "    <creator type=\"composer\">");
            musicxml_score_xmlbuf_append_escaped(buf, score->composer);
            musicxml_score_xmlbuf_append(buf, "</creator>\n");
        }
        if (prep->meter_defaulted) {
            /* The layout is a default 4/4, not a meter anyone wrote: say so, so a reader can tell. */
            musicxml_score_xmlbuf_append(buf,
                          "    <miscellaneous>\n"
                          "      <miscellaneous-field name=\"ursatz:meter-defaulted\">true</miscellaneous-field>\n"
                          "    </miscellaneous>\n");
        }
        musicxml_score_xmlbuf_append(buf, "  </identification>\n");
    }
    musicxml_score_xmlbuf_append(buf, "  <part-list>\n");
    for (size_t p = 0; p < prep->part_count; p++) {
        musicxml_score_xmlbuf_append(buf, "    <score-part id=\"");
        musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].id);
        musicxml_score_xmlbuf_append(buf, prep->parts[p].name[0] == '\0' ? "\">\n      <part-name print-object=\"no\">"
                                                          : "\">\n      <part-name>");
        musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].name);
        musicxml_score_xmlbuf_append(buf, "</part-name>\n");
        if (prep->parts[p].midi_program >= 0) {
            musicxml_score_xmlbuf_append(buf, "      <score-instrument id=\"");
            musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].id);
            musicxml_score_xmlbuf_append(buf, "-I1\">\n        <instrument-name>");
            musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].name);
            musicxml_score_xmlbuf_append(buf, "</instrument-name>\n      </score-instrument>\n      <midi-instrument id=\"");
            musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].id);
            musicxml_score_xmlbuf_append(buf, "-I1\">\n        <midi-program>%d</midi-program>\n      </midi-instrument>\n",
                          prep->parts[p].midi_program + 1);
        }
        musicxml_score_xmlbuf_append(buf, "    </score-part>\n");
    }
    musicxml_score_xmlbuf_append(buf, "  </part-list>\n");
    size_t *dir_pos = (size_t *)calloc(1, sizeof(size_t));
    if (dir_pos == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count && result == MUSICXML_EXPORT_OK; p++) {
        *dir_pos = prep->dir_first[p];
        musicxml_score_xmlbuf_append(buf, "  <part id=\"");
        musicxml_score_xmlbuf_append_escaped(buf, prep->parts[p].id);
        musicxml_score_xmlbuf_append(buf, "\">\n");
        for (size_t m = 0; m < prep->measure_count && result == MUSICXML_EXPORT_OK; m++) {
            bool pickup = m == 0 && meter_map_has_pickup(prep->meter);
            musicxml_score_xmlbuf_append(buf, "    <measure number=\"%d\"%s>\n", prep->first_measure_number + (int)m,
                          pickup ? " implicit=\"yes\"" : "");
            result = write_attributes(prep, buf, p, m, *dir_pos);
            if (result != MUSICXML_EXPORT_OK) {
                break;
            }
            write_measure_body(prep, buf, p, m, &prepared->cells[p * prep->measure_count + m], dir_pos);
            musicxml_score_xmlbuf_append(buf, "    </measure>\n");
        }
        musicxml_score_xmlbuf_append(buf, "  </part>\n");
    }
    musicxml_score_xmlbuf_append(buf, "</score-partwise>\n");
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
    musicxml_score_prep_free(&prepared->prep);
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

/* The event of one segment, as musicxml_score_write_segment writes it. */
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
        musicxml_score_format_note_xml_id(entity_id_value(note_event_id(event)), base, sizeof(base), true);
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
        out->step = musicxml_score_PITCH_STEP_NAMES[pitch_spelling_letter(spelling)][0];
        out->alter = pitch_spelling_accidental(spelling);
        out->octave = pitch_spelling_octave(spelling);
    }
    NotatedValue value;
    if (!segment->measure_rest && musicxml_score_classify_piece(prep, segment->note, segment->end - segment->start, true, &value)) {
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
        return musicxml_score_from_meter_status(status);
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    out->beats = (int)time_signature_top_number(signature);
    out->beat_type = (int)time_signature_bottom_number(signature);
    MusicalTime region_time = meter_map_region_start(region);
    long long region_start = 0;
    if (musicxml_score_time_ticks((long)region_time.value.num, (unsigned long)region_time.value.den, prep->divisions, &region_start) != TIME_OK) {
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
    MusicXmlExportResult result = musicxml_score_prepare(prep);
    SegmentList *lists = NULL;
    size_t *cursor = NULL;
    if (result == MUSICXML_EXPORT_OK) {
        result = musicxml_score_build_segments(prep, &lists);
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
        musicxml_score_segments_free(lists, prep->group_count);
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
    if (!musicxml_score_xmlbuf_init(&buf)) {
        score_layout_destroy(&layout);
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    result = write_document(layout.prepared, &buf);
    if (result == MUSICXML_EXPORT_OK && buf.failed) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }
    score_layout_destroy(&layout);
    if (result != MUSICXML_EXPORT_OK) {
        musicxml_score_xmlbuf_free(&buf);
        return result;
    }
    *out_xml = buf.data;
    return MUSICXML_EXPORT_OK;
}
