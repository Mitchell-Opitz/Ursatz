#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export/musicxml_export.h"
#include "io/midi_importer.h"
#include "midi_import_split.h"
#include "io/musicxml_importer.h"
#include "core/rational.h"
#include "time/time_rational.h"
#include "time/temporal_anchor.h"
#include "../rational_test_util.h"

/*
 * Round trip: each framework-v1 reference piece is imported from MIDI, written by MusicXmlExport, and read back by
 * the MusicXMLImporter; the note multisets (sounding pitch, onset, duration) are compared. Meter and key are NOT
 * compared: the score is exported with no MeterMap, so the export lays a defaulted 4/4 and what comes back
 * is a 4/4 piece whatever the source was (docs/modules/export.md). Bach 114 is 3/4 in the score, for one.
 *
 * The export is exact: <divisions> is chosen per piece so every onset and duration is a whole number of divisions,
 * so a file that exports comes back EXACTLY equal (asserted) -- Bach 114, Field 5 and Mozart 545 (whose 240 ticks
 * per quarter note the old fixed 1/3072 grid rounded). Clementi 36 and Field 1 are two-channel files, whose notes
 * have no voice, so the two hands overlap in one implicit voice; the export writes the overlaps as chords and extra
 * voice lanes, and they come back exactly too.
 * The importer's own exactness is proved by the hand-written fixtures (test_musicxml_importer.c), which a lossy
 * export cannot hide.
 */

#define UNITS_PER_WHOLE 3072.0

typedef struct {
    long midi;
    Rational onset;
    Rational duration;
} Item;

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0);
    assert(fseek(f, 0, SEEK_SET) == 0);
    unsigned char *buffer = (unsigned char *)malloc((size_t)size);
    assert(buffer != NULL && fread(buffer, 1, (size_t)size, f) == (size_t)size);
    fclose(f);
    *out_size = (size_t)size;
    return buffer;
}

static Item *collect(NoteEvent *const *events, PitchNumericValue *const *values, size_t count) {
    Item *items = (Item *)calloc(count > 0 ? count : 1, sizeof(Item));
    assert(items != NULL);
    for (size_t i = 0; i < count; i++) {
        items[i].midi = pitch_numeric_value_position_numerator(values[i]);
        items[i].onset = test_rational_of_time(temporal_anchor_position(note_event_temporal_anchor(events[i])));
        items[i].duration = test_rational_of_duration(note_event_duration(events[i]));
    }
    return items;
}

static int compare_items(const void *a, const void *b) {
    const Item *x = (const Item *)a;
    const Item *y = (const Item *)b;
    if (x->midi != y->midi) {
        return x->midi < y->midi ? -1 : 1;
    }
    int by_onset = rational_cmp(x->onset, y->onset);
    return by_onset != 0 ? by_onset : rational_cmp(x->duration, y->duration);
}

static double as_double(Rational r) {
    return (double)r.num / (double)r.den;
}

/* exact: the file's tick divides the export's unit evenly (asserted equal). Otherwise: asserted within 16 units. */
static void roundtrip(const char *file, bool exact) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    NoteEvent **events = NULL;
    PitchNumericValue **values = NULL;
    size_t count = 0;
    assert(test_midi_import_parts(data, size, &events, &values, &count, NULL, NULL) == MIDI_IMPORT_OK);
    free(data);
    assert(count > 0);

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = events;
    score.pitch_values = values;
    score.count = count;
    char *xml = NULL;
    MusicXmlExportResult exported = musicxml_export_write_score(&score, &xml);
    assert(exported == MUSICXML_EXPORT_OK);

    {
        MusicXmlImport back;
        MusicXmlImportStatus status = musicxml_import_buffer(xml, strlen(xml), &back);
        if (status != MUSICXML_IMPORT_OK) {
            printf("%s: import failed: %s\n", file, back.error_message);
        }
        assert(status == MUSICXML_IMPORT_OK);
        assert(back.count == count);
        assert(!meter_map_is_defaulted(back.meter_map)); /* the export writes its defaulted 4/4 as a declared <time> */

        Item *before = collect(events, values, count);
        Item *after = collect(back.events, back.pitch_values, back.count);
        qsort(before, count, sizeof(Item), compare_items);
        qsort(after, count, sizeof(Item), compare_items);

        double worst_onset = 0.0;
        double worst_duration = 0.0;
        size_t within_16 = 0;
        size_t equal = 0;
        for (size_t i = 0; i < count; i++) {
            assert(before[i].midi == after[i].midi); /* the pitch multiset is always equal */
            bool same = rational_cmp(before[i].onset, after[i].onset) == 0 && rational_cmp(before[i].duration, after[i].duration) == 0;
            equal += same ? 1 : 0;
            double onset = fabs(as_double(before[i].onset) - as_double(after[i].onset)) * UNITS_PER_WHOLE;
            double duration = fabs(as_double(before[i].duration) - as_double(after[i].duration)) * UNITS_PER_WHOLE;
            worst_onset = onset > worst_onset ? onset : worst_onset;
            worst_duration = duration > worst_duration ? duration : worst_duration;
            within_16 += (onset <= 16.0 && duration <= 16.0) ? 1 : 0;
            if (exact) {
                assert(same);
            } else {
                assert(onset <= 16.0 && duration <= 16.0);
            }
        }
        printf("%s: %zu notes; pitches equal; %zu exactly equal in onset and duration; %zu within 16/3072 of a whole note; "
               "largest onset shift %.1f, duration shift %.1f (units of 1/3072 whole note); %zu import warning(s)\n",
               file, count, equal, within_16, worst_onset, worst_duration, back.warnings.count);
        free(before);
        free(after);
        musicxml_import_destroy(&back);
    }

    free(xml);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    free(events);
    free(values);
}

int main(void) {
    roundtrip("Bach 114.mid", true);  /* 384 ticks per quarter note */
    roundtrip("Field 5.mid", true);  /* 256 ticks per quarter note */
    roundtrip("Mozart 545.mid", true);  /* 240 ticks per quarter note: exact too */
    roundtrip("Clementi 36.mid", true);  /* two channels: written as chords and lanes */
    roundtrip("Field 1.mid", true);
    printf("test_musicxml_roundtrip: ok\n");
    return 0;
}
