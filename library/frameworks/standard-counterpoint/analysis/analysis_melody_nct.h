#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_NCT_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_NCT_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_melody.h"

/*
 * Non-chord tones in the melody (the melody adapter, step 3). The adapter does NOT label non-chord tones: that is the
 * chord identification's work (common-practice-minimal, L2.5), and re-deriving it here would give a second, competing claim.
 * It ATTACHES the existing result to each melody note by reference.
 *
 * Each melody note carries the result the caller got from L2.5 (analysis_melody.h): nct = CHORD_TONE, NON_CHORD_TONE (with
 * nct_type, a NonChordTone) or UNCLASSIFIED, and nct_ref, an opaque string naming the source claim (up to
 * STANDARD_COUNTERPOINT_NCT_REF_SIZE - 1 characters; the caller's convention, for example the id of the segment the label
 * was computed in). UNCLASSIFIED is also the status of a note L2.5 did not cover (a melody note shorter than a segment, or
 * in a segment with no chord): the adapter never guesses a label for it.
 *
 * One claim per note (a Structure shell of the note id and an Interpretation targeting it): FrameworkReference
 * standard-counterpoint, claim = a StandardCounterpointNctClaim holding the status, the type, and the reference copied
 * as given. uncertainty = 1 - the note's confidence (the labelling itself is the source's; it is not re-weighed).
 *
 * Per span (as in analysis_melody.h; the whole line by default), a count claim: the notes, the chord tones, the unclassified
 * notes and the non-chord tones by NonChordTone type.
 */

#define STANDARD_COUNTERPOINT_NCT_CLAIM_TYPE "standard-counterpoint/melodic-nct"
#define STANDARD_COUNTERPOINT_NCT_COUNTS_CLAIM_TYPE "standard-counterpoint/nct-counts"
#define STANDARD_COUNTERPOINT_NCT_REF_SIZE 128

typedef struct {
    int index; /* the note's entry in the line */
    int status; /* StandardCounterpointNctStatus */
    int type;   /* NonChordTone when status is NON_CHORD_TONE, else -1 */
    int has_ref;
    char ref[STANDARD_COUNTERPOINT_NCT_REF_SIZE];
} StandardCounterpointNctClaim;

typedef struct {
    StandardCounterpointNctClaim claim;
    double uncertainty;
    Structure *shell;               /* owned: the note id */
    Interpretation *interpretation; /* owned */
} StandardCounterpointNctLabel;

typedef struct {
    StandardCounterpointNctLabel *items; /* owned; one per note in line order; NULL when none */
    size_t count;
} StandardCounterpointNctLabels;

StandardCounterpointMelodyStatus standard_counterpoint_melody_nct_labels(const StandardCounterpointMelodyLine *line,
                                                                         StandardCounterpointNctLabels *out);
void standard_counterpoint_nct_labels_destroy(StandardCounterpointNctLabels *labels);
bool standard_counterpoint_nct_claim_read(const Interpretation *interpretation, StandardCounterpointNctClaim *out);

typedef struct {
    int span_first;
    int span_last;
    int note_count;
    int chord_tones;
    int unclassified;
    int non_chord_tones;                       /* all labeled non-chord tones */
    int by_type[NON_CHORD_TONE_COUNT];         /* indexed by NonChordTone */
} StandardCounterpointNctCountsClaim;

typedef struct {
    char span_id[64];
    StandardCounterpointNctCountsClaim claim;
    double uncertainty; /* 1 - the least note confidence in the span */
    Structure *shell;
    Interpretation *interpretation;
} StandardCounterpointNctCounts;

typedef struct {
    StandardCounterpointNctCounts *items; /* one per span that holds a note; NULL when none */
    size_t count;
} StandardCounterpointNctCountList;

StandardCounterpointMelodyStatus standard_counterpoint_melody_nct_counts(const StandardCounterpointMelodyLine *line,
                                                                         const StandardCounterpointMelodySpan *spans,
                                                                         size_t span_count,
                                                                         StandardCounterpointNctCountList *out);
void standard_counterpoint_nct_count_list_destroy(StandardCounterpointNctCountList *list);
bool standard_counterpoint_nct_counts_claim_read(const Interpretation *interpretation,
                                                 StandardCounterpointNctCountsClaim *out);

#endif
