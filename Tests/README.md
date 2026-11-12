# Tests

The shared verification runner: it stages each plugin from its production allowlist, builds the
package, installs it alone in a generated host, and runs the black-box tests that the plugin owns.

| concern | owner |
|---|---|
| host project template and shared Blueprint-compile helper | [Host/README.md](Host/README.md) |
| how to run the route, and its architecture | [docs/README.md](docs/README.md) |

Test modules, fixtures and declared counts live with their plugin under `Plugins/<Plugin>/Tests/`.
The runner discovers them; a plugin folder therefore carries its own verification.
