# Main view

The plugin's single module, who uses it and what it uses. The module's own parts are in its main view.

```mermaid
flowchart LR
    Project(["Project Blueprints and code"]) -->|"Blueprint nodes, public header"| BGH
    subgraph plugin["UnrealUtilities plugin"]
        BGH["UnrealUtilities (runtime module)"]
    end
    BGH -->|"Core, CoreUObject, Engine, EngineSettings"| UE[("Unreal Engine")]
```
