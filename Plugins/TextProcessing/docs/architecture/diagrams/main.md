# Main view

The plugin's single module, who uses it and what it uses. The module's own parts are in its main view.

```mermaid
flowchart LR
    Project(["Project code and Blueprints"]) -->|"public headers, Blueprint nodes"| IT
    subgraph plugin["TextProcessing plugin"]
        IT["TextProcessing (runtime module)"]
    end
    IT -->|"Core, CoreUObject"| UE[("Unreal Engine")]
```
