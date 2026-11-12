# TextProcessingTests

Tests of the TextProcessing module. Case data is in `Private/Cases/TextCases.h` and `Private/Cases/RegexCases.h`.

| group | covers |
|---|---|
| `Contract` | boundaries, repeats, decimals, split punctuation, tails, Unicode, reset, cap, 1,000 random partitions, wrong thread, invalid settings |
| `Regex` | zero-length matches, invalid patterns, non-ASCII text and literal replacement |
| `Blueprint` | a reflected call and a compiled graph calling every wrapper and regex function |
| `Example` | a streamed reply turned into subtitle lines and a cleaned label |
