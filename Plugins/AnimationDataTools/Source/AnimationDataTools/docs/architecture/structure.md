# Structure

## Purpose

Turn source channels — named float curves and per-frame bone rotation deltas — into the curves and
bones a target skeleton expects, through data-driven rules that any project can edit without code.

## Owns

- The contract types: clip, rule, limit, profile, mapped clip and the result codes.
- Validation of clips and profiles, and the mapping itself.
- Conversion of a pose asset's curve values into rules, so pose assets and profiles share one
  mapping implementation.
- The profile asset, which stores a profile for reuse.

## Does not own

- Writing sequences: the
  [editor module](../../../AnimationDataToolsEditor/docs/architecture/README.md).
- Pre-processing of raw source data, such as normalising a sensor range: the caller, before mapping.
- Retargeting between skeletons: the engine's retargeting tools.

## Composition

| part | header | responsibility |
|---|---|---|
| contract types | `AnimationDataTypes.h` | clip, curve, bone track, rule, limit, profile, mapped clip, `EAnimationDataResult` |
| mapper | `AnimationDataMapper.h` | `Map` and `MakeRulesFromPoseAsset` |
| profile asset | `AnimationDataProfileAsset.h` | `UAnimationDataProfileAsset`, a data asset holding one profile |
| Blueprint nodes | `AnimationDataBlueprintLibrary.h` | the mapper's two operations for Blueprints |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through `UPoseAsset`'s pose names, curve names and curve values: pose conversion reads only those,
on the game thread, and produces ordinary rules.

## Invariants

- `Map` is pure: it reads no `UObject` state and runs on any thread. `MakeRulesFromPoseAsset` reads a
  `UObject` and returns `WrongThread` off the game thread.
- Malformed input is refused with `InvalidInput` and a reason, and nothing is produced: a
  non-positive or non-finite frame rate, no frames, curves or tracks of different lengths,
  non-finite values, rotations that are not unit quaternions, duplicate or empty names (also after
  bone renaming), unnamed rules, or a limit whose `Min` exceeds its `Max`.
- Every rule adds `Source × Multiplier + Offset` to its target at every frame. Contributions are
  summed in rule order, so the same profile and clip always give bit-identical results.
- A contribution or accumulated target value that is not finite is refused with `InvalidInput` and
  target/frame context; no non-finite clip is published.
- Every target curve has a value at every frame, zero included.
- Limits clamp a target after all of its contributions are summed. Nothing else clamps.
- Name filtering applies to mapped names only: rules decide which targets exist; sources no rule
  reads are dropped unless `bPassThroughUnmapped` copies them under their own names; rule sources
  missing from the clip are listed in `MissingSources`.
- Bone tracks keep their deltas and take a target name from `BoneRenames`, or keep their own.
- No narrow index types: any number of channels a `TArray` can hold maps.
- Pose conversion makes one rule per pose and non-zero curve value — the pose name is the source,
  the curve the target, the value the multiplier — and ignores bone poses.
