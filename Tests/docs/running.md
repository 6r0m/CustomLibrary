# Running the verification route

## Prerequisites

- An installed Unreal Engine with its bundled Python, and Visual Studio with the C++ toolchain the
  engine requires. The tested engine is listed in the
  [repository README](../../README.md).
- `ffmpeg` and `ffprobe` with `libmp3lame`, only to regenerate the StreamingAudio fixtures.
- Pass the engine folder with `--engine <UE>` or set `UE_ENGINE_ROOT`.

## Commands

Run every command from the repository root with the engine's Python,
`<UE>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe`.

| command | what it does | what passing proves |
|---|---|---|
| `Tests/run.py self-test` | runs each guard and report check against planted good and bad inputs | the checks can fail; no engine needed |
| `Tests/run.py guards [--denylist FILE] [--history RANGE]` | checks repository rules (below) | the rules hold in the working tree |
| `Tests/run.py build [PLUGIN...]` | stages each plugin from its allowlist and runs `BuildPlugin` | the plugin compiles for the editor and game targets, warning-free, and packages only allowed files |
| `Tests/run.py test [PLUGIN...]` | `build`, then a fresh host with the plugin's package and its colocated test modules, then editor automation | every test of every module of the plugin passes, and the number run equals the plugin's `Tests/expected-tests.json` |
| `Tests/run.py dev [PLUGIN...] [--filter F]` | one host with plugins from source, for fast iteration | nothing on its own; acceptance uses `test` |
| `Tests/run.py all [--denylist FILE] [--history RANGE]` | `self-test`, then `test` for every plugin, then the final guards | the whole route, including the staged and packaged files |

Test modules, fixtures and expected counts are discovered under `Plugins/<Plugin>/Tests/`. The
guards check: retired identifiers and declarations are absent; no redirect entries; every source
and packaged descriptor leaves `EngineVersion` empty; every module has `Public/` and `Private/`;
every plugin's `LICENSE` equals the root `LICENSE`; StreamingAudio never resets or unbinds a
procedural wave's queue or delegate; every owner carries its documents, every relative link
resolves and every document is routed from its parent; every module has a test module with a
declared test count; no internal investigation record is tracked or indexed; packaged Markdown
links resolve inside the package. The optional denylist scan reads a pattern file kept outside the repository
and scans tracked and untracked files, staged trees, packages and, with `--history`, the patches of
a revision range. A line containing a word-boundary escape is a regular expression; every other
line is a literal. The history scan excludes the retired root plugin descriptor, whose existing
attribution is allowed. Files are searched as Latin-1, UTF-16LE, UTF-16BE and, when non-ASCII,
UTF-8, so Windows wide strings are covered. In `all` the guards run after the packages exist;
running `guards` by itself scans whatever packages are already under `tmp/pkg/`.

## Outputs

| path | content |
|---|---|
| `tmp/stage/<plugin>` | the allowlisted copy handed to `BuildPlugin` |
| `tmp/pkg/<plugin>` | the built package |
| `tmp/host/<plugin>`, `tmp/host/dev` | generated host projects |
| `tmp/logs/` | build and editor logs |
| `tmp/reports/<label>/index.json` | automation reports |

## Tiers

- **Required:** `all` passes — every plugin packages, installs alone, and passes its tests.
- **Optional:** playback through a real audio device, and Shipping-configuration smoke runs. They
  are not part of the route.
