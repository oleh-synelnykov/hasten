#include "idl/json_dump.hpp"

#include <nlohmann/json.hpp>

namespace hasten::idl::ast
{
namespace
{

using nlohmann::json;

std::string primitive_kind_to_string(PrimitiveKind kind)
{
    switch (kind) {
        case PrimitiveKind::Bool:
            return "bool";
        case PrimitiveKind::I8:
            return "i8";
        case PrimitiveKind::I16:
            return "i16";
        case PrimitiveKind::I32:
            return "i32";
        case PrimitiveKind::I64:
            return "i64";
        case PrimitiveKind::U8:
            return "u8";
        case PrimitiveKind::U16:
            return "u16";
        case PrimitiveKind::U32:
            return "u32";
        case PrimitiveKind::U64:
            return "u64";
        case PrimitiveKind::F32:
            return "f32";
        case PrimitiveKind::F64:
            return "f64";
        case PrimitiveKind::String:
            return "string";
        case PrimitiveKind::Bytes:
            return "bytes";
    }
    return "<unknown>";
}

std::string method_kind_to_string(MethodKind kind)
{
    switch (kind) {
        case MethodKind::Rpc:
            return "rpc";
        case MethodKind::Oneway:
            return "oneway";
        case MethodKind::Stream:
            return "stream";
        case MethodKind::Notify:
            return "notify";
    }
    return "<unknown>";
}

json bytes_to_json(const Bytes& bytes)
{
    json arr = json::array();
    for (auto byte : bytes) {
        arr.push_back(byte);
    }
    return arr;
}

json qualified_identifier_to_json(const QualifiedIdentifier& ident)
{
    return ident.to_string();
}

json constant_to_json(const ConstantValue& value)
{
    struct Visitor : boost::static_visitor<json> {
        Visitor() = default;

        json operator()(const Null&) const
        {
            return nullptr;
        }

        json operator()(bool v) const
        {
            return v;
        }

        json operator()(std::int64_t v) const
        {
            return v;
        }

        json operator()(double v) const
        {
            return v;
        }

        json operator()(const std::string& v) const
        {
            return v;
        }

        json operator()(const QualifiedIdentifier& ident) const
        {
            return qualified_identifier_to_json(ident);
        }

        json operator()(const Bytes& bytes) const
        {
            return bytes_to_json(bytes);
        }
    };

    return boost::apply_visitor(Visitor{}, value);
}

json attributes_to_json(const AttributeList& attrs)
{
    json arr = json::array();
    for (const auto& attr : attrs) {
        json entry;
        entry["name"] = attr.name;
        if (attr.value) {
            entry["value"] = constant_to_json(*attr.value);
        }
        arr.push_back(std::move(entry));
    }
    return arr;
}

json type_to_json(const Type& type);

json field_like_to_json(std::uint64_t id, const Type& type, const std::string& name,
                        const std::optional<ConstantValue>& default_value, const AttributeList& attrs)
{
    json node;
    node["id"] = id;
    node["name"] = name;
    node["type"] = type_to_json(type);
    if (default_value) {
        node["default"] = constant_to_json(*default_value);
    }
    node["attributes"] = attributes_to_json(attrs);
    return node;
}

json fields_to_json(const std::vector<Field>& fields)
{
    json arr = json::array();
    for (const auto& field : fields) {
        arr.push_back(field_like_to_json(field.id, field.type, field.name, field.default_value, field.attrs));
    }
    return arr;
}

json params_to_json(const std::vector<Parameter>& params)
{
    json arr = json::array();
    for (const auto& param : params) {
        arr.push_back(field_like_to_json(param.id, param.type, param.name, param.default_value, param.attrs));
    }
    return arr;
}

json result_to_json(const Result& result)
{
    struct Visitor : boost::static_visitor<json> {
        Visitor() = default;

        json operator()(const Type& type) const
        {
            return json{{"kind", "type"}, {"type", type_to_json(type)}};
        }

        json operator()(const std::vector<Field>& fields) const
        {
            return json{{"kind", "tuple"}, {"fields", fields_to_json(fields)}};
        }
    };

    return boost::apply_visitor(Visitor{}, result);
}

json type_to_json(const Type& type)
{
    struct Visitor : boost::static_visitor<json> {
        Visitor() = default;

        json operator()(const Primitive& primitive) const
        {
            return json{{"kind", "primitive"}, {"name", primitive_kind_to_string(primitive.kind)}};
        }

        json operator()(const UserType& user_type) const
        {
            return json{{"kind", "user"}, {"name", qualified_identifier_to_json(user_type.name)}};
        }

        json operator()(const Vector& vector_type) const
        {
            return json{{"kind", "vector"}, {"element", type_to_json(vector_type.element)}};
        }

        json operator()(const Map& map_type) const
        {
            return json{{"kind", "map"},
                        {"key", type_to_json(map_type.key)},
                        {"value", type_to_json(map_type.value)}};
        }

        json operator()(const Optional& optional_type) const
        {
            return json{{"kind", "optional"}, {"inner", type_to_json(optional_type.inner)}};
        }
    };

    return boost::apply_visitor(Visitor{}, type);
}

json enum_items_to_json(const std::vector<Enumerator>& items)
{
    json arr = json::array();
    for (const auto& item : items) {
        json entry;
        entry["name"] = item.name;
        if (item.value) {
            entry["value"] = *item.value;
        }
        entry["attributes"] = attributes_to_json(item.attrs);
        arr.push_back(std::move(entry));
    }
    return arr;
}

json declaration_to_json(const Declaration& decl)
{
    struct Visitor : boost::static_visitor<json> {
        Visitor() = default;

        json operator()(const ConstantDeclaration& constant) const
        {
            return json{{"kind", "const"},
                        {"name", constant.name},
                        {"type", type_to_json(constant.type)},
                        {"value", constant_to_json(constant.value)}};
        }

        json operator()(const Enum& e) const
        {
            return json{{"kind", "enum"}, {"name", e.name}, {"items", enum_items_to_json(e.items)}};
        }

        json operator()(const Struct& s) const
        {
            return json{{"kind", "struct"}, {"name", s.name}, {"fields", fields_to_json(s.fields)}};
        }

        json operator()(const Interface& interface_node) const
        {
            json node{{"kind", "interface"}, {"name", interface_node.name}};
            json methods = json::array();
            for (const auto& method : interface_node.methods) {
                json entry;
                entry["name"] = method.name;
                entry["kind"] = method_kind_to_string(method.kind);
                entry["parameters"] = params_to_json(method.params);
                if (method.result) {
                    entry["result"] = result_to_json(*method.result);
                }
                entry["attributes"] = attributes_to_json(method.attrs);
                methods.push_back(std::move(entry));
            }
            node["attributes"] = attributes_to_json(interface_node.attrs);
            node["methods"] = std::move(methods);
            return node;
        }
    };

    return boost::apply_visitor(Visitor{}, decl);
}

json imports_to_json(const std::vector<Import>& imports)
{
    json arr = json::array();
    for (const auto& import : imports) {
        arr.push_back(json{{"path", import.path}});
    }
    return arr;
}

}  // namespace

nlohmann::json to_json(const Module& module)
{
    json node;
    node["kind"] = "module";
    node["name"] = qualified_identifier_to_json(module.name);
    node["imports"] = imports_to_json(module.imports);
    json decls = json::array();
    for (const auto& decl : module.decls) {
        decls.push_back(declaration_to_json(decl));
    }
    node["declarations"] = std::move(decls);
    return node;
}

}  // namespace hasten::idl::ast
