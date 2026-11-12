# UnrealUtilities

Validated C++ and Blueprint utilities the engine library does not expose directly: weighted random
selection, a collision-sphere view test, a body's locked axis, and the project version string.
Each concern has its own function library.

| concern | owner |
|---|---|
| the module, its entry points and its contract | [Source/README.md](Source/README.md) |
| how the plugin is packaged and what it depends on | [docs/README.md](docs/README.md) |
| the plugin-owned black-box tests (source-only) | [Tests/README.md](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/UnrealUtilities/Tests/README.md) |

**Install:** copy this folder into your project's `Plugins/` folder and enable the plugin; the
functions appear under **Unreal Utilities** in the Blueprint action menu. From C++, add
`"UnrealUtilities"` to your module's dependencies. Tested engine and platform support is stated
in the [repository README](https://github.com/6r0m/CustomLibrary/blob/develop/README.md).
