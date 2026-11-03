#ifndef URSATZ_COMPOSER_HARMONY_SCHEMA_H
#define URSATZ_COMPOSER_HARMONY_SCHEMA_H

#include <stdbool.h>

#include <galant-schemas/galant_prototypes.h>

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/harmony_bass.h>

/*
 * The schema operator: a Fill operator that offers a galant schema as the skeleton of the start of a
 * harmony target, writing the harmony and the bass together. The schemas are the prototypes of
 * Ursatz's galant-schemas framework; nothing of them is copied here. For each stage the numeral gives
 * the chord, the bass degree (with the alterations the framework accepts) gives its inversion, and
 * the figures say whether it has a seventh. A stage whose data does not make one consistent chord
 * (Indugio, Quiescenza and Cadenza doppia in the framework as it is) makes the schema unavailable, as
 * do the modulating schemas, since modulation is not planned here.
 *
 * A target is a Constrained harmony lane span, not marked for embellishment, with a key and a meter
 * over it, and the written chords of its owner before it. The bass lane over it is empty or one
 * Constrained placeholder the size of the target. The meter must have two chord positions in a bar
 * (4/4 and 6/8): a stage's strength has to be its cell's, and 2/4 and 3/4 have only strong cells.
 *
 * One stage takes one cell, in order from the start of the target, the last one running to the end of
 * its bar. A schema whose first stage is weak starts on a medium cell; at a bar line it is offered
 * only if its first chord is a tonic, which is then held through the strong cell before it.
 *
 * The roles decide where a schema may go:
 *   opening       an opening gambit, when the target starts its owner node
 *   continuation  a continuation or pre-cadential schema, when it does not
 *   cadence       a cadence schema, when it fills exactly the rest of the target and the target
 *                 (or its owner) asks for a cadence; the cadence heard must be the one asked for
 * What is left of the target after the schema stays a Constrained span with its cadence, and must be
 * long enough to bring it; the loop fills it with whatever operators are registered. The chords of the
 * schema each carry a schema constraint (the schema and the stage); the schema's melody is for the
 * melody operators to read from the framework. The candidate's label is "schema:<name>", so the
 * record of the run says which schema was chosen.
 *
 * The chords are judged by standard-harmony as part of their phrase, the bass by standard-
 * counterpoint's melodic rules. A target no schema fits has no candidates, so the loop leaves it open
 * as FILL_STUCK_ALL_REJECTED (or, for a meter it cannot use, with no operator that applies).
 */

#define HARMONY_SCHEMA_OPERATOR_NAME "harmony-schema"

/* A bit of HarmonySchemaParams.schemas: the schema with this GalantSchemaId. */
#define HARMONY_SCHEMA_BIT(id) (1u << (unsigned)(id))
#define HARMONY_SCHEMA_ALL ((1u << GALANT_SCHEMA_COUNT) - 1u)

typedef enum {
    HARMONY_SCHEMA_ROLE_OPENING = 1u << 0,
    HARMONY_SCHEMA_ROLE_CONTINUATION = 1u << 1,
    HARMONY_SCHEMA_ROLE_CADENCE = 1u << 2,
    HARMONY_SCHEMA_ROLE_ALL = (1u << 3) - 1
} HarmonySchemaRole;

/*
 * schemas is the set of schemas offered (bits of HARMONY_SCHEMA_BIT); roles the set of roles (bits of
 * HarmonySchemaRole); preference is added to the score of a schema's candidate, in thousandths, so
 * that 1000 puts it a whole point ahead of the plain operators' candidates, which score between 0 and
 * 1, and 0 leaves it to the seeded stream; bass is the bass range and subdivision of the bass it writes.
 */
typedef struct {
    unsigned schemas;
    unsigned roles;
    unsigned preference;
    HarmonyBassParams bass;
} HarmonySchemaParams;

/* The id of the schema with this name, as the framework spells it ("romanesca", "do-re-mi"). False if there is none. */
bool harmony_schema_find(const char *name, GalantSchemaId *id);

/*
 * Whether every stage of the schema makes one consistent chord in a key of this mode, so that it can be offered
 * at all (see the top of this header). It does not say whether the meter and the span can hold it.
 */
bool harmony_schema_available(GalantSchemaId id, PlanKeyMode mode);

/* The defaults: every schema, every role, preference 250, and the bass operator's defaults. */
HarmonySchemaParams harmony_schema_params_default(void);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params
 * must outlive it (NULL means the defaults).
 */
FillResult harmony_schema_register(FillOperatorRegistry *registry, const HarmonySchemaParams *params);

#endif
