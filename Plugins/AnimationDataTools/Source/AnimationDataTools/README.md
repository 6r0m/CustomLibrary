# AnimationDataTools module

Runtime module holding the clip, profile and mapped-clip types, the mapper, pose-asset conversion
and the profile asset.

| concern | owner |
|---|---|
| contract — validation, mapping order, limits, thread rules — and architecture | [docs/README.md](docs/README.md) |

**Entry points:** `FAnimationDataMapper` (`Public/AnimationDataMapper.h`), the types in
`Public/AnimationDataTypes.h`, `UAnimationDataProfileAsset`, and Blueprint nodes in
`Public/AnimationDataBlueprintLibrary.h`. A runnable example is the
`AnimationDataTools.Example.NormalizedSensorToCurves` test in
[the module's tests](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/AnimationDataTools/Tests/Source/AnimationDataToolsTests/Private/AnimationDataToolsTests.cpp).
