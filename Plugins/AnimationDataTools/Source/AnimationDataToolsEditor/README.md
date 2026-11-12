# AnimationDataToolsEditor module

Editor module that writes mapped clips as animation sequences, in the transient package or as saved
assets.

| concern | owner |
|---|---|
| contract — timing, validation, curve metadata, saving — and architecture | [docs/README.md](docs/README.md) |

**Entry points:** `FAnimationDataSequenceWriter` (`Public/AnimationDataSequenceWriter.h`);
editor-utility Blueprint nodes in `Public/AnimationDataEditorLibrary.h`. A runnable example is the
`AnimationDataToolsEditor.Example.JsonClipToSequence` test in
[the module's tests](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/AnimationDataTools/Tests/Source/AnimationDataToolsEditorTests/Private/AnimationDataToolsEditorTests.cpp).
