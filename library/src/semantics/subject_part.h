#ifndef MUSIC_SEMANTICS_SUBJECT_PART_H
#define MUSIC_SEMANTICS_SUBJECT_PART_H

#include <stdbool.h>

/*
 * SubjectPart (Value): one of the three parts a fugue subject can be
 * thought of in, charting a course from a distinctive opening to a generic
 * cadence. A plain enum: it names a part and carries no EntityID, no
 * pitches, and no framework claim.
 *
 *   KOPFMOTIV     the "head motive": the opening gesture
 *   FORTSPINNUNG  prolongation after the opening, which may include
 *                 sequence and motor rhythm
 *   CADENCE       the generic cadence that closes the subject
 *
 * Where a subject divides into these parts is an analytic judgment; no type
 * here makes it.
 */

typedef enum {
    SUBJECT_PART_KOPFMOTIV = 0,
    SUBJECT_PART_FORTSPINNUNG,
    SUBJECT_PART_CADENCE,
    SUBJECT_PART_COUNT
} SubjectPart;

/* True for the three parts above; false for any other value. */
bool subject_part_valid(SubjectPart part);

/* "kopfmotiv" "fortspinnung" "cadence"; NULL for an invalid part. */
const char *subject_part_name(SubjectPart part);

#endif
