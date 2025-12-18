# Mermaid Flowchart Integration

This document demonstrates Kryon's native Mermaid flowchart support.

## What Makes This Special?

Unlike other markdown renderers that convert Mermaid to SVG or images, **Kryon converts Mermaid diagrams directly into native Flowchart IR components**. This means:

- ✅ Flowcharts are rendered using the same engine as `.kry` Flowchart components
- ✅ No external dependencies or JavaScript runtime needed
- ✅ Perfect integration with SDL3, terminal, and web backends
- ✅ Same styling and behavior as native components

## Simple Linear Flow

```mermaid
flowchart LR
    A[Start] --> B[Process]
    B --> C[End]
```

## Decision Tree

```mermaid
flowchart TD
    Start[Start] --> Input[Get User Input]
    Input --> Check{Valid Input?}
    Check -->|Yes| Process[Process Data]
    Check -->|No| Error[Show Error]
    Error --> Input
    Process --> Save[Save Results]
    Save --> End[End]
```

## Development Workflow

```mermaid
flowchart LR
    Code[Write Code] --> Test[Run Tests]
    Test --> Pass{Tests Pass?}
    Pass -->|Yes| Review[Code Review]
    Pass -->|No| Fix[Fix Bugs]
    Fix --> Test
    Review --> Approve{Approved?}
    Approve -->|Yes| Deploy[Deploy]
    Approve -->|No| Revise[Revise Code]
    Revise --> Code
    Deploy --> Monitor[Monitor]
```

## Data Processing Pipeline

```mermaid
flowchart TB
    Input[Input Data] --> Parse[Parse JSON]
    Parse --> Validate{Valid?}
    Validate -->|Yes| Transform[Transform Data]
    Validate -->|No| Error[Log Error]
    Transform --> Store[(Database)]
    Store --> Cache[(Cache)]
    Cache --> API[Expose API]
    Error --> Retry{Retry?}
    Retry -->|Yes| Parse
    Retry -->|No| Fail[Fail]
```

## Build System

```mermaid
flowchart LR
    Source[.kry/.nim/.md] --> Parse[Parse Source]
    Parse --> KIR[Generate .kir]
    KIR --> Validate{Valid IR?}
    Validate -->|Yes| Render[Render]
    Validate -->|No| ErrorLog[Error Report]
    Render --> SDL3[SDL3 Backend]
    Render --> Web[Web Backend]
    Render --> Terminal[Terminal Backend]
```

## State Machine

```mermaid
flowchart TD
    Idle[Idle] --> Active{User Action}
    Active -->|Click| Processing[Processing]
    Active -->|Hover| Hover[Show Tooltip]
    Hover --> Idle
    Processing --> Complete{Success?}
    Complete -->|Yes| Success[Show Result]
    Complete -->|No| Retry[Show Error]
    Success --> Idle
    Retry --> Idle
```

## Vertical Top-Down

```mermaid
flowchart TD
    A[Top] --> B[Middle]
    B --> C[Bottom]
```

## Vertical Bottom-Up

```mermaid
flowchart BT
    Bottom[Bottom] --> Middle[Middle]
    Middle --> Top[Top]
```

## Right-to-Left

```mermaid
flowchart RL
    End[End] --> Middle[Middle]
    Middle --> Start[Start]
```

## How It Works

When Kryon's markdown parser encounters a code block with the `mermaid` language tag:

1. The mermaid source is detected: ````mermaid`
2. It's passed to `ir_flowchart_parse()` (the same parser used for `.kry` Flowchart components)
3. The parser generates native `IR_COMPONENT_FLOWCHART` nodes
4. These are rendered like any other Kryon component

**No external tools needed** - the flowchart parser is built into Kryon!

---

*Run with `kryon run flowchart.md` to see interactive flowcharts rendered natively!*
