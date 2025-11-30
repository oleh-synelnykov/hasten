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

struct Hb1FieldInfo {
    hasten::runtime::hb1::WireType wire_type = hasten::runtime::hb1::WireType::Varint;
    hasten::runtime::hb1::ValueKind value_kind = hasten::runtime::hb1::ValueKind::Unsigned;
    bool optional = false;
};

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
    os << "#include \"hasten/runtime/serialization/hb1.hpp\"\n";
    os << "#include \"hasten/runtime/uds.hpp\"\n";
    os << "\n";
    os << "#include <array>\n";
    os << "#include <cstdint>\n";
    os << "#include <expected>\n";
    os << "#include <functional>\n";
    os << "#include <future>\n";
    os << "#include <map>\n";
    os << "#include <memory>\n";
    os << "#include <optional>\n";
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

    os << class_indent << "void bind_" << iface.name << "(hasten::runtime::Dispatcher& dispatcher,\n"
       << class_indent << "             std::shared_ptr<" << iface.name << "> implementation,\n"
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

void generate_metadata(std::ostream& os, int indent_level, const ir::Module& module)
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
            << hash_literal(iface_full_name) << ";\n";
        os << '\n';
        for (const auto& method : iface.methods) {
            const auto method_meta_name = method.name + "Metadata";
            std::string method_full_name = iface_full_name + "." + method.name;
            os << indent << level << "struct " << method_meta_name << " {\n";
            os << indent << level << level << "static constexpr std::uint64_t method_id = "
                << hash_literal(method_full_name) << ";\n";
            os << emit_descriptor_array(indent + "        ", "static constexpr ",
                                         method_meta_name + "RequestFields", method.parameters);
            if (!method.result_fields.empty()) {
                os << emit_descriptor_array(indent + "        ", "static constexpr ",
                                             method_meta_name + "ResponseFields", method.result_fields);
            } else if (method.result_type) {
                auto single = single_field_vector(*method.result_type);
                os << emit_descriptor_array(indent + "        ", "static constexpr ",
                                             method_meta_name + "ResponseFields", single);
            } else {
                std::vector<ir::Field> none;
                os << emit_descriptor_array(indent + "        ", "static constexpr ",
                                             method_meta_name + "ResponseFields", none);
            }
            os << indent << "    };\n\n";
        }
        os << indent << "};\n\n";
    }

    os << indent << "}  // namespace detail\n\n";
}

std::string generate_header(const ir::Module& module, const TupleInfo& tuple_info,
                            const GenerationOptions& options)
{
    (void)options;
    std::ostringstream out;
    TypeMapper mapper;

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

    generate_metadata(out, indent_level, module);

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

        out << indent << "void " << client_name << "::" << method.name << "(";
        write_joined(out, method.parameters, parameter_decl, ", ");
        if (!method.parameters.empty()) {
            out << ", ";
        }
        out << callback_type << " callback) const\n";
        out << indent << "{\n";
        out << body_indent << "(void)channel_;\n";
        out << body_indent << "(void)dispatcher_;\n";
        for (const auto& param : method.parameters) {
            out << body_indent << "(void)" << param.name << ";\n";
        }
        out << body_indent << "if (callback) {\n";
        out << nested_indent << "callback(hasten::runtime::unimplemented_result<" << result_type
            << ">(\"Client stub not implemented\"));\n";
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

std::string generate_server_source(const ir::Module& module, const ir::Interface& iface,
                                   const GenerationOptions& options)
{
    (void)options;
    std::ostringstream out;

    out << "// Auto-generated server helpers for module " << module.name << ", interface " << iface.name
        << "\n";
    out << "#include \"" << module_base_name(module) << ".gen.hpp\"\n";
    out << "#include <utility>\n\n";

    open_namespaces(out, module);
    out << "\n";

    std::string indent(module.namespace_parts.size() * 4, ' ');
    out << indent << "void bind_" << iface.name << "(hasten::runtime::Dispatcher& dispatcher,\n"
        << indent << "             std::shared_ptr<" << iface.name << "> implementation,\n"
        << indent << "             std::shared_ptr<hasten::runtime::Executor> executor)\n"
        << indent << "{\n"
        << indent << indent << "(void)dispatcher;\n"
        << indent << indent << "(void)implementation;\n"
        << indent << indent << "(void)executor;\n"
        << indent << indent << "// TODO: Register interface with runtime dispatcher once transport is ready.\n"
        << indent << "}\n\n";

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
        result = write_file_if_changed(server_path, generate_server_source(module, iface, _options));
        if (!result) {
            return std::unexpected(result.error());
        }

        output_files.interfaces.push_back(InterfaceArtifacts{iface.name, client_path, server_path});
    }

    return output_files;
}

}  // namespace hasten::codegen
