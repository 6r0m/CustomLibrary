# Structure

## Purpose

Emit each delimiter-terminated fragment of a text stream as soon as it is known to be complete, so
a consumer (subtitles, speech, logging) can act on it before the rest of the text arrives, and
provide validated regex replacement for that text.

## Owns

- Fragment boundaries, ordering and exactly-once emission.
- The cap on text held while waiting for a delimiter.
- The fragmenter's thread ownership.
- Regex replacement and the literal-replacement rule.

## Does not own

- Linguistic sentence segmentation (abbreviations, quotes, languages without delimiters): not
  provided; boundaries are purely character rules.
- Trimming or rewriting fragments: the consumer; fragments are exact slices of the input.
- The ICU regex engine: Unreal Engine; this module validates and applies it.

## Composition

| part | header | responsibility |
|---|---|---|
| fragmenter | `TextProcessingFragmenter.h` | `FTextProcessingFragmenter`, `FTextProcessingSettings`, `ETextProcessingResult` |
| Blueprint wrapper | `TextProcessingFragmenterObject.h` | `UTextProcessingFragmenterObject`, a thin object owning one fragmenter |
| regex library | `TextProcessingLibrary.h` | `UTextProcessingLibrary::ReplaceRegexMatches` |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through `UTextProcessingFragmenterObject`: Blueprint calls reach the same fragmenter and get the
same results; the wrapper adds no behaviour.

Through `FRegexMatcher::ReplaceAll`: the regex helper escapes `\` and `$` in the replacement so it
is inserted literally, and checks that the pattern compiled before replacing.

## Invariants

- Fragments are exact slices of the input: the emitted fragments followed by the flushed tail equal
  the input, for any way the input was split into deltas, and the fragments themselves do not
  depend on that split.
- Every character of `Delimiters` ends a fragment. A run of consecutive delimiters ends one fragment.
  Whitespace after a delimiter starts the next fragment.
- A single delimiter with a digit on both sides (`3.14`, and `1,000` when `,` is a delimiter) does
  not end a fragment.
- A delimiter run at the end of the pending text waits for the next delta or `Flush`, because the
  next character decides whether it is a boundary. This is what makes the result independent of
  the split.
- Each fragment is emitted once, in input order; an empty delta emits nothing, and no fragment is
  empty.
- `Flush` returns the remaining text (possibly empty) and ends the stream; the next `Append` starts
  a new one. `Reset` discards pending text without emitting it.
- Pending text never exceeds `MaxPendingChars` UTF-16 code units. `Append` preflights the
  prospective tail on the pending and incoming views before growing storage; an oversized tail
  returns `CapExceeded` with the pending text, scan position and stream unchanged. A batch whose
  raw size exceeds the cap is admitted when its completed fragments leave a tail within the cap;
  after extraction only tail-sized storage is retained.
- Delimiters are single UTF-16 code units outside the surrogate range; settings with no delimiters,
  a surrogate delimiter or a non-positive cap make every call return `InvalidSettings`.
- The fragmenter belongs to the thread that created it; calls from any other thread return
  `WrongThread` and change nothing.
- The regex replace terminates on zero-length matches, inserts the replacement literally, and
  returns false with `OutText` equal to `Input` when the pattern does not compile.
