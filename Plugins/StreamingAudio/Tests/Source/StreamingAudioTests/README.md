# StreamingAudioTests

Tests of the StreamingAudio module. Fixture names and case constants are in
`Private/Cases/AudioCases.h`; the fixtures themselves are in `Plugins/StreamingAudio/Tests/Fixtures/`.

| group | covers |
|---|---|
| `Feeder` | odd-sized pieces, stereo alignment, queued sounds |
| `Mp3` | the fixture oracle, corruption recovery, sync window and first-output latency, header classification, Layer 2 refusal, format change, truncation |
| `Chunker`, `Wav`, `Caps` | chunk duration and alignment, WAV round trip and refusal, every memory cap |
| `Lifecycle` | finish and reset, reset interleaving with its engine-hazard control, live-wave handoff under a concurrent consumer with its control, concurrent underflow getter and reset, underflow counting, wrong thread |
| `Blueprint` | a reflected call and a compiled graph calling every node |
| `Example` | streamed MP3 decoded and fed for playback |
