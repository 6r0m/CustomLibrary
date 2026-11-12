# Structure

## Purpose

Give C++ and Blueprints a few validated engine conveniences, each with a narrow contract so that a
bad input returns a failure value instead of crashing, hanging or biasing a result.

## Owns

- Input validation and failure reporting for each utility.
- The weighted-selection algorithm.

## Does not own

- The underlying engine features — random streams, scene views, body instances, project settings:
  Unreal Engine.
- Text fragmentation and regex replacement: TextProcessing.
- Anything the engine's Blueprint library already offers; a utility exists only where it adds a
  guarantee to an engine call or fills a missing node.

## Composition

| library | functions | responsibility |
|---|---|---|
| `URandomUtilitiesLibrary` | `TrySelectWeightedIndex`, `TrySelectWeightedIndexFromStream` | an index with probability proportional to its weight |
| `USceneUtilitiesLibrary` | `IsActorCollisionSphereInViewFrustum`, `TryGetBodyLockedAxis` | validated scene and body queries |
| `UProjectUtilitiesLibrary` | `GetProjectVersionString` | the project version from the general project settings |

The implementation is split into private files per concern; each library is a separate Blueprint
entry point.

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through `FBodyInstance::GetLockedAxis`: the physics helper validates the component and its body,
then returns the engine's answer unchanged.

## Invariants

- Every utility validates its inputs and reports failure through its return value or a defined
  failure output; none asserts or dereferences a missing object.
- Weighted selection refuses an empty array, any negative weight, all-zero weights and a total above
  `MAX_int32`, setting `OutIndex` to `INDEX_NONE`. Otherwise each index owns exactly its weight's
  share of a uniform draw over `[0, Total − 1]`, so a zero weight is never picked and equal weights
  are equally likely.
- The frustum test returns false and logs the reason when there is no actor, no world, no local
  player, no viewport or no view, and when called off the game thread. It tests a collision sphere
  and ignores occlusion and rendered bounds.
- The locked-axis query returns false and a zero vector when the component or its body is missing.
