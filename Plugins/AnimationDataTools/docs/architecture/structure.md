# Structure

## Purpose

The installable unit for animation from channel data: a runtime module that maps data and an
editor module that turns mapped data into animation sequences.

## Owns

- The descriptor: a runtime module and an editor module, both at the default loading phase, no
  content.
- The split between runtime and editor code, and the dependency between the two modules.
- The packaged file set: `Config/FilterPlugin.ini` adds `README.md`, `LICENSE` and `docs/` to what
  the engine packages by default.

## Does not own

- Mapping rules, validation and thread rules: the
  [runtime module's architecture](../../Source/AnimationDataTools/docs/architecture/README.md).
- Sequence timing and asset writing: the
  [editor module's architecture](../../Source/AnimationDataToolsEditor/docs/architecture/README.md).
- Animation storage, compression and evaluation: the engine.

## Composition

| part | responsibility | architecture |
|---|---|---|
| AnimationDataTools (runtime module) | contract types, the pure mapper, pose-asset conversion, the profile asset | [runtime architecture](../../Source/AnimationDataTools/docs/architecture/README.md) |
| AnimationDataToolsEditor (editor module) | writes mapped clips as transient or saved sequences | [editor architecture](../../Source/AnimationDataToolsEditor/docs/architecture/README.md) |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through the mapped clip (`FAnimationDataMappedClip`): the editor module consumes what the runtime
module produces, and nothing flows back. A packaged game uses the runtime module alone.

## Invariants

- Editor-only code — the animation data controller, package saving, the asset registry — lives only
  in the editor module; the runtime module compiles for every target `BuildPlugin` builds.
- The runtime module never depends on the editor module.
- The plugin ships no test code and no content.

## Risks and technical debt

- **No runtime sequence writer.** A packaged game can map data but cannot create an animation
  sequence from it; sequences are written in the editor. The engine exposes no supported runtime
  API for installing compressed animation data: `UAnimSequence::CompressedData` is deprecated for
  removal and `GetCompressedData()` returns a read-only scoped view. A runtime writer would need
  deprecated or internal fields, which this plugin does not use.
