# Structure

## Purpose

The installable unit for validated engine utilities: one runtime module with focused random, scene
and project libraries, and the files that ship with it.

## Owns

- The descriptor: one runtime module, default loading phase, no content.
- The packaged file set: `Config/FilterPlugin.ini` adds `README.md`, `LICENSE` and `docs/` to what
  the engine packages by default.

## Does not own

- What each utility validates and returns: the
  [module's architecture](../../Source/UnrealUtilities/docs/architecture/README.md).
- Text fragmentation and regex replacement: TextProcessing.

## Composition

| part | responsibility | architecture |
|---|---|---|
| UnrealUtilities (runtime module) | random, scene and project function libraries | [module architecture](../../Source/UnrealUtilities/docs/architecture/README.md) |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through the engine's `Core`, `CoreUObject` and `Engine` modules, and privately `EngineSettings` for
the project version.

## Invariants

- One runtime module; the plugin ships no test code and no content.
