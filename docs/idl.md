# Hasten IDL

## Overview

Hasten IDL is a domain-specific language (DSL) for describing request/response RPC interfaces, messages, and related types. Every field and method parameter carries a stable numeric identifier so that schemas can evolve without breaking older binaries.

## Key properties

Each `.hidl` file starts with a `module` declaration and may import other files. Modules are dot-separated namespaces that flow into generated code. Unknown numeric IDs are ignored at runtime, letting newer schema versions interoperate with older peers as long as IDs are never reused.

It is possible to import other files to reuse declarations, constants, enums, structs, and interfaces.

C/C++ style comments are supported - `//` or `/* ... */` and they are ignored by the parser.

## Type system

- **Primitives:** `bool`, signed/unsigned integers (`i8`…`i64`, `u8`…`u64`), `f32`, `f64`, `string`, `bytes`.
- **Containers:** `vector<T>`, `map<K,V>` (scalar/hashable keys), `optional<T>`.
- **User types:** previously declared structs/enums/interfaces referenced by qualified name.
- **Interface references:** referencing an `interface` type encodes a capability handle on the wire (see “Interface references & UIDs”).

## Declarations & methods

- **Structs:** groups of numbered fields. Each field entry is `ID : type name` with an optional default value and attribute list.
- **Interfaces:** groups of methods introduced with the `interface` keyword. Methods declare numbered parameters and either a single return type or a parenthesized list of numbered result fields.
- **Constants & enums:** provide named values for reuse in schemas or defaults.

### Method kinds

- `rpc` - unary request/response.
- `oneway` - fire-and-forget without a response body.
- `stream` and `notify` - reserved keywords for future streaming/event styles.

## Protocol evolution

Every struct field, method parameter, and tuple-style return field has a **stable numeric ID** baked into the wire format. Readers skip unknown IDs and writers must never reuse them.

Guidelines:

1. Adding a new optional field/parameter with a fresh ID is backward compatible.
2. Renaming a field keeps the same ID; generated code changes do not affect the wire.
3. Type changes must be widening or otherwise wire-compatible (e.g., `u32 → u64`).
4. Removing fields requires marking them with `deprecated` attribute and reserving the ID indefinitely.

```hidl
struct User {
  1: u64 id;
  2: string name;
  3: optional<string> email; // added in v2
}

interface UserSvc {
  rpc Get(1: u64 id, 2: bool include_email = false) -> (1: User user); // include_email added in v2
}
```

Older clients simply ignore field `3` and parameter `2`; newer clients treat missing IDs as unset optionals or defaults.

## Attributes

Attributes appear in square brackets after declarations, fields, methods, or parameters. Currently supported attributes include `deprecated` and `[uid="urn:uuid:..."]` with 128-bit UUID for pinning stable type identifiers.

## Interface references & stable type IDs

- Any user type that resolves to an `interface` is encoded as a **capability reference**. On the wire it carries `{type_uid, instance_id, endpoint_hint}` so capabilities can be passed around safely.
- Interfaces (and optionally structs/enums) should declare `[uid="urn:uuid:..."]`. The CLI can mint deterministic UUIDv5 values via `hasten --assign-uids` and validate them with `--check-uids` or a `uids.json` registry.

## Tooling & workflow

Use the `hasten` CLI to turn `.hidl` files into generated C++ code:

1. Author or modify schemas.
2. Run `hasten` (via CMake integration) to generate headers/sources into `gen/`.
3. Include the generated files, link against `libhasten_runtime` (or use header-only mode), and deploy.

Flags such as `--print-ast` or `--check` aid troubleshooting without generating C++.

## Grammar

```ebnf
file                  ::= module_declaration import* declaration* EOF

module_declaration    ::= 'module' qualified_identifier ';'
import                ::= 'import' string_literal ';'

declaration           ::= constant | enum | struct | interface

constant              ::= 'const' type name '=' constant_value ';'

enum                  ::= 'enum' name '{' enum_item (',' enum_item)* ','? '}'
enum_item             ::= identifier ('=' integer_literal)? attribute_list?

struct                ::= 'struct' name '{' field* '}'
field                 ::= integer_literal ':' type name default_opt attribute_list? ';'

interface             ::= 'interface' name '{' method* '}'

method                ::= method_kind name '(' parameter_list? ')' result? attribute_list? ';'
method_kind           ::= 'rpc' | 'oneway' | 'stream' | 'notify'
parameter_list        ::= parameter (',' parameter)*
parameter             ::= integer_literal ':' type name default_opt attribute_list?

result                ::= '->' ( type | '(' return_fields ')' )
return_fields         ::= return_field (',' return_field)*
return_field          ::= integer_literal ':' type name attribute_list? ';'?

type                  ::= primitive_type | user_type | vector_type | map_type | optional_type
primitive_type        ::= 'bool' | 'i8' | 'i16' | 'i32' | 'i64' | 'u8' | 'u16' | 'u32' | 'u64' | 'f32' | 'f64' | 'string' | 'bytes'
user_type             ::= qualified_identifier
vector_type           ::= 'vector' '<' type '>'
map_type              ::= 'map' '<' type ',' type '>'
optional_type         ::= 'optional' '<' type '>'

attribute_list        ::= '[' attribute (',' attribute)* ']'
attribute             ::= identifier ( '=' constant_value )?

default_opt           ::= ( '=' constant_value )?
constant_value        ::= integer_literal | float_literal | string_literal | boolean_literal | bytes_literal | qualified_identifier | 'null'


reserved_keyword      ::= 'module' | 'import' | 'interface' | 'struct' | 'enum' | 'const' | 'rpc' | 'oneway' | 'stream' | 'notify' | 'vector' | 'map' | 'optional' | 'null' | 'bool' | 'i8' | 'i16' | 'i32' | 'i64' | 'u8' | 'u16' | 'u32' | 'u64' | 'f32' | 'f64' | 'string' | 'bytes'

name                  ::= identifier - reserved_keyword

qualified_identifier  ::= name ('.' name)*
identifier            ::= ident_start ident_continue*
ident_start           ::= '_' | LETTER
ident_continue        ::= '_' | LETTER | DIGIT

integer_literal       ::= decimal_literal | hex_literal | binary_literal | octal_literal
decimal_literal       ::= ('+' | '-')? ('0' | [1-9] DIGIT*) (int_suffix)?
hex_literal           ::= '0x' HEX+
binary_literal        ::= '0b' [01]+
octal_literal         ::= '0o' [0-7]+
int_suffix            ::= ('u' | 'U' | 'l' | 'L' | 'ul' | 'UL')?
float_literal         ::= DIGIT+ '.' DIGIT+ (exp_part)? | DIGIT+ exp_part
exp_part              ::= ('e' | 'E') ('+' | '-')? DIGIT+
string_literal        ::= '"' ( ESC | ~['"','\\'] )* '"'
bytes_literal         ::= 'b' '"' ( HEXPAIR | WS )* '"'
boolean_literal       ::= 'true' | 'false'

comment               ::= line_comment | block_comment
line_comment          ::= '//' (CHAR - EOL)* (EOL | EOF)
block_comment         ::= '/*' (CHAR* - '*/') '*/'

HEXPAIR               ::= HEX HEX
HEX                   ::= [0-9a-fA-F]
LETTER                ::= [A-Za-z]
DIGIT                 ::= [0-9]
CHAR                  ::= #x0009 | #x000A | #x000D | [#x0020-#xFFFF]
EOL                   ::= '\n' | '\r\n' | '\r'
ESC                   ::= '\\' ['"\'nrt0]
WS                    ::= [ \t\n\r]+
```
