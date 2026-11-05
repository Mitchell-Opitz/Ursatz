# standard-meter metric position

The metric position rules of `frameworks/standard-meter/`. [Docs index](../README.md)

Framework overview: [frameworks/standard-meter/README.md](../../frameworks/standard-meter/README.md).

- **Metric position** (`metric_position_rules.h`): reads a `MetricalPosition`
  (`time/metrical_position.h`: a measure number and an exact offset in whole
  notes, no beat claim) together with a `TimeSignature`, using the meter
  class's beat unit (the bottom unit in simple meter, a dotted unit in
  compound).
  - **Beat and subdivision**: `standard_meter_beat_position` gives the beat
    (1-based) and where the offset falls inside it, an exact fraction of the
    beat, 0 on the beat: 3/8 into 4/4 is beat 2 at 1/2, and 3/8 into 6/8 is
    beat 2 at 0.
  - **Metric level**: `standard_meter_metric_level` names the position's level,
    strongest to weakest: downbeat (beat 1); secondary beat (beat 3 of a
    quadruple meter, 4/4 or 12/8); beat (any other beat); off-beat (on the
    beat's division: the "and" at half a beat in simple meter, "la" and "li" at
    a third and two thirds in compound); finer (anything between those).
  - **BeatStrength**: `standard_meter_beat_strength` maps those levels onto the
    shared `BeatStrength` (`semantics/beat_strength.h`), which has three values
    and gets none added: the downbeat is STRONG, the secondary beat MEDIUM, and
    every other level WEAK. This is the convention of standard-counterpoint
    (four against one is strong, weak, medium, weak), so 4/4 reads S W M W and
    3/4 reads S W W. `BeatStrength` alone cannot tell a beat from an off-beat;
    the metric level and the beat position keep that distinction.
  - **Irregular meters** (5/8, 7/8): this framework defines no grouping for
    them, so every bottom-unit pulse is a beat, the downbeat is STRONG, and no
    beat is secondary.
  - **Pickups**: a pickup is read at its offset from where a full measure
    would have started (a quarter pickup in 4/4 is beat 4). The signature to
    pass is `metrical_position_reading_signature` in the kernel, which for a
    short leading signature (1/8 before 12/8) is the following region's.
  - Positions must be inside the measure; a function is false for an offset at
    or past its end, and for arithmetic that does not fit (checked, never
    wrapped).
