# Structure

## Purpose

A set of small Unreal Engine plugins, each installable on its own, that turn data arriving from
outside the engine (channel samples, audio bytes, text) into engine assets, playback or text
fragments, plus a few Blueprint helpers that fill gaps in the engine's Blueprint surface.

## Owns

- The plugin set and the rules every plugin in it follows (Invariants below).
- The verification route that builds and tests each plugin on its own.
- The single statement of tested engine and platform support, in the root `README.md`.

## Does not own

- Each plugin's contract, states, threading and limits: the plugin's own documentation.
- Engine features the plugins build on — WAV import, procedural sound playback, animation data
  storage and evaluation, regular expressions, Blueprint compilation: Unreal Engine.
- Network transports, service protocols and credentials: the application using a plugin. Plugins
  accept bytes, samples and text and never fetch them.

## Composition

| part | responsibility | architecture |
|---|---|---|
| AnimationDataTools | maps named channels and bone rotations to a skeleton's curves and bones; writes animation sequences in the editor | [AnimationDataTools architecture](../../Plugins/AnimationDataTools/docs/architecture/README.md) |
| StreamingAudio | plays PCM that arrives in pieces, decodes streamed MP3, reads, writes and chunks WAV | [StreamingAudio architecture](../../Plugins/StreamingAudio/docs/architecture/README.md) |
| TextProcessing | splits text that arrives in pieces into delimiter-terminated fragments, and validates regex replacement | [TextProcessing architecture](../../Plugins/TextProcessing/docs/architecture/README.md) |
| UnrealUtilities | validated weighted selection, collision-sphere view test, locked-axis query and project version | [UnrealUtilities architecture](../../Plugins/UnrealUtilities/docs/architecture/README.md) |
| Tests | stages, packages, installs and tests each plugin alone, and checks the rules below | [verification architecture](../../Tests/docs/architecture/README.md) |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through engine modules only: a plugin never depends on another plugin from this repository, so any
one of them can be installed, built and removed without the others.

**Decision:** independent plugins with no shared core module.
**Why:** each capability is useful on its own, and a shared core would make every install carry
code it does not use and couple release of unrelated plugins.

Through `BuildPlugin` packages and public headers: the verification route consumes each plugin the
way a project does, so a header that only compiles inside the plugin, or a file missing from the
package, fails verification.

## Invariants

- No plugin's descriptor or build rules name another plugin from this repository.
- Each plugin builds with `BuildPlugin`, installs and passes its tests with no other plugin from
  this repository present.
- Each plugin directory carries a `LICENSE` byte-identical to the root `LICENSE`, so a plugin folder
  copied on its own keeps its licence.
- No `.uplugin` sets `EngineVersion`. The root `README.md` alone states tested engine and platform
  support; plugin documents link to it by absolute URL.
- A plugin package contains only the plugin's descriptor, `LICENSE`, `README.md`, `Config/`,
  `Content/`, `Resources/`, `Source/` and `docs/`, plus build output.
- Every module keeps its public interface in `Public/` and its implementation in `Private/`, has a
  `README.md` and its own `docs/architecture/`, and has its own host test module.
- Third-party code lives only under a module's `Private/` and keeps its licence notice: the vendored
  MP3 decoder in StreamingAudio is the only instance.
- Failures are returned as result codes with a reason where useful; caller mistakes never assert or
  crash, and a refused call leaves state unchanged.
- The repository holds no compatibility adapters and no redirect entries.

**Decision:** engine support is stated once, in the root `README.md`, and descriptors leave
`EngineVersion` empty.
**Why:** a pinned descriptor version blocks loading on engines that work, and a support statement
copied into every plugin drifts.

**Decision:** no compatibility adapters or redirects.
**Why:** every public API here is new; an adapter would carry a contract nobody here maintains.

## Accepted decisions

- Independent plugins with no shared core — see [Relationships](#relationships)
- Engine support stated once; empty `EngineVersion` — see [Invariants](#invariants)
- No compatibility adapters or redirects — see [Invariants](#invariants)

## Risks and technical debt

- **One tested engine and platform.** Only UE 5.8.3 on Win64 is built and tested; other versions
  may fail to compile. Mitigation: run the verification route on each engine before claiming it in
  the root `README.md`.
- **No packaged-build animation generation.** Sequences are written only in the editor; a packaged
  game cannot create one from channel data at runtime. The engine has no supported runtime API for
  installing compressed animation data. Mitigation, if ever needed: a runtime writer proven by a
  packaged-build probe through a supported interface before it is advertised.
- **Vendored decoder is a fixed copy.** Upstream fixes to the MP3 decoder do not arrive on their
  own. Mitigation: StreamingAudio's architecture records the vendored file's hash, so an update is
  a deliberate replacement checked by the MP3 tests.
