#ifndef URSATZ_COMPOSER_PIECE_PLAN_KEYS_H
#define URSATZ_COMPOSER_PIECE_PLAN_KEYS_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The keys of a piece plan (R-069). A section is in the home key, or in a closely related key
 * (standard-harmony's harmony_related_key_at and harmony_key_is_closely_related) reached by a pivot chord. The key lane
 * is Given, owned by the form; a section that changes key halfway has two spans, split at its subphrase boundary (4 of
 * 8 bars, 2 of 4), and spans that follow each other in one key are one span.
 *
 * Which sections leave home and where they go is the form's: its regions (the degrees of the home key a key is drawn
 * from, in a major and in a minor home key) and each section's keys (form_spec.h, docs/forms.md). The built-in forms:
 *
 *   binary          A ends in V (major) or III (minor), B starts there and returns home; or both stay home.
 *   rounded binary  the same for A and B; A' is home.
 *   ternary         A home; B starts home and ends in IV, vi or V (major) or III, iv or VI (minor); A home again.
 *   variations      all home, except with three variations or more the middle one, wholly in the relative key
 *                   (vi in major, III in minor). The parallel mode is not closely related and has no pivot.
 *   loop, layered   home.
 *
 * A section that starts in another key than the one before it, or ends in another key than it started in, is
 * where a new key opens: the target of its chords that starts there asks for a pivot chord (PLAN_HARMONY_ANCHOR_PIVOT).
 * Only a section that begins with an antecedent can open a key, so a key never opens in a presentation.
 */

/*
 * The keys a section starts and ends in, and for a key that is not home the K6 relation entry that reached it from the home key
 * (R-597: `omt.relation.dominant`; a static string), NULL for home. The key lane writes it beside the key.
 */
typedef struct {
    SpelledKey start;
    SpelledKey end;
    const char *start_relation;
    const char *end_relation;
    PlanKeyAnchor anchor;      /* what the relations are taken from: home for a drawn region, else a request's region (R-610) */
    const char *anchor_section; /* the anchor section's name for PLAN_KEY_ANCHOR_SECTION, else NULL */
} PieceSectionKeys;

typedef struct {
    size_t count;
    PieceSectionKeys sections[FORM_SPEC_MAX_SECTIONS];
    bool modulates; /* some section is in a key other than home */
} PieceKeyPlan;

/*
 * The key of a region of the form for a home key (R-485): one draw of rng picks one of the region's degrees for the home
 * key's mode, and the degree names a relation entry of the snapshot (1 parallel, 2 supertonic, 3 mediant, 4 subdominant, 5 dominant,
 * 6 submediant, 7 subtonic) that region_resolve resolves from the home key's region (R-596). Degree 1 is the home key's parallel
 * key (the other mode, the same tonic); any other degree is the major or minor key the triad on that degree makes, which for the
 * degrees a form lists is the closely related key of standard-harmony on it (harmony_related_key_of). False when the relation
 * refuses the degree (a diminished triad names no key) or the snapshot lacks the entry; one draw of rng is made in any case.
 */
bool piece_region_key(const KnowledgeSnapshot *knowledge, SpelledKey home, const FormRegion *region, FillRng *rng, SpelledKey *out);

/*
 * Plans the keys of the plan made by form_build_plan from the form's size before its form is Given: adds the Given key lane spans to the
 * form, over the sections and the material, and sets the KEY constraint of a section that ends in another key to the
 * key it ends in. The home key is the KEY constraint of the first section. rng NULL plans no modulation (one span, the
 * home key); otherwise every choice is drawn from rng. knowledge resolves the regions and the parallel mode (piece_region_key); it may
 * be NULL for a form with no section in the parallel mode that stays home. *out is the plan made. regions (NULL for none) is one
 * entry for each section of the size, the keys a request's overlays ask for (R-610): after the draws, a section with a region
 * starts and ends in it, so the draws and every other section's key are as they were.
 */
PieceResult piece_plan_keys(PiecePlan *plan, const KnowledgeSnapshot *knowledge, const FormSpec *form, const FormSpecSize *size,
                            FillRng *rng, const PieceRegion *regions, PieceKeyPlan *out);

typedef struct {
    bool ok;
    size_t changes;     /* the key changes in the key lane (a span in another key than the one before it) */
    size_t modulations; /* of them, those that open a key a section ends in and are confirmed as a modulation */
    char reason[96];    /* the first change that is not, when it is not ok */
} PieceKeyCheck;

/*
 * piece_check_keys_in (check/hearing.h) checks the keys of a composed piece against standard-harmony (R-069): every key is closely related to the home
 * key (the key of the first span), and every key change that opens a key a section ends in is a modulation, not a
 * tonicization. The change is classified on its length and what follows the cadence (harmony_key_change_classify:
 * the chords in the new key, the planned cadence in it, whether the key goes on after it), then confirmed by the
 * cadences heard (harmony_key_change_confirm): an authentic cadence must arrive inside the new key's span, as form_hear_in
 * hears it in the key of its chord. The plan's sections must be written. True when the plan could be read; the result
 * is in out.
 */

/*
 * The cadences across the sections (R-069), set on the plan after its keys. In binary, rounded binary, ternary and
 * variations, which close:
 *   - the last section ends in a PAC;
 *   - a section that ends in another key ends in a PAC there (an IAC would do as well);
 *   - a section that ends on the dominant ends in an HC: a binary A that stays home, which would otherwise close in
 *     the home key before the piece does;
 *   - every other section keeps the form's cadence.
 * A form that does not close (loop, layered) ends open on purpose and is left as its document has it. Only the first CADENCE constraint of
 * a section, the one it ends in, is changed.
 */
PieceResult piece_plan_cadences(PiecePlan *plan, const FormSpec *form, const FormSpecSize *size, const PieceKeyPlan *keys);

typedef struct {
    bool ok;
    char reason[96]; /* what is broken, when it is not ok */
} PieceCadenceCheck;

/*
 * Checks the cadences of the plan against standard-harmony's strength order (harmony_cadence_strength, R-010), reading
 * the sections' CADENCE and KEY constraints and the key lane: the last section's cadence is a PAC, so the strongest of
 * the piece; a section that ends in another key ends in a PAC or IAC; and no section before the last closes in the home
 * key with a PAC unless the form marks it closed (its sections' "closed": ternary A, every variation and the theme, rounded binary A).
 * True when the plan could be read; the result is in out. A form that does not close (loop, layered), and a plan that records no form (plan_form_record), have nothing to
 * check: `closing` is read from the record and each section's `closed` from its description.
 */
bool piece_check_cadences(const PiecePlan *plan, PieceCadenceCheck *out);

/*
 * A plan of a format before 5 has no form record: this gives it the record of the built-in form whose plan name is its root node's
 * name, and each section's `closed` from that form's size (the first size with the plan's sections in its proportions). A plan of
 * no built-in form is left as it is. False only when out of memory; nothing is Given yet when the plan reader calls it.
 */
bool piece_form_record_migrate(PiecePlan *plan);

#endif
