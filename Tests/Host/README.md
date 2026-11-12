# Host

Template for the generated host projects: `HostEditor.Target.cs.in`, which the runner fills with
the test modules a host needs, and the shared `HostTestSupport` module used by the plugin-owned
tests. Per-plugin test modules live under `Plugins/<Plugin>/Tests/Source/`.

| concern | owner |
|---|---|
| the shared Blueprint-compile helper | [Source/README.md](Source/README.md) |
