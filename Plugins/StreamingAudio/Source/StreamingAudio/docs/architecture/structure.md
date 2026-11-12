# Structure

## Contents

- Purpose, Owns, Does not own
- Composition
- Relationships — the procedural wave, the engine WAV support, the vendored decoder
- Invariants — PCM and results, threads, feeder, MP3 decoder, chunker and WAV
- Accepted decisions
- Risks and technical debt

## Purpose

Turn audio that arrives in pieces — raw PCM or an MP3 byte stream — into sound the engine can play
while the rest is still arriving, with every memory bound, failure and thread rule explicit.

## Owns

- The PCM format record and the module's result codes.
- WAV parsing and writing for 16-bit PCM, on top of the engine's WAV support.
- Splitting PCM into frame-aligned chunks.
- Feeding PCM to a procedural sound wave, including stream reset and underflow counting.
- Decoding MPEG Layer 3 streams incrementally, including resynchronisation and format checks.
- The Blueprint nodes over all of the above.

## Does not own

- Rendering, mixing and stopping playback: the engine's audio renderer drains the wave, and the
  caller starts and stops the component that plays it.
- Importing WAV files as sound assets: the engine's WAV importer.
- Resampling, channel mixing and other sample formats: not provided; other formats are refused.
- Transport: the caller hands bytes in.

## Composition

| part | header | responsibility |
|---|---|---|
| format and results | `StreamingAudioTypes.h` | `FStreamingAudioFormat` (rate, channels) and `EStreamingAudioResult` |
| WAV | `StreamingAudioWav.h` | 16-bit PCM WAV file to format and samples, and back |
| chunker | `StreamingAudioChunker.h` | PCM to consecutive whole-frame chunks of a given duration |
| feeder | `StreamingAudioFeeder.h` | PCM pieces to a procedural wave; one-shot queued sounds |
| MP3 decoder | `StreamingAudioMp3Decoder.h` | MP3 byte pieces to PCM; frame header classification |
| Blueprint nodes | `StreamingAudioBlueprint.h` | `UStreamingAudioBlueprintLibrary`, `UStreamingAudioFeederObject`, `UStreamingAudioMp3DecoderObject` |
| vendored decoder | `Private/ThirdParty/minimp3/` | minimp3, public domain (CC0), built with Layer 3 only; `minimp3.h` SHA-256 `57e437c5c1f0e8b243885d3929c8973b5e6c778451e0100ab4251d19915cb3ad` |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through `USoundWaveProcedural`: the feeder is the producer and the audio renderer the only
consumer. The feeder queues from the game thread and reads the wave's available byte count; it
never calls `ResetAudio()` and never unbinds or clears `OnSoundWaveProceduralUnderflow`.
**Why:** the wave's queue allows one consumer, and `ResetAudio()` empties it from the calling thread
while the renderer may be draining it. The engine also defers the reset to the next render, which
clears the byte count after audio queued since the reset was already counted, so the count goes
negative and a cap measured on it no longer holds. A fresh wave per stream avoids both.

Through `FWaveModInfo` and `SerializeWaveFile`: WAV parsing and writing stay the engine's. The parser
gets its own copy of the file bytes, because it may rewrite a truncated data chunk's length in place.

Through the vendored decoder: a private implementation detail of the MP3 decoder. It confirms a frame
by the header of the frame after it, and when that header is missing it discards its bit reservoir
and rescans, which the decoder's margin rule (below) prevents inside a valid stream.

## Invariants

**PCM and results**

- PCM is signed 16-bit little-endian, interleaved, in whole frames. A format is valid with 1 to 8
  channels and 1 to 384000 frames per second. Nothing converts, resamples or mixes channels.
- Every operation returns an `EStreamingAudioResult`. Caller mistakes never assert. A refused call
  changes nothing, except that the decoder's `UnsupportedFormat` and `FormatChanged` end the stream.

**Decision:** the sample contract is signed 16-bit little-endian interleaved PCM with an explicit
format; other formats are refused, never converted.
**Why:** a silent conversion hides a producer's mistake and changes audio the caller did not ask to
change.

**Threads**

- The feeder and queued sounds belong to the game thread, because they create objects. The decoder
  belongs to the thread that created it. WAV, chunking and header classification run on any thread.
- A call from the wrong thread returns `WrongThread` and changes nothing.

**Feeder**

- States: Open, then Finished after `Flush`; `Reset` returns to Open on a new wave.
- `Append` queues every whole frame of the carried bytes plus the new bytes and carries the rest;
  `Flush` reports the partial frame it drops; `Append` and `Flush` after `Flush` return `Finished`.
- The cap is `MaxQueuedSeconds` of the stream's format, checked before anything changes against the
  current wave's available byte count. The count is exact, because nothing else queues to the
  current wave and it is never reset.
- `Reset` (also cancel) retires the current wave's underflow state, releases the wave untouched,
  creates a new wave with the same format, clears the carried bytes and returns to Open.
  `GetSoundWave()` always names the current wave. Audio already queued on the released wave plays
  out unless the caller stops the component playing it; the feeder never queues to it again.
- Underflows are counted per wave, thread-safely; underflows of a released wave never reach the
  current count. The current wave's state is published under a lock, so `GetUnderflowCount()` may
  be called from another thread while `Reset` replaces it. The feeder never synthesises audio.
- `CreateQueuedSound` queues complete PCM on a new wave whose duration is the PCM's length.

**MP3 decoder**

- Decodes MPEG-1, MPEG-2 and MPEG-2.5 Layer 3, CBR and VBR, mono and stereo. Layer 1, Layer 2 and
  free-format streams return `UnsupportedFormat`; a change of sample rate or channel count returns
  `FormatChanged`. Both move the decoder to Failed with no audio from the offending frame, and
  Failed lasts until `Reset`.
- States: Buffering until the first frame decodes, then Decoding, then Finished after `Finish`.
- Nothing is decoded before `SyncWindowBytes` (default 16 KiB, at least `DecodeMarginBytes`) have
  arrived. At 128 kbps the default delays first output by about one second of audio; a 4 KiB window
  by about a quarter of a second, at a higher risk of locking onto a false header in leading
  non-audio data.
- Until `Finish`, the last `DecodeMarginBytes` (one largest Layer 3 frame plus a header) stay
  undecoded, so a frame is decoded only once the header after it has arrived.
- Bytes that form no frame are skipped and counted in `GetSkippedBytes()`, including frames lost
  while the decoder resynchronises after damage. At `Finish`, bytes that do not complete a frame are
  counted in `GetTruncatedBytes()`.
- Held bytes never exceed `MaxBufferedBytes`; an `Append` that would exceed it returns
  `CapExceeded`.

**Decision:** the sync window is configurable with a safe default instead of one fixed policy.
**Why:** the right trade between start-up latency and false synchronisation depends on the stream.

**Chunker and WAV**

- Chunk duration must be positive and finite; each chunk holds `floor(duration × rate)` frames, at
  least one, and the last chunk holds the rest.
- WAV parsing accepts integer PCM at 16 bits per sample (an extensible header with a PCM subformat
  included) and returns `UnsupportedFormat` for anything else. PCM that is not whole frames is
  refused by every operation.

## Accepted decisions

- 16-bit PCM only, never converted — see [Invariants](#invariants), PCM and results
- Configurable MP3 sync window with a safe default — see [Invariants](#invariants), MP3 decoder

## Risks and technical debt

- **Playback through an audio device is not exercised.** The tests drive the wave's render
  callback directly; a project should listen to the result once on its target platform.
- **Queued-sound duration.** `CreateQueuedSound` sets the wave's duration to the PCM length; how a
  given engine version ends playback of a procedural wave at that duration is not covered by a test.
- **Vendored decoder is a fixed copy.** Replacing it is deliberate: update the file and its hash
  above, then run the MP3 tests.
