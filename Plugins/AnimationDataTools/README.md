# AnimationDataTools

Maps named float channels and bone rotation deltas — from capture devices, solvers, data files or
services — onto a skeleton's curves and bones through ordered, data-driven rules, and writes the
result as an animation sequence in the editor.

| concern | owner |
|---|---|
| the runtime and editor modules, their entry points and contracts | [Source/README.md](Source/README.md) |
| the plugin-owned black-box tests (source-only) | [Tests/README.md](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/AnimationDataTools/Tests/README.md) |
| how the plugin is packaged and how its modules relate | [docs/README.md](docs/README.md) |

**Install:** copy this folder into your project's `Plugins/` folder and enable the plugin. Add
`"AnimationDataTools"` to a runtime module's dependencies for mapping, and `"AnimationDataToolsEditor"`
to an editor module's dependencies for writing sequences. Tested engine and platform support is
stated in the [repository README](https://github.com/6r0m/CustomLibrary/blob/develop/README.md).
