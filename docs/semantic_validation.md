# Semantic Validation Architecture

This document briefly describes the semantic analysis pipeline. The `Validator` orchestrates a collection of independent passes, each consuming a shared `Context`. This enables easy composition of checks, optional pass selection, and reusable helpers such as the `TypeValidator`.

## Class Diagram

```mermaid
---
  config:
    class:
      hideEmptyMembersBox: true
---
classDiagram
    class Validator {
        -Program& program_
        -DiagnosticSink& sink_
        -vector<PassFactory> pass_factories_
        +add_pass_factory()
        +clear_passes()
        +use_default_passes()
        +run()
    }

    class Pass {
        <<interface>>
        +name() string
        +run(context : Context)
    }

    class Context {
        -Program& program_
        -DiagnosticSink& sink_
        -ModuleIndex module_index_
        -DeclarationIndex declarations_
        +program()
        +diagnostics()
        +module_index()
        +declaration_index()
        +resolve_user_type()
        +report()
        +qualified_name()
    }

    class TypeValidator {
        -Context& context_
        +validate(...)
        -validate_map_key(...)
    }

    class ModuleIndexPass
    class DeclarationIndexPass
    class EnumValidationPass
    class StructValidationPass
    class InterfaceValidationPass

    Validator --> Pass : uses
    Validator --> Context : creates
    Pass --> Context : uses
    StructValidationPass --> TypeValidator
    InterfaceValidationPass --> TypeValidator
    TypeValidator --> Context : uses

    ModuleIndexPass ..|> Pass
    DeclarationIndexPass ..|> Pass
    EnumValidationPass ..|> Pass
    StructValidationPass ..|> Pass
    InterfaceValidationPass ..|> Pass
```

## Sequence Diagrams

### High-level senquence
```mermaid
sequenceDiagram
    autonumber
    actor CLI
    participant Validator as Validator
    participant Context as Context
    participant Passes as Pass[]

    CLI ->> +Validator: run()
    Validator ->> Validator: instantiate_passes()
    Validator ->> +Context: create(program, sink)
    Context -->> -Validator: context

    loop for each pass
        Validator ->> +Passes: pass.run(Context)
        Passes -->> -Validator: return
    end

    Validator -->> -CLI: return
```

### Detailed sequence with current passes included

```mermaid
sequenceDiagram
    autonumber
    participant Validator as Validator
    participant Context as Context
    participant ModulePass as ModuleIndexPass
    participant DeclarationIndexPass as DeclarationIndexPass
    participant EnumValidationPass as EnumValidationPass
    participant StructValidationPass as StructValidationPass
    participant InterfaceValidationPass as InterfaceValidationPass
    participant TypeVal as TypeValidator

    activate Validator
    Validator ->> +ModulePass: run(Context)
    ModulePass ->> Context: fill module_index
    ModulePass -->> -Validator: return

    Validator ->> +DeclarationIndexPass: run(Context)
    DeclarationIndexPass ->> Context: fill declaration_index
    DeclarationIndexPass -->> -Validator: return

    Validator ->> +EnumValidationPass: run(Context)
    EnumValidationPass ->> EnumValidationPass: validate enums
    opt if has any diagnostic report
        EnumValidationPass ->> Context: report diagnostic
    end
    EnumValidationPass -->> -Validator: return

    Validator ->> +StructValidationPass: run(Context)
    StructValidationPass ->> +TypeVal: validate field types
    TypeVal -->> -StructValidationPass: return
    StructValidationPass ->> StructValidationPass: validate structs
    opt if has any diagnostic report
        StructValidationPass ->> Context: report diagnostic
    end
    StructValidationPass -->> -Validator: return

    Validator --> +InterfaceValidationPass: run(Context)
    InterfaceValidationPass ->> +TypeVal: validate method param types
    TypeVal -->> -InterfaceValidationPass: return
    InterfaceValidationPass ->> +TypeVal: validate return types
    TypeVal -->> -InterfaceValidationPass: return
    InterfaceValidationPass ->> InterfaceValidationPass: validate interfaces
    opt if has any diagnostic report
        InterfaceValidationPass ->> Context: report diagnostic
    end
    InterfaceValidationPass -->> -Validator: return

    deactivate Validator
```
