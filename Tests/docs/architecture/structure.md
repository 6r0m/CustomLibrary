# Structure

## Purpose

Prove each plugin works the way a project would use it — packaged, installed alone, consumed through
public headers and Blueprint nodes — and that the repository's rules hold.

## Owns

- The runner (`run.py`): discovery, staging, packaging, host generation, automation runs, report
  checks and guards.
- The host-project template and the shared `HostTestSupport` helper.

## Does not own

- What each plugin guarantees: each module's architecture.
- Each plugin's test modules, fixtures and declared counts: `Plugins/<Plugin>/Tests/`. They are
  black-box consumers of the plugin and are never packaged with it.
- The engine's build and automation tools: `RunUAT`, UnrealBuildTool and the editor's automation
  framework.

## Composition

| part | responsibility |
|---|---|
| runner, `run.py` | commands, discovery of plugin tests and fixtures, staging allowlist, host generation, report checks, guards and their self-test |
| [host template and shared helper](../../Host/README.md) | the target template and the Blueprint-compile support every generated host carries |
| plugin tests | `Plugins/<Plugin>/Tests/Source/`, `Fixtures/` and `expected-tests.json`, owned by that plugin |

## Relationships

The relationships at this level are drawn once, in [the main view](diagrams/main.md).

Through the `BuildPlugin` package: acceptance hosts install the package, never the source tree, so a
file missing from the package or a header that only compiles inside the plugin fails the run.

Through the automation report: a run passes only when the report exists, lists at least one test,
every test succeeded, and each module ran exactly its declared number of tests.

## Invariants

- Each plugin is tested in its own freshly generated host that contains only that plugin and the
  test modules and fixtures it owns under `Plugins/<Plugin>/Tests/`.
- Test modules include only plugins' public headers and are never packaged with a plugin.
- A stage holds only the plugin's allowlisted production entries; `Tests/` is excluded.
- A missing report, an empty run, a count different from the plugin's `Tests/expected-tests.json`,
  or a nonzero editor exit fails.
- A packaged descriptor keeps `EngineVersion` empty; `BuildPlugin` runs with `-Unversioned`.
- The final guards run after the packages exist, so the denylist scan covers staged and packaged
  files; files are searched as Latin-1 and as UTF-16LE/BE (and UTF-8 when non-ASCII). A denylist
  line with a word-boundary escape is a regular expression; the history scan excludes the retired
  root plugin descriptor, whose existing attribution is allowed.
- Every guard and report check has a planted failing input in `self-test`.
- Every engine tool runs with a timeout; the runner kills the process tree when it expires, so a
  hang fails the run instead of stalling it.
- All output stays under `tmp/`.
