# The fill loop

The fill loop turns a plan with open material into a plan with none. There is one loop. It does
not matter whether the plan fills top-down, bottom-up or from the middle: that comes only from
where the Given material sits and which operators are registered.

Headers: `<ursatz_composer/fill_loop.h>`, `fill_operator.h`, `fill_target.h`, `fill_rng.h`,
`fill_record.h`, `fill_suggest.h`. This branch ships the engine only. Real music operators are
added later and plug in as described below.

## Targets

A target is one node or one lane span (`FillTarget`). A target is **open** when it is Constrained.
Given material is never changed. Realized material is not touched by the loop; regenerating it is a
separate, explicit step.

`fill_pick_open_target` returns the open target with the most constraints. Ties go to the target
that starts earlier, then to a node over a lane span, then to the lower id. The same plan always
gives the same pick.

## One iteration

1. **Pick** the open target with the most constraints, skipping targets set aside as unfillable.
2. **Generate.** Every operator that fits the target and says it applies proposes candidates,
   in registration order.
3. **Check.** The checker looks at each candidate. A hard violation rejects it; a score ranks the rest.
4. **Choose** the best score. Candidates tied for the best are settled by the seeded random stream.
5. **Apply** the candidate, mark the target Realized if the operator did not, and **propagate**.

Then repeat until nothing is open.

### When a target cannot be filled

- No operator applies: the target is set aside as `FILL_STUCK_NO_OPERATOR`.
- Operators apply, but every candidate is rejected (or none was proposed): `FILL_STUCK_ALL_REJECTED`.

A target set aside is left open and the loop moves on. After the next successful step the set-aside
targets are tried again, since a step can add constraints that make an operator applicable. The loop
always ends, with one of:

| `FillStatus` | Meaning |
| --- | --- |
| `FILL_STATUS_COMPLETE` | Nothing is open. |
| `FILL_STATUS_STUCK` | Open targets remain and none can be filled. `outcome.stuck` lists them and why. |
| `FILL_STATUS_MAX_ITERATIONS` | The iteration limit was reached (`FillConfig.max_iterations`, default 10000, counting failed attempts too). |
| `FILL_STATUS_ERROR` | `fill_run` returned an error: an operator's `generate` or `apply` failed, the checker failed, or allocation failed. The plan may hold the steps before the error. |

## Operators

An operator is a `FillOperator`. It wraps Ursatz's `Generator`: `generator.name` and
`generator.version` are required (and the name must be unique in the registry); `deterministic` and the
descriptions are carried for the record. `Generator.generate` is not used, since it takes a
`GenerationPlan` and returns material ids, with no way to name a node or say what to change.

| Kind | Target | Does |
| --- | --- | --- |
| `FILL_OPERATOR_REFINE` | node | Splits a node into children. |
| `FILL_OPERATOR_GROW` | node | Builds a node from its children. It fills a node that already exists; the plan has no way yet to insert a node above existing ones. |
| `FILL_OPERATOR_FILL` | lane span | Fills a lane span. |

The loop never offers an operator a target of the wrong kind.

| Hook | Required | Purpose |
| --- | --- | --- |
| `applies(plan, target, context)` | yes | Can the operator propose anything for this target? |
| `generate(plan, target, rng, context, out)` | yes | Proposes candidates with `fill_candidates_add`. Must not change the plan. Any randomness comes from `rng`. Returns false on failure. |
| `apply(plan, target, candidate, context)` | yes | Applies one candidate through the public plan API. Returns `PLAN_OK` on success. |
| `propagate(plan, target, candidate, context)` | no | Adds constraints to the target's relatives after `apply`. |

A candidate is a short label (for the record), an opaque payload, and an optional destroy function.
Only the operator that made a candidate reads its payload; a checker may too, if it knows the operator.

`fill_relatives` lists the relatives to propagate to: for a node, its parent, children and the
siblings before and after; for a lane span, its owner node and the spans before and after it in the
same lane and voice.

Operators are registered with `fill_registry_register` and tried in registration order. That order
is part of what makes a run reproducible.

## The checker

`FillCheckerFn` is called once per candidate:

```c
bool checker(const PiecePlan *plan, FillTarget target, const FillOperator *op,
             const FillCandidate *candidate, void *context, FillCheck *out);
```

It fills `FillCheck`: `hard_violation` rejects the candidate; the score is a non-negative rational
(`score_numerator / score_denominator`, as in Ursatz's `Candidate`), higher is better; `reason` is
short text for the record. It returns false only when it fails itself, not for a rejected candidate.
The default, `fill_default_checker`, accepts everything at 0/1. Rule checking against a framework
plugs in here; none ships on this branch.

## Seeds and records

One `FillRng` (SplitMix64) is seeded from `FillConfig.seed` and serves the whole run: the loop's
tie-breaks and any randomness in operators. Operators must not use another source. With the same
plan, operators, checker and seed, the plan and the record come out the same.

`fill_run` can return a `FillRecord`. It wraps Ursatz's `GenerationRecord` (seed, Composer version)
and adds a step per attempt: the target, the outcome (`applied`, `no_operator` or `all_rejected`),
every candidate considered with its operator, score, rejection and reason, the chosen index, and the
random state before and after. `fill_record_to_json` writes it as JSON, with the seed and states as
decimal strings, to be kept next to the plan. The plan's own format does not change.

## Suggestion mode

`fill_suggest` returns the top N candidates for one open target instead of choosing, and leaves the
plan unchanged. They come back as an Ursatz `CandidateSet`, best first: each `Candidate` names its
material by id and carries its score (it has no Interpretations, since there is no framework yet).
`fill_suggestion_label` and `fill_suggestion_operator` say what each one is. Rejected candidates are
not suggested. `fill_apply_suggestion` applies one, by its index in the set, the way the loop would.

## A minimal operator

A Fill operator that writes the same pitch into every melody span:

```c
static bool applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN &&
           plan_lane_span_lane(plan, (LaneSpanId)target.id) == PLAN_LANE_MELODY;
}

static bool generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                     FillCandidateList *out) {
    (void)plan; (void)target; (void)rng; (void)context;
    return fill_candidates_add(out, "middle C", NULL, NULL);
}

static PlanResult apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate,
                        void *context) {
    (void)candidate; (void)context;
    LaneSpanId id = (LaneSpanId)target.id;
    PlanNote note = {60, {0, 1}, plan_lane_span_span(plan, id).length};
    PlanLanePayload payload = {0};
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    return plan_lane_span_write(plan, id, &payload); /* also marks the span Realized */
}

FillOperator middle_c = {
    .generator = {.name = "middle-c", .version = "1", .deterministic = true},
    .kind = FILL_OPERATOR_FILL,
    .applies = applies,
    .generate = generate,
    .apply = apply,
};

FillOperatorRegistry *registry = fill_registry_create();
fill_registry_register(registry, &middle_c);

FillConfig config = {.seed = 42};
FillOutcome outcome;
FillRecord *record = NULL;
fill_run(plan, registry, &config, &outcome, &record);
```

After the run, `outcome.status` says how it ended, `record` holds what happened and why, and
`fill_outcome_destroy`, `fill_record_destroy` and `fill_registry_destroy` release them.
