# Structure

## Purpose

Write a mapped clip as an animation sequence the editor's tools can preview, edit, save and
compress.

## Owns

- Converting a mapped clip into keys: timing, the final hold, bone composition and interpolation.
- The treatment of curves the target skeleton has no metadata for.
- Validation before anything is created, and creating and saving the asset.

## Does not own

- Mapping and the mapped clip's shape: the
  [runtime module](../../../AnimationDataTools/docs/architecture/README.md).
- Animation storage, compression and evaluation: the engine's animation data model and sequence.

## Composition

| part | header | responsibility |
|---|---|---|
| sequence writer | `AnimationDataSequenceWriter.h` | `FAnimationDataSequenceWriter`, build options and build report |
| editor-utility nodes | `AnimationDataEditorLibrary.h` | the writer's two operations for Blueprints |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through `IAnimationDataController`: all keys are written in one controller bracket, without
transactions, so the engine's data model stays the only store of the sequence's data.

Through `UPackage::SavePackage` and the asset registry: a saved sequence is a normal asset.

## Invariants

- Game thread only; other threads get `WrongThread`.
- Everything is validated before any object exists: a missing skeleton, a non-positive or
  non-finite frame rate, a frame rate that cannot be represented as an `FFrameRate` without a zero
  or overflowing numerator (below 0.001 fps or beyond the int32 numerator limit), a frame rate
  whose interval is near the animation data controller's zero tolerance (about 10,000 fps),
  no frames, tracks or curves of the wrong length, non-finite values, rotations that are not unit
  quaternions, duplicate names, and bones the skeleton lacks return `InvalidInput` with a reason
  and create nothing.
- A clip of N frames at F frames per second becomes a sequence of length N / F with N + 1 keys: key i
  holds frame i at time i / F, and key N repeats frame N − 1, so every time in [(N − 1) / F, N / F]
  evaluates to the last frame and a one-frame clip holds over [0, 1 / F].
- A bone key's rotation is `Reference × Delta` in the bone's local space, with the reference
  translation and scale; bones without a track keep the reference pose.
- Interpolation is linear or step, for bones and curves alike.
- Curves the skeleton has no metadata for are listed in the build report and written, unless
  `bDropCurvesMissingFromSkeleton` leaves them out.
- `WriteAsset` refuses an invalid package name or an existing asset, and never overwrites. On a
  save failure it returns `SaveFailed` and marks the unsaved sequence and package for garbage
  collection. It does not promise filesystem rollback: the engine's save path may leave partial
  files, so a caller that needs a clean disk target must verify or remove them.
