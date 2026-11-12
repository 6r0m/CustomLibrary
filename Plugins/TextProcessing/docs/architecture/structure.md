# Structure

## Purpose

The installable unit for text processing: one runtime module with the incremental fragmenter and
regex replacement, and the files that ship with it.

## Owns

- The descriptor: one runtime module, default loading phase, no content.
- The packaged file set: `Config/FilterPlugin.ini` adds `README.md`, `LICENSE` and `docs/` to what
  the engine packages by default.

## Does not own

- The fragmenter and regex contracts — boundaries, caps, threads, replacement semantics: the
  [module's architecture](../../Source/TextProcessing/docs/architecture/README.md).
- Where the text comes from and what happens to each fragment: the application.

## Composition

| part | responsibility | architecture |
|---|---|---|
| TextProcessing (runtime module) | the fragmenter, its Blueprint wrapper and the regex library | [module architecture](../../Source/TextProcessing/docs/architecture/README.md) |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through the engine's `Core`, `CoreUObject` and `Engine` modules, plus the ICU regular-expression
support exposed by Core.

## Invariants

- One runtime module; the plugin ships no test code and no content.
