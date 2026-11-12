#ifndef URSATZ_COMPOSER_PIECE_PERFORM_H
#define URSATZ_COMPOSER_PIECE_PERFORM_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A composed piece performed in order (R-485, docs/form-performance.md). A form is composed once; the performance is a
 * new plan that holds the composed sections in the order they are played: a repeated group of sections written out twice
 * in place (A A B A1 B A1). The performed plan is what the piece version renders (plan JSON, MIDI, MusicXML); the
 * composed plan and its compose outcome give the hearing.
 *
 * Every performed section is a section node at its performed start that holds the source section's letter, cadences and
 * keys, and its lanes Given (plan_give_section: harmony, bass, melody and inner voices moved, its phrase and idea nodes
 * copied). The key lane over the source section is copied, cut to it and moved. The material, the motive nodes and the
 * seam are not performed. Every node is Realized and the form node Given.
 */

/* The most sections a performance holds. */
#define PIECE_PERFORMED_MAX 32

/* One section of the performance. */
typedef struct {
    size_t part;                                      /* the part it belongs to (0 for a form of one part) */
    NodeId source;                                    /* the section in its part's composed plan */
    NodeId node;                                      /* the section in the performed plan */
    char name[FORM_SPEC_SECTION_NAME_MAX + 12];       /* "A", "A (repeat)"; a part's name and a space come before it */
    Rational start;                                   /* where it starts in the performed plan */
    bool repeat;                                      /* its second time in a repeated group */
} PiecePerformedSection;

/* The composed plan of a part, and what is performed of it. */
typedef struct {
    const FormSpec *form;
    const FormSpecSize *size;
    const PiecePlan *plan;
    bool repeats; /* its repeated groups are written out */
    const char *name; /* the part's name, or NULL */
} PiecePerformPart;

typedef struct {
    PiecePlan *plan;
    PiecePerformedSection sections[PIECE_PERFORMED_MAX];
    size_t section_count;
} PiecePerformance;

/*
 * The performance of the parts in order, into *out (zeroed first). The parts' meter must be one and the same
 * (PIECE_ERR_BAD_REQUEST), and a performance has at most PIECE_PERFORMED_MAX sections (PIECE_ERR_BAD_REQUEST too).
 * A failed call leaves *out empty.
 */
PieceResult piece_perform(const PiecePerformPart *parts, size_t part_count, PiecePerformance *out);

/* Frees the plan; safe on a zeroed or already destroyed performance. */
void piece_performance_destroy(PiecePerformance *performance);

#endif
