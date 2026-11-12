# AnimationDataToolsEditorTests

Tests of the AnimationDataToolsEditor module, writing to the engine's `SkeletalCube_Skeleton`.

| group | covers |
|---|---|
| `Writer` | zero keys, length and key count, bone composition, curves missing from the skeleton, one- and two-sample timing, refusal without creating anything, save and reload, wrong thread |
| `Blueprint` | a reflected call and a compiled graph calling every node |
| `Example` | a JSON clip mapped and written as a sequence |
