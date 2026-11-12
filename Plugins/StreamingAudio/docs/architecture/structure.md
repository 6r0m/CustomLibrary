# Structure

## Purpose

The installable unit for streamed audio: one runtime module and the files that ship with it.

## Owns

- The descriptor: one runtime module, default loading phase, no content.
- The packaged file set: `Config/FilterPlugin.ini` adds `README.md`, `LICENSE` and `docs/` to what
  the engine packages by default.
- The licence obligations of the vendored MP3 decoder, which ships inside the module.

## Does not own

- The streaming contract — states, results, threading, caps: the
  [module's architecture](../../Source/StreamingAudio/docs/architecture/README.md).
- Playback, mixing and audio devices: the engine's audio renderer and `UAudioComponent`.
- Fetching audio bytes: the application.

## Composition

| part | responsibility | architecture |
|---|---|---|
| StreamingAudio (runtime module) | PCM format, WAV, chunker, feeder, MP3 decoder and their Blueprint nodes | [module architecture](../../Source/StreamingAudio/docs/architecture/README.md) |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through the engine's `Core`, `CoreUObject` and `Engine` modules only: the plugin needs no other
plugin, from this repository or the engine.

## Invariants

- One runtime module; the plugin ships no test code and no content.
- The vendored decoder is compiled only inside the module's `Private/` folder and never appears in
  a public header, so projects that use only PCM never see it.
