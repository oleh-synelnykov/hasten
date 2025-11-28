#pragma once

#include "idl/ast.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hasten::codegen::ir
{

namespace ast = idl::ast;

struct Attribute {
    std::string name;
    std::optional<ast::ConstantValue> value;
};

struct Field {
    std::uint64_t id = 0;
    std::string name;
    ast::Type type;
    std::optional<ast::ConstantValue> default_value;
    std::vector<Attribute> attributes;
};

struct Struct {
    std::string name;
    std::vector<Field> fields;
    std::vector<Attribute> attributes;
};

struct Enumerator {
    std::string name;
    std::optional<std::int64_t> value;
    std::vector<Attribute> attributes;
};

struct Enum {
    std::string name;
    std::vector<Enumerator> values;
    std::vector<Attribute> attributes;
};

struct Method {
    std::string name;
    ast::MethodKind kind = ast::MethodKind::Rpc;
    std::vector<Field> parameters;
    std::vector<Field> result_fields;
    std::optional<ast::Type> result_type;
    std::vector<Attribute> attributes;
};

struct Interface {
    std::string name;
    std::vector<Method> methods;
    std::vector<Attribute> attributes;
};

struct Module {
    std::string name;
    std::vector<std::string> namespace_parts;
    std::vector<Struct> structs;
    std::vector<Enum> enums;
    std::vector<Interface> interfaces;
};

struct CompilationUnit {
    std::vector<Module> modules;
};

}  // namespace hasten::codegen::ir
