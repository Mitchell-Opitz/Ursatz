#include "pipeline/markings_pipeline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "standard-markings/dynamics_rules.h"
#include "theory/framework.h"
#include "standard-markings/tempo_rules.h"
#include "time/musical_time.h"

/* standard-markings stages no framework.h; the symbol is in the linked library. */
const Framework *standard_markings_framework(void);

static const char *tempo_group_name(TempoGroup group) {
    switch (group) {
        case TEMPO_GROUP_SLOW:
            return "slow";
        case TEMPO_GROUP_MEDIUM:
            return "medium";
        case TEMPO_GROUP_FAST:
            return "fast";
        default:
            return NULL;
    }
}

static const char *tempo_change_name(TempoChange change) {
    switch (change) {
        case TEMPO_CHANGE_GRADUALLY_SLOWER:
            return "gradually-slower";
        case TEMPO_CHANGE_GRADUALLY_FASTER:
            return "gradually-faster";
        case TEMPO_CHANGE_RETURN:
            return "return";
        default:
            return "none";
    }
}

static const char *dynamic_change_name(DynamicChange change) {
    switch (change) {
        case DYNAMIC_CHANGE_LOUDER:
            return "louder";
        case DYNAMIC_CHANGE_QUIETER:
            return "quieter";
        default:
            return "none";
    }
}

/* The group of a tempo text is the library's group of its first word ("allegro assai" is "allegro"). NULL on
   allocation failure; *group is TEMPO_GROUP_UNKNOWN for text with no tempo word. */
static bool text_group(const char *text, TempoGroup *group) {
    *group = TEMPO_GROUP_UNKNOWN;
    while (isspace((unsigned char)*text)) {
        text++;
    }
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length])) {
        length++;
    }
    char *word = malloc(length + 1);
    if (!word) {
        return false;
    }
    memcpy(word, text, length);
    word[length] = '\0';
    *group = standard_markings_tempo_group(word);
    free(word);
    return true;
}

static bool collect_tempo(const ImportedNotes *notes, const char *framework, AnalysisReport *report) {
    const ImportedSource *source = &notes->source;
    if (source->tempo_marking_count == 0) {
        return true;
    }
    report->tempo_markings = calloc(source->tempo_marking_count, sizeof(AnalysisTempoMarking));
    if (!report->tempo_markings) {
        return false;
    }
    const MusicalTime *piece_end = notes->meter.end_time;
    for (size_t i = 0; i < source->tempo_marking_count; i++) {
        const ImportedTempoMarking *in = &source->tempo_markings[i];
        AnalysisTempoMarking *out = &report->tempo_markings[i];
        report->tempo_marking_count = i + 1;
        out->framework = framework;
        out->has_metronome = in->has_metronome;
        out->beat_unit_numerator = in->beat_unit_numerator;
        out->beat_unit_denominator = in->beat_unit_denominator;
        out->bpm_numerator = in->bpm_numerator;
        out->bpm_denominator = in->bpm_denominator;
        out->start_numerator = in->position_numerator;
        out->start_denominator = in->position_denominator;
        if (i + 1 < source->tempo_marking_count) {
            out->end_numerator = source->tempo_markings[i + 1].position_numerator;
            out->end_denominator = source->tempo_markings[i + 1].position_denominator;
        } else {
            out->end_numerator = piece_end ? musical_time_numerator(piece_end) : in->position_numerator;
            out->end_denominator = piece_end ? musical_time_denominator(piece_end) : in->position_denominator;
        }
        out->part = in->part;
        out->group = NULL;
        out->change = "none";
        if (in->text) {
            if (!(out->text = strdup(in->text))) {
                return false;
            }
            TempoGroup group;
            if (!text_group(in->text, &group)) {
                return false;
            }
            out->has_rank = standard_markings_tempo_text_rank(in->text, &out->rank);
            out->group = out->has_rank ? tempo_group_name(group) : NULL;
            out->change = tempo_change_name(standard_markings_tempo_change(in->text));
            out->uncertainty = out->has_rank || standard_markings_tempo_change(in->text) != TEMPO_CHANGE_NONE ? 0.0 : 1.0;
        }
    }
    return true;
}

static bool collect_dynamics(const ImportedSource *source, const char *framework, AnalysisReport *report) {
    if (source->dynamics_count == 0) {
        return true;
    }
    report->dynamics = calloc(source->dynamics_count, sizeof(AnalysisDynamic));
    if (!report->dynamics) {
        return false;
    }
    for (size_t i = 0; i < source->dynamics_count; i++) {
        const ImportedDynamic *in = &source->dynamics[i];
        if (!in->marking) {
            continue;
        }
        AnalysisDynamic *out = &report->dynamics[report->dynamic_count++];
        out->framework = framework;
        if (!(out->marking = strdup(in->marking))) {
            return false;
        }
        out->start_numerator = in->start_numerator;
        out->start_denominator = in->start_denominator;
        out->end_numerator = in->end_numerator;
        out->end_denominator = in->end_denominator;
        out->part = in->part;
        out->staff = in->staff;
        out->has_rank = standard_markings_dynamic_rank(in->marking, &out->rank);
        DynamicChange change = standard_markings_dynamic_change(in->marking);
        out->change = dynamic_change_name(change);
        out->uncertainty = out->has_rank || change != DYNAMIC_CHANGE_NONE ? 0.0 : 1.0;
        /* Against the previous level marking in the same part, not an intervening crescendo word. */
        for (size_t back = report->dynamic_count - 1; out->has_rank && back-- > 0;) {
            const AnalysisDynamic *before = &report->dynamics[back];
            if (before->part != out->part || !before->has_rank) {
                continue;
            }
            out->relation = standard_markings_dynamic_is_quieter(out->marking, before->marking)   ? "quieter"
                            : standard_markings_dynamic_is_quieter(before->marking, out->marking) ? "louder"
                                                                                                  : "same";
            break;
        }
    }
    return true;
}

bool markings_stage_collect(const ImportedNotes *notes, AnalysisReport *report) {
    if (notes->source.format != INPUT_FORMAT_MUSICXML) {
        return true;
    }
    const char *framework = framework_name(standard_markings_framework());
    return collect_tempo(notes, framework, report) && collect_dynamics(&notes->source, framework, report);
}
