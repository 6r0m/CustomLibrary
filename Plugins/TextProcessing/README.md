# TextProcessing

Splits text that arrives in pieces — a streamed reply, a live transcript — into delimiter-terminated
fragments as soon as each one is known to be complete, emitting every fragment exactly once and in
order; `Flush` returns the text that remains when the stream ends. It also provides validated
regular-expression replacement with literal replacement text.

| concern | owner |
|---|---|
| the module, its entry points and its contract | [Source/README.md](Source/README.md) |
| how the plugin is packaged and what it depends on | [docs/README.md](docs/README.md) |
| the plugin-owned black-box tests (source-only) | [Tests/README.md](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/TextProcessing/Tests/README.md) |

**Install:** copy this folder into your project's `Plugins/` folder, enable the plugin, and add
`"TextProcessing"` to your module's dependencies. Tested engine and platform support is stated in
the [repository README](https://github.com/6r0m/CustomLibrary/blob/develop/README.md).
