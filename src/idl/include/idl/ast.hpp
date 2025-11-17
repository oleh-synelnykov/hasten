#pragma once

#include <boost/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hasten::idl::ast
{

// ---------- identifiers ----------

struct QualifiedIdentifier {
    std::vector<std::string> parts;  // e.g., {"browser","v1","App"}

    std::string to_string() const
    {
        std::string out;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i)
                out += '.';
            out += parts[i];
        }
        return out;
    }
};

// ---------- literals / constants ----------

struct Null {
};

using Bytes = std::vector<std::uint8_t>;

// clang-format off
using ConstantValue = boost::variant<
    Null,
    bool,
    std::int64_t,
    double,
    std::string,
    QualifiedIdentifier,
    Bytes
>;
// clang-format on

// ---------- primitive & user types ----------

enum class PrimitiveKind { Bool, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, String, Bytes };

struct Primitive {
    PrimitiveKind kind{};
};

struct UserType {
    QualifiedIdentifier name;
};

// Forward-declared recursive “Type”
struct Vector;
struct Map;
struct Optional;

// clang-format off
using Type = boost::variant<
    Primitive,
    UserType,
    boost::recursive_wrapper<Vector>,
    boost::recursive_wrapper<Map>,
    boost::recursive_wrapper<Optional>
>;
// clang-format on

struct Vector {
    Type element;
};

struct Map {
    Type key;
    Type value;
};

struct Optional {
    Type inner;
};

// ---------- attributes ----------

struct Attribute {
    std::string name;
    std::optional<ConstantValue> value;  // [name] or [name=value]
};

using AttributeList = std::vector<Attribute>;

// ---------- fields / params / results ----------

struct Field {
    std::uint64_t id = 0;
    Type type;
    std::string name;
    std::optional<ConstantValue> default_value;
    AttributeList attrs;
};

struct Param {
    std::uint64_t id = 0;
    Type type;
    std::string name;
    std::optional<ConstantValue> default_value;
    AttributeList attrs;
};

// result can be: single type OR tuple-like fields
struct Result {
    bool is_tuple = false;
    Type single;                      // used when !is_tuple
    std::vector<Field> tuple_fields;  // used when is_tuple
};

// ---------- declarations ----------

struct ConstantDeclaration {
    Type type;
    std::string name;
    ConstantValue value;
};

struct Enumerator {
    std::string name;
    std::optional<std::int64_t> value;
    AttributeList attrs;
};

struct Enum {
    std::string name;
    std::vector<Enumerator> items;
};

struct Struct {
    std::string name;
    std::vector<Field> fields;
};

enum class MethodKind { Rpc, Oneway, Stream, Notify };

struct Method {
    MethodKind kind = MethodKind::Rpc;
    std::string name;
    std::vector<Param> params;
    std::optional<Result> result;
    AttributeList attrs;
};

struct Interface {
    std::string name;
    std::vector<Method> methods;
};

// Any top-level declaration
// clang-format off
using Declaration = boost::variant<
    ConstantDeclaration,
    Enum,
    Struct,
    Interface
>;
// clang-format on

// ---------- module / file ----------

struct Import {
    std::string path;  // as in: import "foo/bar.hidl";
};

struct Module {
    QualifiedIdentifier name;
    std::vector<Import> imports;
    std::vector<Declaration> decls;
};

}  // namespace hasten::idl::ast
