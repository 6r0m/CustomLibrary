# TextProcessing module

Runtime module holding the incremental text fragmenter, its Blueprint wrapper and the regex
replacement library.

| concern | owner |
|---|---|
| contract — boundaries, results, caps, thread ownership, regex semantics — and architecture | [docs/README.md](docs/README.md) |

**Entry points:** `FTextProcessingFragmenter` (`Public/TextProcessingFragmenter.h`); Blueprint
fragment access through `UTextProcessingFragmenterObject`
(`Public/TextProcessingFragmenterObject.h`); regex replacement through `UTextProcessingLibrary`
(`Public/TextProcessingLibrary.h`). A runnable example is the
`TextProcessing.Example.StreamedReplyToSubtitles` test in
[the module's tests](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/TextProcessing/Tests/Source/TextProcessingTests/Private/TextProcessingTests.cpp).
