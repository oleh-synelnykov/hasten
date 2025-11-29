#include "emitter.hpp"

#include "file_writer.hpp"
#include "ir.hpp"
#include "ostream_joiner.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <fmt/format.h>

namespace hasten::codegen
{
namespace
{

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

namespace ast = idl::ast;

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

std::string method_result_type(const ir::Method& method, const TypeMapper& mapper,
                               const TupleNameLookup& tuple_names)
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
    os << "#include \"hasten/runtime/uds.hpp\"\n";
    os << "\n";
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
