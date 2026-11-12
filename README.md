# CustomLibrary

Independently installable Unreal Engine plugins for animation data, streamed audio, text processing
and validated engine utilities, with C++ and Blueprint entry points.

| concern | owner |
|---|---|
| the plugins, one row each | [Plugins/README.md](Plugins/README.md) |
| how the plugin set is organised and the rules every plugin follows | [docs/README.md](docs/README.md) |
| building and testing every plugin | [Tests/README.md](Tests/README.md) |

**Tested support:** Unreal Engine 5.8.3, Win64 editor and game targets. Other engine versions and
platforms have not been built or tested.

**Install a plugin:** copy its folder from `Plugins/` into your project's `Plugins/` folder (or
into the engine's `Plugins/Marketplace/`) and enable it. Each plugin works without the others.
