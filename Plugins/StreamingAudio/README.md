# StreamingAudio

Plays 16-bit PCM that arrives in pieces through a procedural sound wave, decodes streamed MP3 into
that PCM, and reads, writes and chunks 16-bit WAV data — for projects that receive speech or music
over a network or generate it at runtime.

| concern | owner |
|---|---|
| the module, its entry points and its contract | [Source/README.md](Source/README.md) |
| how the plugin is packaged and what it depends on | [docs/README.md](docs/README.md) |
| the plugin-owned black-box tests (source-only) | [Tests/README.md](https://github.com/6r0m/CustomLibrary/blob/develop/Plugins/StreamingAudio/Tests/README.md) |

**Install:** copy this folder into your project's `Plugins/` folder, enable the plugin, and add
`"StreamingAudio"` to your module's dependencies. Tested engine and platform support is stated in
the [repository README](https://github.com/6r0m/CustomLibrary/blob/develop/README.md).
