# StreamingAudio module

Runtime module holding the streaming-audio types, their Blueprint nodes and a private MP3 decoder.

| concern | owner |
|---|---|
| contract — states, results, threading, caps, lifetime — and architecture | [docs/README.md](docs/README.md) |

**Entry points:** `FStreamingAudioFeeder` (`Public/StreamingAudioFeeder.h`),
`FStreamingAudioMp3Decoder` (`Public/StreamingAudioMp3Decoder.h`), `FStreamingAudioWav`,
`FStreamingAudioChunker`; Blueprint nodes in `Public/StreamingAudioBlueprint.h`. A runnable example
is the `StreamingAudio.Example.StreamedMp3ToQueuedPlayback` test in
[the module's tests](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/StreamingAudio/Tests/Source/StreamingAudioTests/Private/StreamingAudioTests.cpp).
