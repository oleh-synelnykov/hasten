#include "emitter.hpp"

#include "file_writer.hpp"
#include "ir.hpp"
#include "ostream_joiner.hpp"

#include <filesystem>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/get.hpp>
#include "hasten/runtime/serialization/hb1.hpp"

#include <fmt/format.h>

namespace hasten::codegen
{
namespace
{

namespace ast = idl::ast;

constexpr std::uint64_t kFNVOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFNVPrime = 1099511628211ULL;

constexpr int kIndentWidth = 4;

std::string indentation(int indent_level)
{
    return std::string(indent_level * kIndentWidth, ' ');
}

template <typename Range, typename Transformer, typename Delimiter>
void write_joined(std::ostream& os, const Range& range, Transformer&& transformer, Delimiter&& delimiter)
{
    using namespace boost::adaptors;
    boost::copy(range | transformed(std::forward<Transformer>(transformer)),
                make_ostream_joiner(os, std::forward<Delimiter>(delimiter)));
}

using TupleStructEntry = std::pair<std::string, const ir::Method*>;
using TupleNameLookup = std::unordered_map<const ir::Method*, std::string>;

struct Hb1FieldInfo {
    hasten::runtime::hb1::WireType wire_type = hasten::runtime::hb1::WireType::Varint;
    hasten::runtime::hb1::ValueKind value_kind = hasten::runtime::hb1::ValueKind::Unsigned;
    bool optional = false;
};

Hb1FieldInfo describe_type(const ast::Type& type);
std::string wire_type_literal(hasten::runtime::hb1::WireType type);

struct TupleInfo {
    std::vector<TupleStructEntry> structs;
    TupleNameLookup lookup;
};

TupleInfo build_tuple_info(const ir::Module& module)
{
    TupleInfo info;
    for (const auto& iface : module.interfaces) {
        for (const auto& method : iface.methods) {
            if (method.result_fields.empty()) {
                continue;
            }
            auto name = iface.name + method.name + std::string("Result");
            info.structs.emplace_back(name, &method);
            info.lookup.emplace(&method, std::move(name));
        }
    }
    return info;
}

std::uint64_t stable_hash(std::string_view text)
{
    std::uint64_t hash = kFNVOffset;
    for (unsigned char c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kFNVPrime;
    }
    return hash;
}

std::string hash_literal(std::string_view text)
{
    return fmt::format("{}ULL", stable_hash(text));
}

std::string sanitize_identifier(const std::string& name)
{
    std::string result;
    result.reserve(name.size());
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            result.push_back(ch == '.' ? '_' : ch);
        } else {
            result.push_back('_');
        }
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) {
        result.insert(result.begin(), '_');
    }
    return result;
}

class TypeMapper : public boost::static_visitor<std::string>
{
public:
    std::string operator()(const ast::Primitive& primitive) const
    {
        using ast::PrimitiveKind;
        switch (primitive.kind) {
            case PrimitiveKind::Bool:
                return "bool";
            case PrimitiveKind::I8:
                return "std::int8_t";
            case PrimitiveKind::I16:
                return "std::int16_t";
            case PrimitiveKind::I32:
                return "std::int32_t";
            case PrimitiveKind::I64:
                return "std::int64_t";
            case PrimitiveKind::U8:
                return "std::uint8_t";
            case PrimitiveKind::U16:
                return "std::uint16_t";
            case PrimitiveKind::U32:
                return "std::uint32_t";
            case PrimitiveKind::U64:
                return "std::uint64_t";
            case PrimitiveKind::F32:
                return "float";
            case PrimitiveKind::F64:
                return "double";
            case PrimitiveKind::String:
                return "std::string";
            case PrimitiveKind::Bytes:
                return "std::vector<std::uint8_t>";
        }
        return "void";
    }

    std::string operator()(const ast::UserType& user_type) const
    {
        std::string result;
        for (std::size_t i = 0; i < user_type.name.parts.size(); ++i) {
            if (i) {
                result += "::";
            }
            result += user_type.name.parts[i];
        }
        return result;
    }

    std::string operator()(const ast::Vector& vector) const
    {
        return fmt::format("std::vector<{}>", boost::apply_visitor(*this, vector.element));
    }

    std::string operator()(const ast::Map& map) const
    {
        return fmt::format("std::map<{}, {}>", boost::apply_visitor(*this, map.key),
                           boost::apply_visitor(*this, map.value));
    }

    std::string operator()(const ast::Optional& optional) const
    {
        return fmt::format("std::optional<{}>", boost::apply_visitor(*this, optional.inner));
    }
};

using StructLookup = std::unordered_map<std::string, const ir::Struct*>;

StructLookup build_struct_lookup(const ir::Module& module)
{
    StructLookup lookup;
    for (const auto& s : module.structs) {
        lookup.emplace(s.name, &s);
    }
    return lookup;
}

std::string qualified_name(const ir::Module& module, const std::string& symbol)
{
    if (module.namespace_parts.empty()) {
        return symbol;
    }
    std::string result;
    for (std::size_t i = 0; i < module.namespace_parts.size(); ++i) {
        if (i) {
            result += "::";
        }
        result += module.namespace_parts[i];
    }
    result += "::" + symbol;
    return result;
}

std::string sanitized_helper_name(std::string_view prefix, std::string_view name)
{
    return sanitize_identifier(std::string(prefix) + std::string(name));
}

std::string encode_struct_helper(const std::string& struct_name)
{
    return "encode_struct_" + sanitize_identifier(struct_name);
}

std::string decode_struct_helper(const std::string& struct_name)
{
    return "decode_struct_" + sanitize_identifier(struct_name);
}

std::string method_helper_prefix(const std::string& iface_name, const std::string& method_name)
{
    return sanitize_identifier(iface_name + "_" + method_name);
}

const ast::Type* unwrap_optional_type(const ast::Type& type, bool& is_optional)
{
    if (const auto* optional = boost::get<ast::Optional>(&type)) {
        is_optional = true;
        return &optional->inner;
    }
    is_optional = false;
    return &type;
}

const ir::Struct* find_struct(const StructLookup& lookup, const ast::UserType& type)
{
    if (type.name.parts.empty()) {
        return nullptr;
    }
    auto it = lookup.find(type.name.parts.back());
    if (it != lookup.end()) {
        return it->second;
    }
    return nullptr;
}

std::string qualified_type_name(const ir::Module& module, const std::string& name)
{
    return qualified_name(module, name);
}

void emit_encode_field_value(std::ostream& os,
                             int indent_level,
                             const ir::Field& field,
                             const ast::Type& type,
                             const std::string& value_expr,
                             const StructLookup& struct_lookup,
                             const ir::Module& module);

void emit_decode_field_value(std::ostream& os,
                             int indent_level,
                             const ir::Field& field,
                             const ast::Type& type,
                             const std::string& target_expr,
                             const StructLookup& struct_lookup,
                             const ir::Module& module,
                             const std::string& field_value_expr,
                             const TypeMapper& mapper);

void emit_encode_field_value(std::ostream& os,
                             int indent_level,
                             const ir::Field& field,
                             const ast::Type& type,
                             const std::string& value_expr,
                             const StructLookup& struct_lookup,
                             const ir::Module& module)
{
    bool is_optional = false;
    const ast::Type* actual = unwrap_optional_type(type, is_optional);
    const auto indent = indentation(indent_level);
    if (is_optional) {
        os << indent << "if (" << value_expr << ") {\n";
        os << indent << "    auto&& inner = *" << value_expr << ";\n";
        emit_encode_field_value(os, indent_level + 1, field, *actual, "inner", struct_lookup, module);
        os << indent << "}\n";
        return;
    }

    const auto info = describe_type(*actual);
    os << indent << "{\n";
    const auto inner_indent = indentation(indent_level + 1);
    os << inner_indent << "hasten::runtime::hb1::FieldValue field_value;\n";
    os << inner_indent << "field_value.id = " << field.id << ";\n";
    os << inner_indent << "field_value.wire_type = " << wire_type_literal(info.wire_type) << ";\n";

    if (const auto* primitive = boost::get<ast::Primitive>(actual)) {
        using ast::PrimitiveKind;
        switch (primitive->kind) {
            case PrimitiveKind::Bool:
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_unsigned((" << value_expr
                   << ") ? 1 : 0);\n";
                break;
            case PrimitiveKind::I8:
            case PrimitiveKind::I16:
            case PrimitiveKind::I32:
            case PrimitiveKind::I64:
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_signed(static_cast<std::int64_t>("
                   << value_expr << "));\n";
                break;
            case PrimitiveKind::U8:
            case PrimitiveKind::U16:
            case PrimitiveKind::U32:
            case PrimitiveKind::U64:
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_unsigned(static_cast<std::uint64_t>("
                   << value_expr << "));\n";
                break;
            case PrimitiveKind::F32: {
                os << inner_indent << "std::uint32_t bits = 0;\n";
                os << inner_indent << "std::memcpy(&bits, &" << value_expr << ", sizeof(bits));\n";
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_unsigned(bits);\n";
                break;
            }
            case PrimitiveKind::F64: {
                os << inner_indent << "std::uint64_t bits = 0;\n";
                os << inner_indent << "std::memcpy(&bits, &" << value_expr << ", sizeof(bits));\n";
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_unsigned(bits);\n";
                break;
            }
            case PrimitiveKind::String:
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_string(" << value_expr
                   << ");\n";
                break;
            case PrimitiveKind::Bytes:
                os << inner_indent
                   << "field_value.value = hasten::runtime::hb1::Value::make_bytes(" << value_expr
                   << ");\n";
                break;
        }
        os << inner_indent << "values.push_back(std::move(field_value));\n";
        os << indent << "}\n";
        return;
    }

    if (const auto* user = boost::get<ast::UserType>(actual)) {
        if (const auto* struct_ir = find_struct(struct_lookup, *user)) {
            os << inner_indent << "std::vector<std::uint8_t> nested_buffer;\n";
            os << inner_indent << "hasten::runtime::VectorSink nested_sink{nested_buffer};\n";
            os << inner_indent << "hasten::runtime::hb1::Writer nested_writer{nested_sink};\n";
            os << inner_indent << "if (auto res = " << encode_struct_helper(struct_ir->name) << "(" << value_expr
               << ", nested_writer); !res) {\n";
            os << inner_indent << "    return res;\n";
            os << inner_indent << "}\n";
            os << inner_indent
               << "field_value.value = hasten::runtime::hb1::Value::make_bytes(std::move(nested_buffer));\n";
            os << inner_indent << "values.push_back(std::move(field_value));\n";
            os << indent << "}\n";
            return;
        }
    }

    os << inner_indent
       << "return hasten::runtime::unimplemented_result(\"HB1 encode not implemented for field ''" << field.name
       << "''\");\n";
    os << indent << "}\n";
}

void emit_decode_field_value(std::ostream& os,
                             int indent_level,
                             const ir::Field& field,
                             const ast::Type& type,
                             const std::string& target_expr,
                             const StructLookup& struct_lookup,
                             const ir::Module& module,
                             const std::string& field_value_expr,
                             const TypeMapper& mapper)
{
    bool is_optional = false;
    const ast::Type* actual = unwrap_optional_type(type, is_optional);
    const auto indent = indentation(indent_level);
    if (const auto* primitive = boost::get<ast::Primitive>(actual)) {
        using ast::PrimitiveKind;
        auto type_name = boost::apply_visitor(mapper, type);
        switch (primitive->kind) {
            case PrimitiveKind::Bool:
                os << indent << target_expr << " = (" << field_value_expr
                   << ".value.unsigned_value != 0);\n";
                break;
            case PrimitiveKind::I8:
            case PrimitiveKind::I16:
            case PrimitiveKind::I32:
            case PrimitiveKind::I64:
                os << indent << target_expr << " = static_cast<" << type_name
                   << ">(" << field_value_expr << ".value.signed_value);\n";
                break;
            case PrimitiveKind::U8:
            case PrimitiveKind::U16:
            case PrimitiveKind::U32:
            case PrimitiveKind::U64:
                os << indent << target_expr << " = static_cast<" << type_name
                   << ">(" << field_value_expr << ".value.unsigned_value);\n";
                break;
            case PrimitiveKind::F32: {
                os << indent << "std::uint32_t bits32 = static_cast<std::uint32_t>(" << field_value_expr
                   << ".value.unsigned_value);\n";
                os << indent << "float decoded32 = 0.0f;\n";
                os << indent << "std::memcpy(&decoded32, &bits32, sizeof(decoded32));\n";
                os << indent << target_expr << " = decoded32;\n";
                break;
            }
            case PrimitiveKind::F64: {
                os << indent << "std::uint64_t bits64 = " << field_value_expr
                   << ".value.unsigned_value;\n";
                os << indent << "double decoded64 = 0.0;\n";
                os << indent << "std::memcpy(&decoded64, &bits64, sizeof(decoded64));\n";
                os << indent << target_expr << " = decoded64;\n";
                break;
            }
            case PrimitiveKind::String:
                os << indent << target_expr << " = " << field_value_expr << ".value.text;\n";
                break;
            case PrimitiveKind::Bytes:
                os << indent << target_expr << " = " << field_value_expr << ".value.bytes;\n";
                break;
        }
        return;
    }

    if (const auto* user = boost::get<ast::UserType>(actual)) {
        if (const auto* struct_ir = find_struct(struct_lookup, *user)) {
            os << indent << "{\n";
            const auto inner_indent = indentation(indent_level + 1);
            os << inner_indent << "auto decoded = " << decode_struct_helper(struct_ir->name) << "(";
            os << "std::span<const std::uint8_t>(" << field_value_expr << ".value.bytes.data(), "
               << field_value_expr << ".value.bytes.size()));\n";
            os << inner_indent << "if (!decoded) {\n";
            os << inner_indent << "    return std::unexpected(decoded.error());\n";
            os << inner_indent << "}\n";
            os << inner_indent << target_expr << " = std::move(*decoded);\n";
            os << indent << "}\n";
            return;
        }
    }

    os << indent
       << "return hasten::runtime::unimplemented_result(\"HB1 decode not implemented for field ''"
       << field.name << "''\");\n";
}

bool is_scalar(const ast::Type& type)
{
    struct Visitor : boost::static_visitor<bool> {
        Visitor()
            : boost::static_visitor<bool>()
        {
        }

        bool operator()(const ast::Primitive& primitive) const
        {
            using ast::PrimitiveKind;
            switch (primitive.kind) {
                case PrimitiveKind::Bool:
                case PrimitiveKind::I8:
                case PrimitiveKind::I16:
                case PrimitiveKind::I32:
                case PrimitiveKind::I64:
                case PrimitiveKind::U8:
                case PrimitiveKind::U16:
                case PrimitiveKind::U32:
                case PrimitiveKind::U64:
                case PrimitiveKind::F32:
                case PrimitiveKind::F64:
                    return true;
                case PrimitiveKind::String:
                case PrimitiveKind::Bytes:
                    return false;
            }
            return false;
        }

        bool operator()(const ast::UserType&) const
        {
            return false;
        }

        bool operator()(const ast::Vector&) const
        {
            return false;
        }

        bool operator()(const ast::Map&) const
        {
            return false;
        }

        bool operator()(const ast::Optional&) const
        {
            return false;
        }
    };

    return boost::apply_visitor(Visitor{}, type);
}

std::string parameter_declaration(const ir::Field& field, const TypeMapper& mapper)
{
    const auto type = boost::apply_visitor(mapper, field.type);
    if (is_scalar(field.type)) {
        return fmt::format("{} {}", type, field.name);
    }
    return fmt::format("const {}& {}", type, field.name);
}

void open_namespaces(std::ostream& os, const ir::Module& module)
{
    os << "namespace ";
    write_joined(os, module.namespace_parts, std::identity(), "::");
    os << " {\n";
}

void close_namespaces(std::ostream& os, const ir::Module& module)
{
    os << "}  // namespace ";
    write_joined(os, module.namespace_parts, std::identity(), "::");
    os << "\n";
}

std::string module_base_name(const ir::Module& module)
{
    if (module.namespace_parts.empty()) {
        return module.name;
    }
    std::string result;
    for (std::size_t i = 0; i < module.namespace_parts.size(); ++i) {
        if (i) {
            result += "_";
        }
        result += module.namespace_parts[i];
    }
    return result;
}

Hb1FieldInfo describe_type(const ast::Type& type)
{
    Hb1FieldInfo info;
    if (const auto* prim = boost::get<ast::Primitive>(&type)) {
        using ast::PrimitiveKind;
        switch (prim->kind) {
            case PrimitiveKind::Bool:
            case PrimitiveKind::U8:
            case PrimitiveKind::U16:
            case PrimitiveKind::U32:
            case PrimitiveKind::U64:
                info.wire_type = hasten::runtime::hb1::WireType::Varint;
                info.value_kind = hasten::runtime::hb1::ValueKind::Unsigned;
                break;
            case PrimitiveKind::I8:
            case PrimitiveKind::I16:
            case PrimitiveKind::I32:
            case PrimitiveKind::I64:
                info.wire_type = hasten::runtime::hb1::WireType::ZigZagVarint;
                info.value_kind = hasten::runtime::hb1::ValueKind::Signed;
                break;
            case PrimitiveKind::F32:
                info.wire_type = hasten::runtime::hb1::WireType::Fixed32;
                info.value_kind = hasten::runtime::hb1::ValueKind::Unsigned;
                break;
            case PrimitiveKind::F64:
                info.wire_type = hasten::runtime::hb1::WireType::Fixed64;
                info.value_kind = hasten::runtime::hb1::ValueKind::Unsigned;
                break;
            case PrimitiveKind::String:
                info.wire_type = hasten::runtime::hb1::WireType::LengthDelimited;
                info.value_kind = hasten::runtime::hb1::ValueKind::String;
                break;
            case PrimitiveKind::Bytes:
                info.wire_type = hasten::runtime::hb1::WireType::LengthDelimited;
                info.value_kind = hasten::runtime::hb1::ValueKind::Bytes;
                break;
        }
        return info;
    }
    if (const auto* opt = boost::get<ast::Optional>(&type)) {
        auto inner = describe_type(opt->inner);
        inner.optional = true;
        return inner;
    }
    // user types, vectors, maps treated as length-delimited blobs for now.
    info.wire_type = hasten::runtime::hb1::WireType::LengthDelimited;
    info.value_kind = hasten::runtime::hb1::ValueKind::Bytes;
    return info;
}

std::string wire_type_literal(hasten::runtime::hb1::WireType type)
{
    switch (type) {
        case hasten::runtime::hb1::WireType::Varint:
            return "hasten::runtime::hb1::WireType::Varint";
        case hasten::runtime::hb1::WireType::ZigZagVarint:
            return "hasten::runtime::hb1::WireType::ZigZagVarint";
        case hasten::runtime::hb1::WireType::Fixed32:
            return "hasten::runtime::hb1::WireType::Fixed32";
        case hasten::runtime::hb1::WireType::Fixed64:
            return "hasten::runtime::hb1::WireType::Fixed64";
        case hasten::runtime::hb1::WireType::LengthDelimited:
            return "hasten::runtime::hb1::WireType::LengthDelimited";
        case hasten::runtime::hb1::WireType::Capability:
            return "hasten::runtime::hb1::WireType::Capability";
    }
    return "hasten::runtime::hb1::WireType::Varint";
}

std::string value_kind_literal(hasten::runtime::hb1::ValueKind kind)
{
    switch (kind) {
        case hasten::runtime::hb1::ValueKind::Unsigned:
            return "hasten::runtime::hb1::ValueKind::Unsigned";
        case hasten::runtime::hb1::ValueKind::Signed:
            return "hasten::runtime::hb1::ValueKind::Signed";
        case hasten::runtime::hb1::ValueKind::String:
            return "hasten::runtime::hb1::ValueKind::String";
        case hasten::runtime::hb1::ValueKind::Bytes:
            return "hasten::runtime::hb1::ValueKind::Bytes";
    }
    return "hasten::runtime::hb1::ValueKind::Unsigned";
}

std::string descriptor_initializer(const ir::Field& field)
{
    auto info = describe_type(field.type);
    return fmt::format(
        "{{{}, {}, {}, {}}}",
        field.id,
        wire_type_literal(info.wire_type),
        info.optional ? "true" : "false",
        value_kind_literal(info.value_kind));
}

std::string descriptor_initializer_from_type(const ast::Type& type, std::uint32_t id = 1)
{
    ir::Field synthetic;
    synthetic.id = id;
    synthetic.type = type;
    return descriptor_initializer(synthetic);
}

std::string emit_descriptor_array(const std::string& indent,
                                  std::string_view qualifier,
                                  const std::string& name,
                                  const std::vector<ir::Field>& fields)
{
    std::ostringstream out;
    TypeMapper mapper;
    out << indent << qualifier << "std::array<hasten::runtime::hb1::FieldDescriptor, "
        << fields.size() << "> " << name << " = {";
    if (fields.empty()) {
        out << "};\n";
        return out.str();
    }
    out << "\n";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        out << indent << "    " << descriptor_initializer(fields[i]);
        out << (i + 1 == fields.size() ? "\n" : ",\n");
    }
    out << indent << "};\n";
    return out.str();
}

std::vector<ir::Field> single_field_vector(const ast::Type& type)
{
    std::vector<ir::Field> fields;
    ir::Field field;
    field.id = 1;
    field.type = type;
    fields.push_back(field);
    return fields;
}

std::string method_result_type(const ir::Method& method, const TypeMapper& mapper,
                               const std::unordered_map<const ir::Method*, std::string>& tuple_names)
{
    if (!method.result_fields.empty()) {
        auto it = tuple_names.find(&method);
        if (it != tuple_names.end()) {
            return it->second;
        }
    }
    if (method.result_type) {
        return boost::apply_visitor(mapper, *method.result_type);
    }
    return "void";
}

void generate_header_comment(std::ostream& os)
{
    os << "// Auto-generated by Hasten. DO NOT EDIT MANUALLY.\n";
    os << "#pragma once\n\n";
}

void generate_header_includes(std::ostream& os)
{
    os << "#include \"hasten/runtime/channel.hpp\"\n";
    os << "#include \"hasten/runtime/executor.hpp\"\n";
    os << "#include \"hasten/runtime/result.hpp\"\n";
    os << "#include \"hasten/runtime/rpc.hpp\"\n";
    os << "#include \"hasten/runtime/serialization/hb1.hpp\"\n";
    os << "#include \"hasten/runtime/serialization/payload.hpp\"\n";
    os << "#include \"hasten/runtime/uds.hpp\"\n";
    os << "\n";
    os << "#include <array>\n";
    os << "#include <cstdint>\n";
    os << "#include <cstring>\n";
    os << "#include <expected>\n";
    os << "#include <functional>\n";
    os << "#include <future>\n";
    os << "#include <map>\n";
    os << "#include <memory>\n";
    os << "#include <optional>\n";
    os << "#include <span>\n";
    os << "#include <string>\n";
    os << "#include <vector>\n";
    os << "\n";
}

void generate_enum(std::ostream& os, int indent_level, const ir::Enum& enum_ir)
{
    const auto indent = indentation(indent_level);
    const auto value_indent = indentation(indent_level + 1);

    os << indent << "enum class " << enum_ir.name << " {\n";

    // helper to format enumerator with proper indentation
    auto to_indented_string = [&value_indent](const ir::Enumerator& e) {
        std::string name = value_indent + e.name;
        if (e.value) {
            name += " = " + std::to_string(*e.value);
        }
        return name;
    };

    write_joined(os, enum_ir.values, to_indented_string, ",\n");
    os << '\n';
    os << indent << "};\n\n";
}

void generate_struct_definition(std::ostream& os, int indent_level, const std::string& name,
                                const std::vector<ir::Field>& fields, const TypeMapper& mapper)
{
    const auto indent = indentation(indent_level);
    const auto field_indent = indentation(indent_level + 1);
    os << indent << "struct " << name << " {\n";
    for (const auto& field : fields) {
        os << field_indent << boost::apply_visitor(mapper, field.type) << " " << field.name
           << ";  // field id: " << field.id << "\n";
    }
    os << indent << "};\n\n";
}

void generate_tuple_structs(std::ostream& os, int indent_level, const TupleInfo& tuple_info,
                            const TypeMapper& mapper)
{
    for (const auto& [name, method] : tuple_info.structs) {
        generate_struct_definition(os, indent_level, name, method->result_fields, mapper);
    }
}

void generate_interface(std::ostream& os, int indent_level, const ir::Interface& iface,
                        const TypeMapper& mapper, const TupleNameLookup& tuple_names)
{
    const auto class_indent = indentation(indent_level);
    const auto access_indent = indentation(indent_level);
    const auto member_indent = indentation(indent_level + 1);
    const auto client_name = iface.name + "Client";

    const auto parameter_decl = [&](const ir::Field& field) {
        return parameter_declaration(field, mapper);
    };

    // Client class
    os << class_indent << "class " << client_name << " {\n";
    os << access_indent << "public:\n";
    os << member_indent << "// Constructor\n";
    os << member_indent << client_name << "(std::shared_ptr<hasten::runtime::Channel> channel,\n"
       << member_indent << "     std::shared_ptr<hasten::runtime::Dispatcher> dispatcher);\n\n";
    for (const auto& method : iface.methods) {
        const auto result_type = method_result_type(method, mapper, tuple_names);
        const auto callback_type =
            fmt::format("std::function<void(hasten::runtime::Result<{}>)>", result_type);

        os << member_indent << "// " << method.name << "\n";
        os << member_indent << "void " << method.name << "(";
        write_joined(os, method.parameters, parameter_decl, ", ");
        if (!method.parameters.empty()) {
            os << ", ";
        }
        os << callback_type << " callback) const;\n";

        os << member_indent << "std::future<hasten::runtime::Result<" << result_type << ">> " << method.name
           << "_async(";
        write_joined(os, method.parameters, parameter_decl, ", ");
        os << ") const;\n";

        os << member_indent << "hasten::runtime::Result<" << result_type << "> " << method.name << "_sync(";
        write_joined(os, method.parameters, parameter_decl, ", ");
        os << ") const;\n\n";
    }
    os << access_indent << "private:\n";
    os << member_indent << "std::shared_ptr<hasten::runtime::Channel> channel_;\n";
    os << member_indent << "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher_;\n";
    os << class_indent << "};\n\n";

    // Service interface
    os << class_indent << "class " << iface.name << " {\n";
    os << access_indent << "public:\n";
    os << member_indent << "virtual ~" << iface.name << "() = default;\n";
    for (const auto& method : iface.methods) {
        const auto result_type = method_result_type(method, mapper, tuple_names);
        os << member_indent << "virtual hasten::runtime::Result<" << result_type << "> " << method.name
           << "(";
        write_joined(os, method.parameters, parameter_decl, ", ");
        os << ") = 0;\n";
    }
    os << class_indent << "};\n\n";

    os << class_indent << "void bind_" << iface.name << "(std::shared_ptr<" << iface.name
       << "> implementation,\n"
       << class_indent << "             std::shared_ptr<hasten::runtime::Executor> executor = nullptr);\n"
       << '\n';
    os << class_indent << "std::shared_ptr<" << client_name << "> make_" << iface.name
       << "_client(std::shared_ptr<hasten::runtime::Channel> channel,\n"
       << class_indent
       << "                                          std::shared_ptr<hasten::runtime::Dispatcher> "
          "dispatcher);\n"
       << '\n';
}

void generate_uds_client_creation(std::ostream& os, int indent_level, const ir::Interface& iface)
{
    const std::string indent = indentation(indent_level);
    const std::string level = indentation(1);

    const auto client_name = iface.name + "Client";
    os << indent << "inline\n"
       << indent << "hasten::runtime::Result<std::shared_ptr<" << client_name << ">>\n"
       << indent << "make_" << iface.name << "_client_uds(const std::string& path)\n"
       << indent << "{\n"
       << indent << level << "auto channel_result = hasten::runtime::uds::connect(path);\n"
       << indent << level << "if (!channel_result) {\n"
       << indent << level << level << "return std::unexpected(channel_result.error());\n"
       << indent << level << "}\n"
       << indent << level << "auto dispatcher = hasten::runtime::uds::make_dispatcher();\n"
       << indent << level << "return make_" << iface.name
       << "_client(std::move(channel_result.value()), dispatcher);\n"
       << indent << "}\n\n";
}

void generate_detail_namespace(std::ostream& os,
                               int indent_level,
                               const ir::Module& module,
                               const StructLookup& struct_lookup,
                               const TupleInfo& tuple_info,
                               const TypeMapper& mapper)
{
    const std::string indent = indentation(indent_level);
    const std::string level = indentation(1);

    os << indent << "namespace detail {\n\n";

    os << indent << "inline void append_varint(std::vector<std::uint8_t>& buffer, std::uint64_t value) {\n";
    os << indent << level << "while (value >= 0x80) {\n";
    os << indent << level << level << "buffer.push_back(static_cast<std::uint8_t>(value | 0x80));\n";
    os << indent << level << level << "value >>= 7;\n";
    os << indent << level << "}\n";
    os << indent << level << "buffer.push_back(static_cast<std::uint8_t>(value));\n";
    os << indent << "}\n";

    os << indent << "inline hasten::runtime::Result<std::uint64_t> read_varint(const std::vector<std::uint8_t>& buffer, std::size_t& offset) {\n";
    os << indent << level << "std::uint64_t result = 0;\n";
    os << indent << level << "int shift = 0;\n";
    os << indent << level << "while (offset < buffer.size()) {\n";
    os << indent << level << level << "std::uint8_t byte = buffer[offset++];\n";
    os << indent << level << level << "result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;\n";
    os << indent << level << level << "if ((byte & 0x80) == 0) {\n";
    os << indent << level << level << level << "return result;\n";
    os << indent << level << level << "}\n";
    os << indent << level << level << "shift += 7;\n";
    os << indent << level << level << "if (shift >= 64) {\n";
    os << indent << level << level << level << "return hasten::runtime::unexpected_result<std::uint64_t>(hasten::runtime::ErrorCode::TransportError, \"varint too long\");\n";
    os << indent << level << level << "}\n";
    os << indent << level << "}\n";
    os << indent << level << "return hasten::runtime::unexpected_result<std::uint64_t>(hasten::runtime::ErrorCode::TransportError, \"truncated varint\");\n";
    os << indent << "}\n\n";

    for (const auto& struct_ir : module.structs) {
        const auto array_name = struct_ir.name + "_FieldDescriptors";
        os << emit_descriptor_array(indent, "inline constexpr ", array_name, struct_ir.fields) << "\n";
    }

    for (const auto& iface : module.interfaces) {
        const auto iface_meta_name = iface.name + "Metadata";
        std::string iface_full_name = module.name + "." + iface.name;
        os << indent << "struct " << iface_meta_name << " {\n";
        os << indent << level << "static constexpr std::uint64_t module_id = " << hash_literal(module.name) << ";\n";
        os << indent << level << "static constexpr std::uint64_t interface_id = "
           << hash_literal(iface_full_name) << ";\n\n";
        for (const auto& method : iface.methods) {
            const auto method_meta_name = method.name + "Metadata";
            std::string method_full_name = iface_full_name + "." + method.name;
            os << indent << level << "struct " << method_meta_name << " {\n";
            os << indent << level << level << "static constexpr std::uint64_t method_id = "
               << hash_literal(method_full_name) << ";\n";
            os << emit_descriptor_array(indent + "        ",
                                         "static constexpr ",
                                         "RequestFields",
                                         method.parameters);
            if (!method.result_fields.empty()) {
                os << emit_descriptor_array(indent + "        ",
                                             "static constexpr ",
                                             "ResponseFields",
                                             method.result_fields);
            } else if (method.result_type) {
                auto single = single_field_vector(*method.result_type);
                os << emit_descriptor_array(indent + "        ",
                                             "static constexpr ",
                                             "ResponseFields",
                                             single);
            } else {
                std::vector<ir::Field> none;
                os << emit_descriptor_array(indent + "        ",
                                             "static constexpr ",
                                             "ResponseFields",
                                             none);
            }
            os << indent << "    };\n\n";
        }
        os << indent << "};\n\n";
    }

    // Struct serializers
    for (const auto& struct_ir : module.structs) {
        const auto qualified = qualified_type_name(module, struct_ir.name);
        const auto encode_name = encode_struct_helper(struct_ir.name);
        const auto decode_name = decode_struct_helper(struct_ir.name);

        os << indent << "inline hasten::runtime::Result<void> " << encode_name
           << "(const " << qualified << "& value, hasten::runtime::hb1::Writer& writer)\n";
        os << indent << "{\n";
        os << indent << level << "std::vector<hasten::runtime::hb1::FieldValue> values;\n";
        os << indent << level << "values.reserve(" << struct_ir.fields.size() << ");\n";
        for (const auto& field : struct_ir.fields) {
            emit_encode_field_value(os, indent_level + 1, field, field.type, "value." + field.name, struct_lookup, module);
        }
        os << indent << level
           << "hasten::runtime::hb1::MessageDescriptor descriptor{" << struct_ir.name
           << "_FieldDescriptors};\n";
        os << indent << level << "return hasten::runtime::hb1::encode_message(descriptor, values, writer);\n";
        os << indent << "}\n\n";

        os << indent << "inline hasten::runtime::Result<" << qualified << "> " << decode_name
           << "(std::span<const std::uint8_t> bytes)\n";
        os << indent << "{\n";
        os << indent << level << qualified << " value{};\n";
        for (const auto& field : struct_ir.fields) {
            bool optional = false;
            unwrap_optional_type(field.type, optional);
            const auto presence = sanitize_identifier("has_" + field.name);
            if (!optional) {
                os << indent << level << "bool " << presence << " = false;\n";
            }
        }
        os << indent << level << "hasten::runtime::SpanSource source{bytes};\n";
        os << indent << level << "hasten::runtime::hb1::Reader reader{source};\n";
        os << indent << level
           << "auto decoded = hasten::runtime::hb1::decode_message({" << struct_ir.name
           << "_FieldDescriptors}, reader);\n";
        os << indent << level << "if (!decoded) {\n";
        os << indent << level << "    return std::unexpected(decoded.error());\n";
        os << indent << level << "}\n";
        os << indent << level << "for (const auto& field_value : *decoded) {\n";
        os << indent << level << "    switch (field_value.id) {\n";
        for (const auto& field : struct_ir.fields) {
            os << indent << level << "        case " << field.id << ": {\n";
            emit_decode_field_value(os,
                                    indent_level + 3,
                                    field,
                                    field.type,
                                    "value." + field.name,
                                    struct_lookup,
                                    module,
                                    "field_value",
                                    mapper);
            bool optional = false;
            unwrap_optional_type(field.type, optional);
            if (!optional) {
                const auto presence = sanitize_identifier("has_" + field.name);
            os << indentation(indent_level + 3) << presence << " = true;\n";
            }
            os << indent << level << "            break;\n";
            os << indent << level << "        }\n";
        }
        os << indent << level << "        default: break;\n";
        os << indent << level << "    }\n";
        os << indent << "    }\n";
        for (const auto& field : struct_ir.fields) {
            bool optional = false;
            unwrap_optional_type(field.type, optional);
            if (!optional) {
                const auto presence = sanitize_identifier("has_" + field.name);
                os << indent << level << "if (!" << presence << ") {\n";
                os << indent << level
                   << "    return hasten::runtime::unexpected_result<" << qualified
                   << ">(hasten::runtime::ErrorCode::TransportError, \"missing field " << field.name
                   << "\");\n";
                os << indent << level << "}\n";
            }
        }
        os << indent << level << "return value;\n";
        os << indent << "}\n\n";
    }

    // Method helpers
    for (const auto& iface : module.interfaces) {
        for (const auto& method : iface.methods) {
            const auto prefix = method_helper_prefix(iface.name, method.name);
            const auto request_helper = "encode_" + prefix + "_request";
            const auto decode_request_helper = "decode_" + prefix + "_request";
            const auto request_struct = prefix + "Request";
            const auto method_meta_name = method.name + std::string("Metadata");
            const auto method_scope = iface.name + "Metadata::" + method_meta_name + "::";
            const auto request_field_symbol = method_scope + "RequestFields";
            const auto response_field_symbol = method_scope + "ResponseFields";

            // Request encode function
            os << indent << "inline hasten::runtime::Result<void> " << request_helper
               << "(hasten::runtime::hb1::Writer& writer";
            for (const auto& field : method.parameters) {
                os << ", const " << boost::apply_visitor(mapper, field.type) << "& " << field.name;
            }
            os << ")\n" << indent << "{\n";
            os << indent << level << "std::vector<hasten::runtime::hb1::FieldValue> values;\n";
            os << indent << level << "values.reserve(" << method.parameters.size() << ");\n";
            for (const auto& field : method.parameters) {
                emit_encode_field_value(os, indent_level + 1, field, field.type, field.name, struct_lookup, module);
            }
            os << indent << level << "hasten::runtime::hb1::MessageDescriptor descriptor{" << request_field_symbol
               << "};\n";
            os << indent << level << "return hasten::runtime::hb1::encode_message(descriptor, values, writer);\n";
            os << indent << "}\n\n";

            // Request struct + decode
            os << indent << "struct " << request_struct << " {\n";
            for (const auto& field : method.parameters) {
                os << indent << level << boost::apply_visitor(mapper, field.type) << " " << field.name << ";\n";
            }
            os << indent << "};\n\n";

            os << indent << "inline hasten::runtime::Result<" << request_struct << "> "
               << decode_request_helper << "(std::span<const std::uint8_t> bytes)\n";
            os << indent << "{\n";
            os << indent << level << request_struct << " request{};\n";
            for (const auto& field : method.parameters) {
                bool optional = false;
                unwrap_optional_type(field.type, optional);
                if (!optional) {
                    os << indent << level << "bool " << sanitize_identifier("has_" + field.name) << " = false;\n";
                }
            }
            os << indent << level << "hasten::runtime::SpanSource source{bytes};\n";
            os << indent << level << "hasten::runtime::hb1::Reader reader{source};\n";
            os << indent << level
               << "auto decoded = hasten::runtime::hb1::decode_message({" << request_field_symbol
               << "}, reader);\n";
            os << indent << level << "if (!decoded) {\n";
            os << indent << level << "    return std::unexpected(decoded.error());\n";
            os << indent << level << "}\n";
            os << indent << level << "for (const auto& field_value : *decoded) {\n";
            os << indent << level << "    switch (field_value.id) {\n";
            for (const auto& field : method.parameters) {
                os << indent << level << "        case " << field.id << ": {\n";
                emit_decode_field_value(os,
                                        indent_level + 3,
                                        field,
                                        field.type,
                                        "request." + field.name,
                                        struct_lookup,
                                        module,
                                        "field_value",
                                        mapper);
                bool optional = false;
                unwrap_optional_type(field.type, optional);
                if (!optional) {
                    os << indentation(indent_level + 3)
                       << sanitize_identifier("has_" + field.name) << " = true;\n";
                }
                os << indent << level << "            break;\n";
                os << indent << level << "        }\n";
            }
            os << indent << level << "        default: break;\n";
            os << indent << level << "    }\n";
            os << indent << "    }\n";
            for (const auto& field : method.parameters) {
                bool optional = false;
                unwrap_optional_type(field.type, optional);
                if (!optional) {
                    os << indent << level << "if (!" << sanitize_identifier("has_" + field.name) << ") {\n";
                    os << indent << level
                       << "    return hasten::runtime::unexpected_result<" << request_struct
                       << ">(hasten::runtime::ErrorCode::TransportError, \"missing parameter " << field.name
                       << "\");\n";
                    os << indent << level << "}\n";
                }
            }
            os << indent << level << "return request;\n";
            os << indent << "}\n\n";

            // Response helpers if method returns value
            std::vector<ir::Field> response_fields;
            if (!method.result_fields.empty()) {
                response_fields = method.result_fields;
            } else if (method.result_type) {
                response_fields = single_field_vector(*method.result_type);
                response_fields.front().name = "value";
            }

            const auto response_type = method_result_type(method, mapper, tuple_info.lookup);
            if (!response_fields.empty()) {
                const auto encode_response = "encode_" + prefix + "_response";
                const auto decode_response = "decode_" + prefix + "_response";

                os << indent << "inline hasten::runtime::Result<void> " << encode_response
                   << "(hasten::runtime::hb1::Writer& writer, const " << response_type << "& value)\n";
                os << indent << "{\n";
                os << indent << level << "std::vector<hasten::runtime::hb1::FieldValue> values;\n";
                os << indent << level << "values.reserve(" << response_fields.size() << ");\n";
                for (const auto& field : response_fields) {
                    emit_encode_field_value(os, indent_level + 1, field, field.type, "value." + field.name, struct_lookup, module);
                }
                os << indent << level << "hasten::runtime::hb1::MessageDescriptor descriptor{" << response_field_symbol
                   << "};\n";
                os << indent << level << "return hasten::runtime::hb1::encode_message(descriptor, values, writer);\n";
                os << indent << "}\n\n";

                os << indent << "inline hasten::runtime::Result<" << response_type << "> " << decode_response
                   << "(std::span<const std::uint8_t> bytes)\n";
                os << indent << "{\n";
                os << indent << level << response_type << " value{};\n";
                for (const auto& field : response_fields) {
                    bool optional = false;
                    unwrap_optional_type(field.type, optional);
                    if (!optional) {
                        os << indent << level << "bool " << sanitize_identifier("has_" + field.name) << " = false;\n";
                    }
                }
                os << indent << level << "hasten::runtime::SpanSource source{bytes};\n";
                os << indent << level << "hasten::runtime::hb1::Reader reader{source};\n";
                os << indent << level
                   << "auto decoded = hasten::runtime::hb1::decode_message({" << response_field_symbol
                   << "}, reader);\n";
                os << indent << level << "if (!decoded) {\n";
                os << indent << level << "    return std::unexpected(decoded.error());\n";
                os << indent << level << "}\n";
                os << indent << level << "for (const auto& field_value : *decoded) {\n";
                os << indent << level << "    switch (field_value.id) {\n";
                for (const auto& field : response_fields) {
                    os << indent << level << "        case " << field.id << ": {\n";
                    emit_decode_field_value(os,
                                            indent_level + 3,
                                            field,
                                            field.type,
                                            "value." + field.name,
                                            struct_lookup,
                                            module,
                                            "field_value",
                                            mapper);
                    bool optional = false;
                    unwrap_optional_type(field.type, optional);
                    if (!optional) {
                os << indentation(indent_level + 3)
                   << sanitize_identifier("has_" + field.name) << " = true;\n";
                    }
                    os << indent << level << "            break;\n";
                    os << indent << level << "        }\n";
                }
                os << indent << level << "        default: break;\n";
                os << indent << level << "    }\n";
                os << indent << "    }\n";
                for (const auto& field : response_fields) {
                    bool optional = false;
                    unwrap_optional_type(field.type, optional);
                    if (!optional) {
                        os << indent << level << "if (!" << sanitize_identifier("has_" + field.name) << ") {\n";
                        os << indent << level
                           << "    return hasten::runtime::unexpected_result<" << response_type
                           << ">(hasten::runtime::ErrorCode::TransportError, \"missing response field "
                           << field.name << "\");\n";
                        os << indent << level << "}\n";
                    }
                }
                os << indent << level << "return value;\n";
                os << indent << "}\n\n";
            }
        }
    }

    os << indent << "}  // namespace detail\n\n";
}

std::string generate_header(const ir::Module& module, const TupleInfo& tuple_info,
                            const GenerationOptions& options)
{
    (void)options;
    std::ostringstream out;
    TypeMapper mapper;
    auto struct_lookup = build_struct_lookup(module);

    generate_header_comment(out);
    generate_header_includes(out);

    open_namespaces(out, module);

    // const int indent_level = static_cast<int>(module.namespace_parts.size());
    const int indent_level = 0;
    out << indentation(indent_level) << "// Module: " << module.name << "\n\n";

    for (const auto& enum_ir : module.enums) {
        generate_enum(out, indent_level, enum_ir);
    }

    for (const auto& struct_ir : module.structs) {
        generate_struct_definition(out, indent_level, struct_ir.name, struct_ir.fields, mapper);
    }

    generate_tuple_structs(out, indent_level, tuple_info, mapper);

    for (const auto& iface : module.interfaces) {
        generate_interface(out, indent_level, iface, mapper, tuple_info.lookup);
    }

    for (const auto& iface : module.interfaces) {
        generate_uds_client_creation(out, indent_level, iface);
    }

    generate_detail_namespace(out, indent_level, module, struct_lookup, tuple_info, mapper);

    close_namespaces(out, module);
    out << '\n';
    return out.str();
}

std::string generate_client_source(const ir::Module& module, const ir::Interface& iface,
                                   const TupleNameLookup& tuple_names, const GenerationOptions& options)
{
    (void)options;
    std::ostringstream out;
    TypeMapper mapper;

    out << "// Auto-generated client stubs for module " << module.name << ", interface " << iface.name
        << "\n\n";
    out << "#include \"" << module_base_name(module) << ".gen.hpp\"\n\n";
    out << "#include <future>\n";
    out << "#include <utility>\n\n";

    open_namespaces(out, module);
    out << "\n";

    const int indent_level = static_cast<int>(module.namespace_parts.size());
    const auto indent = indentation(indent_level);
    const auto body_indent = indentation(indent_level + 1);
    const auto nested_indent = indentation(indent_level + 2);

    const auto client_name = iface.name + "Client";
    out << indent << client_name << "::" << client_name
        << "(std::shared_ptr<hasten::runtime::Channel> channel,\n"
        << indent << "                 std::shared_ptr<hasten::runtime::Dispatcher> dispatcher)\n"
        << indent << "    : channel_(std::move(channel))\n"
        << indent << "    , dispatcher_(std::move(dispatcher))\n"
        << indent << "{\n"
        << indent << "}\n\n";

    const auto parameter_decl = [&](const ir::Field& field) {
        return parameter_declaration(field, mapper);
    };

    const auto parameter_name = [](const ir::Field& field) {
        return field.name;
    };

    for (const auto& method : iface.methods) {
        const auto result_type = method_result_type(method, mapper, tuple_names);
        const auto callback_type =
            fmt::format("std::function<void(hasten::runtime::Result<{}>)>", result_type);
        const auto method_meta_name = method.name + std::string("Metadata");
        const auto meta_prefix = "detail::" + iface.name + std::string("Metadata::") + method_meta_name + "::";
        const auto request_field_symbol = meta_prefix + "RequestFields";
        const auto response_field_symbol = meta_prefix + "ResponseFields";
        const auto helper_prefix = method_helper_prefix(iface.name, method.name);
        const auto encode_request_helper = "detail::encode_" + helper_prefix + "_request";
        const auto decode_response_helper = "detail::decode_" + helper_prefix + "_response";
        const bool has_response = !method.result_fields.empty() || method.result_type.has_value();

        out << indent << "void " << client_name << "::" << method.name << "(";
        write_joined(out, method.parameters, parameter_decl, ", ");
        if (!method.parameters.empty()) {
            out << ", ";
        }
        out << callback_type << " callback) const\n";
        out << indent << "{\n";
        out << body_indent << "auto callback_fn = callback;\n";
        out << body_indent << "if (!channel_ || !dispatcher_) {\n";
        out << nested_indent << "if (callback_fn) {\n";
        out << nested_indent << "    callback_fn(hasten::runtime::unexpected_result<" << result_type
            << ">(hasten::runtime::ErrorCode::TransportError, \"channel not ready\"));\n";
        out << nested_indent << "}\n";
        out << nested_indent << "return;\n";
        out << body_indent << "}\n";
        out << body_indent << "auto stream_id = dispatcher_->open_stream();\n";
        out << body_indent << "std::vector<std::uint8_t> frame_payload;\n";
        out << body_indent << "frame_payload.reserve(64);\n";
        out << body_indent << "detail::append_varint(frame_payload, detail::" << iface.name
            << "Metadata::module_id);\n";
        out << body_indent << "detail::append_varint(frame_payload, detail::" << iface.name
            << "Metadata::interface_id);\n";
        out << body_indent << "detail::append_varint(frame_payload, " << meta_prefix << "method_id);\n";
        out << body_indent << "detail::append_varint(frame_payload, static_cast<std::uint64_t>(hasten::runtime::Encoding::Hb1));\n";
        out << body_indent << "detail::append_varint(frame_payload, stream_id);\n";
        out << body_indent << "std::vector<std::uint8_t> message_body;\n";
        out << body_indent << "hasten::runtime::VectorSink sink{message_body};\n";
        out << body_indent << "hasten::runtime::hb1::Writer writer{sink};\n";
        out << body_indent << "if (auto res = " << encode_request_helper << "(writer";
        for (const auto& param : method.parameters) {
            out << ", " << param.name;
        }
        out << "); !res) {\n";
        out << nested_indent << "dispatcher_->close_stream(stream_id);\n";
        out << nested_indent << "if (callback_fn) {\n";
        out << nested_indent << "    callback_fn(std::unexpected(res.error()));\n";
        out << nested_indent << "}\n";
        out << nested_indent << "return;\n";
        out << body_indent << "}\n";
        out << body_indent << "frame_payload.insert(frame_payload.end(), message_body.begin(), message_body.end());\n";
        out << body_indent << "dispatcher_->set_response_handler(stream_id,\n";
        out << body_indent
            << "    [dispatcher = dispatcher_, callback_fn, stream_id](hasten::runtime::rpc::Response response) mutable {\n";
        out << body_indent << "        if (dispatcher) {\n";
        out << body_indent << "            dispatcher->close_stream(stream_id);\n";
        out << body_indent << "        }\n";
        out << body_indent << "        if (!callback_fn) {\n";
        out << body_indent << "            return;\n";
        out << body_indent << "        }\n";
        out << body_indent << "        if (response.status != hasten::runtime::rpc::Status::Ok) {\n";
        out << body_indent << "            callback_fn(hasten::runtime::unexpected_result<" << result_type
            << ">(hasten::runtime::ErrorCode::InternalError, \"rpc error\"));\n";
        out << body_indent << "            return;\n";
        out << body_indent << "        }\n";
        if (has_response) {
            out << body_indent << "        auto decoded = " << decode_response_helper << "(response.body);\n";
            out << body_indent << "        if (!decoded) {\n";
            out << body_indent << "            callback_fn(std::unexpected(decoded.error()));\n";
            out << body_indent << "            return;\n";
            out << body_indent << "        }\n";
            out << body_indent << "        callback_fn(hasten::runtime::Result<" << result_type
                << ">{std::move(*decoded)});\n";
        } else {
            out << body_indent << "        callback_fn(hasten::runtime::Result<void>{});\n";
        }
        out << body_indent << "    });\n";
        out << body_indent << "hasten::runtime::Frame frame;\n";
        out << body_indent << "frame.header.type = hasten::runtime::FrameType::Data;\n";
        out << body_indent << "frame.header.flags = hasten::runtime::FrameFlagEndStream;\n";
        out << body_indent << "frame.header.stream_id = stream_id;\n";
        out << body_indent << "frame.payload = std::move(frame_payload);\n";
        out << body_indent << "if (auto res = channel_->send(std::move(frame)); !res) {\n";
        out << nested_indent << "dispatcher_->take_response_handler(stream_id);\n";
        out << nested_indent << "dispatcher_->close_stream(stream_id);\n";
        out << nested_indent << "if (callback_fn) {\n";
        out << nested_indent << "    callback_fn(std::unexpected(res.error()));\n";
        out << nested_indent << "}\n";
        out << nested_indent << "return;\n";
        out << body_indent << "}\n";
        out << indent << "}\n\n";

        out << indent << "std::future<hasten::runtime::Result<" << result_type << ">> " << client_name
            << "::" << method.name << "_async(";
        write_joined(out, method.parameters, parameter_decl, ", ");
        out << ") const\n";
        out << indent << "{\n";
        out << body_indent << "auto promise = std::make_shared<std::promise<hasten::runtime::Result<"
            << result_type << ">>>();\n";
        out << body_indent << "auto future = promise->get_future();\n";
        out << body_indent << method.name << "(";
        write_joined(out, method.parameters, parameter_name, ", ");
        if (!method.parameters.empty()) {
            out << ", ";
        }
        out << "[promise](hasten::runtime::Result<" << result_type << "> result) mutable {\n";
        out << nested_indent << "promise->set_value(std::move(result));\n";
        out << body_indent << "});\n";
        out << body_indent << "return future;\n";
        out << indent << "}\n\n";

        out << indent << "hasten::runtime::Result<" << result_type << "> " << client_name
            << "::" << method.name << "_sync(";
        write_joined(out, method.parameters, parameter_decl, ", ");
        out << ") const\n";
        out << indent << "{\n";
        out << body_indent << "auto future = " << method.name << "_async(";
        write_joined(out, method.parameters, parameter_name, ", ");
        out << ");\n";
        out << body_indent << "return future.get();\n";
        out << indent << "}\n\n";
    }

    out << indent << "std::shared_ptr<" << client_name << "> make_" << iface.name
        << "_client(std::shared_ptr<hasten::runtime::Channel> channel,\n"
        << indent
        << "                                          std::shared_ptr<hasten::runtime::Dispatcher> "
           "dispatcher)\n"
        << indent << "{\n"
        << indent << "    return std::make_shared<" << client_name << ">(std::move(channel), "
        << "std::move(dispatcher));\n"
        << indent << "}\n\n";

    close_namespaces(out, module);
    out << '\n';
    return out.str();
}

std::string generate_server_source(const ir::Module& module,
                                   const ir::Interface& iface,
                                   const TupleNameLookup& tuple_names,
                                   const GenerationOptions& options)
{
    (void)options;
    std::ostringstream out;
    TypeMapper mapper;

    out << "// Auto-generated server helpers for module " << module.name << ", interface " << iface.name
        << "\n";
    out << "#include \"" << module_base_name(module) << ".gen.hpp\"\n";
    out << "#include <utility>\n\n";

    open_namespaces(out, module);
    out << "\n";

    const int indent_level = static_cast<int>(module.namespace_parts.size());
    const auto indent = indentation(indent_level);
    const auto body_indent = indentation(indent_level + 1);
    const auto inner_indent = indentation(indent_level + 2);
    const auto case_indent = indentation(indent_level + 3);
    const auto case_body_indent = indentation(indent_level + 4);

    out << indent << "void bind_" << iface.name << "(std::shared_ptr<" << iface.name
        << "> implementation,\n"
        << indent << "             std::shared_ptr<hasten::runtime::Executor> executor)\n"
        << indent << "{\n";
    out << body_indent << "if (!implementation) {\n";
    out << body_indent << "    return;\n";
    out << body_indent << "}\n";
    out << body_indent << "if (!executor) {\n";
    out << body_indent << "    executor = hasten::runtime::make_default_executor();\n";
    out << body_indent << "}\n";
    out << body_indent << "hasten::runtime::rpc::register_handler(detail::" << iface.name
        << "Metadata::interface_id,\n";
    out << body_indent
        << "    [impl = std::move(implementation), exec = std::move(executor)](\n";
    out << body_indent
        << "        std::shared_ptr<hasten::runtime::rpc::Request> request,\n";
    out << body_indent
        << "        hasten::runtime::rpc::Responder responder) mutable {\n";
    out << body_indent << "        exec->schedule([impl, req = std::move(request), responder = std::move(responder)]() mutable {\n";
    out << body_indent << "            switch (req->method_id) {\n";

    for (const auto& method : iface.methods) {
        const auto helper_prefix = method_helper_prefix(iface.name, method.name);
        const auto decode_request_helper = "detail::decode_" + helper_prefix + "_request";
        const auto encode_response_helper = "detail::encode_" + helper_prefix + "_response";
        const auto result_type = method_result_type(method, mapper, tuple_names);
        const bool has_response = !method.result_fields.empty() || method.result_type.has_value();
        const auto method_meta_name = method.name + std::string("Metadata");
        const auto meta_prefix = "detail::" + iface.name + std::string("Metadata::") + method_meta_name + "::";

        out << case_indent << "case " << meta_prefix << "method_id: {\n";
        out << case_body_indent << "auto decoded = " << decode_request_helper << "(req->payload);\n";
        out << case_body_indent << "if (!decoded) {\n";
        out << case_body_indent
            << "    responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::InvalidRequest, {}});\n";
        out << case_body_indent << "    return;\n";
        out << case_body_indent << "}\n";
        out << case_body_indent << "auto request_data = std::move(*decoded);\n";
        out << case_body_indent << "auto result = impl->" << method.name << "(";
        for (std::size_t i = 0; i < method.parameters.size(); ++i) {
            if (i) {
                out << ", ";
            }
            out << "request_data." << method.parameters[i].name;
        }
        out << ");\n";
        out << case_body_indent << "if (!result) {\n";
        out << case_body_indent
            << "    responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::ApplicationError, {}});\n";
        out << case_body_indent << "    return;\n";
        out << case_body_indent << "}\n";
        if (has_response) {
            out << case_body_indent << "std::vector<std::uint8_t> response_body;\n";
            out << case_body_indent << "hasten::runtime::VectorSink response_sink{response_body};\n";
            out << case_body_indent << "hasten::runtime::hb1::Writer response_writer{response_sink};\n";
            out << case_body_indent << "auto response_value = std::move(*result);\n";
            out << case_body_indent << "if (auto encode_res = " << encode_response_helper
                << "(response_writer, response_value); !encode_res) {\n";
            out << case_body_indent
                << "    responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::InternalError, {}});\n";
            out << case_body_indent << "    return;\n";
            out << case_body_indent << "}\n";
            out << case_body_indent
                << "responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::Ok, std::move(response_body)});\n";
        } else {
            out << case_body_indent
                << "responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::Ok, {}});\n";
        }
        out << case_body_indent << "return;\n";
        out << case_indent << "}\n";
    }

    out << case_indent << "default: {\n";
    out << case_body_indent
        << "responder(hasten::runtime::rpc::Response{hasten::runtime::rpc::Status::NotFound, {}});\n";
    out << case_body_indent << "return;\n";
    out << case_indent << "}\n";

    out << body_indent << "            }\n";
    out << body_indent << "        });\n";
    out << body_indent << "    });\n";
    out << indent << "}\n\n";

    close_namespaces(out, module);
    out << '\n';
    return out.str();
}

}  // namespace

Emitter::Emitter(const GenerationOptions& options, const std::filesystem::path& root)
    : _options(options)
    , _root(root)
{
}

std::expected<Emitter::OutputFiles, std::string> Emitter::emit_module(const ir::Module& module) const
{
    namespace fs = std::filesystem;
    fs::path dir = _root;
    for (const auto& part : module.namespace_parts) {
        dir /= part;
    }

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return std::unexpected("Failed to create directory '" + dir.string() + "': " + ec.message());
    }

    const auto base = module_base_name(module);
    auto header_path = dir / (base + ".gen.hpp");

    auto tuple_info = build_tuple_info(module);

    auto result = write_file_if_changed(header_path, generate_header(module, tuple_info, _options));
    if (!result) {
        return std::unexpected(result.error());
    }

    OutputFiles output_files;
    output_files.header = header_path;
    output_files.include_dir = dir;
    output_files.module_base = base;

    for (const auto& iface : module.interfaces) {
        auto client_path = dir / fmt::format("{}_{}_client.gen.cpp", base, iface.name);
        auto server_path = dir / fmt::format("{}_{}_server.gen.cpp", base, iface.name);

        // client
        result = write_file_if_changed(client_path,
                                       generate_client_source(module, iface, tuple_info.lookup, _options));
        if (!result) {
            return std::unexpected(result.error());
        }

        // server
        result = write_file_if_changed(server_path, generate_server_source(module, iface, tuple_info.lookup, _options));
        if (!result) {
            return std::unexpected(result.error());
        }

        output_files.interfaces.push_back(InterfaceArtifacts{iface.name, client_path, server_path});
    }

    return output_files;
}

}  // namespace hasten::codegen
