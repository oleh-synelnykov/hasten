#pragma once

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hasten::idl::ast
{

struct PositionTaggedNode : boost::spirit::x3::position_tagged {
};

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

struct UserType : PositionTaggedNode {
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

struct Attribute : PositionTaggedNode {
    std::string name;
    std::optional<ConstantValue> value;  // [name] or [name=value]
};

using AttributeList = std::vector<Attribute>;

// ---------- fields / params / results ----------

struct Field : PositionTaggedNode {
    std::uint64_t id = 0;
    Type type;
    std::string name;
    std::optional<ConstantValue> default_value;
    AttributeList attrs;
};

struct Parameter : PositionTaggedNode {
    std::uint64_t id = 0;
    Type type;
    std::string name;
    std::optional<ConstantValue> default_value;
    AttributeList attrs;
};

using Result = boost::variant<
    Type,
    std::vector<Field>
>;

// ---------- declarations ----------

struct ConstantDeclaration {
    Type type;
    std::string name;
    ConstantValue value;
};

struct Enumerator : PositionTaggedNode {
    std::string name;
    std::optional<std::int64_t> value;
    AttributeList attrs;
};

struct Enum : PositionTaggedNode {
    std::string name;
    std::vector<Enumerator> items;
};

struct Struct : PositionTaggedNode {
    std::string name;
    std::vector<Field> fields;
};

enum class MethodKind { Rpc, Oneway, Stream, Notify };

struct Method : PositionTaggedNode {
    MethodKind kind = MethodKind::Rpc;
    std::string name;
    std::vector<Parameter> params;
    std::optional<Result> result;
    AttributeList attrs;
};

struct Interface : PositionTaggedNode {
    std::string name;
    std::vector<Method> methods;
    AttributeList attrs;
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

struct Import : PositionTaggedNode {
    std::string path;  // as in: import "foo/bar.hidl";
};

struct Module : PositionTaggedNode {
    QualifiedIdentifier name;
    std::vector<Import> imports;
    std::vector<Declaration> decls;
};

}  // namespace hasten::idl::ast

BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::QualifiedIdentifier, parts)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Primitive, kind)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::UserType, name)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Vector, element)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Map, key, value)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Optional, inner)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Attribute, name, value)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Field, id, type, name, default_value, attrs)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Parameter, id, type, name, default_value, attrs)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::ConstantDeclaration, type, name, value)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Enumerator, name, value, attrs)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Enum, name, items)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Struct, name, fields)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Method, kind, name, params, result, attrs)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Interface, name, attrs, methods)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Import, path)
BOOST_FUSION_ADAPT_STRUCT(hasten::idl::ast::Module, name, imports, decls)
