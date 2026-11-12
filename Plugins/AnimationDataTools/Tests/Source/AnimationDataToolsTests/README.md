# AnimationDataToolsTests

Tests of the AnimationDataTools runtime module. Case helpers are in `Private/Cases/MappingCases.h`;
the skeleton is the engine's `SkeletalCube_Skeleton`.

| group | covers |
|---|---|
| `Mapper` | renamed targets, zero keys, accumulation by profile and by pose asset, 300 channels, limits after the sum, malformed input, deterministic order, one source to many targets, pass-through, bone renames, thread rules |
| `Blueprint` | a reflected call and a compiled graph calling every node |
| `Example` | a normalised sensor reading mapped through a profile asset |
