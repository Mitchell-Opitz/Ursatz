#ifndef MUSIC_CORE_EQUALITY_H
#define MUSIC_CORE_EQUALITY_H

#include <stdbool.h>

#include "core/entity.h"
#include "core/value.h"

/*
 * Five independently testable equality concepts (System Registry,
 * EQUALITY / CANONICALIZATION; Test Spec §9). They are never collapsed
 * into a single equals() — each answers a different question, and
 * callers must pick the one that matches theirs:
 *
 * - IdentityEquality: entity_identity_equals (core/entity.h). Same
 *   EntityID + SchemaVersion, regardless of instance.
 * - ValueEquality: value_equals (core/value.h). Same content, regardless
 *   of instance.
 * - StructuralEquality: structural_equals below. Equivalent structure
 *   under declared rules, independent of EntityID.
 * - SemanticEquality: contract only at L1 (SemanticEqualsFn). Full
 *   comparison needs Context, an L6 concept — deferred, not implemented
 *   here.
 * - CanonicalEquality: canonical_equals below. Same canonical normal
 *   form under a versioned normalization policy. Canonical-equal does
 *   NOT imply identity-equal (Law 23).
 */

/* StructuralEquality: same type and metadata, independent of EntityID. */
bool structural_equals(const Entity *a, const Entity *b);

/*
 * SemanticEquality contract: two things are semantically equal when they
 * denote the same musical fact despite differing representation (e.g.
 * different Context/Tuning). Comparing that requires Context, which is
 * an L6 concept — at L1 this is only the function-pointer contract a
 * higher layer implements against, not a usable comparison.
 */
typedef bool (*SemanticEqualsFn)(const void *a, const void *b, const void *context);

/*
 * CanonicalEquality: applies a versioned canonicalization function to
 * both values and compares the results by ValueEquality. Two values
 * canonical-equal under policy version V may be canonical-unequal under
 * a different version V' — canonicalizing equal does not mean identical
 * (Law 23). Caller owns the Value returned by canonicalize.
 */
typedef Value *(*CanonicalizeFn)(const Value *value, unsigned long policy_version);

bool canonical_equals(const Value *a, const Value *b, unsigned long policy_version, CanonicalizeFn canonicalize);

#endif