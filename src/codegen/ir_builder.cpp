#include "ir_builder.hpp"

#include <algorithm>
#include <unordered_map>

namespace hasten::codegen::ir
{
namespace
{

std::vector<std::string> split_module_name(const std::string& name)
{
    std::vector<std::string> parts;
    std::string current;
    for (char ch : name) {
        if (ch == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    if (parts.empty() && !name.empty()) {
        parts.push_back(name);
    }
    return parts;
}

Attribute make_attribute(const ast::Attribute& attr)
{
    Attribute copy;
    copy.name = attr.name;
    copy.value = attr.value;
    return copy;
}

std::vector<Attribute> copy_attributes(const ast::AttributeList& attrs)
{
    std::vector<Attribute> out;
    out.reserve(attrs.size());
    for (const auto& attr : attrs) {
        out.push_back(make_attribute(attr));
    }
    return out;
}

Field make_field(const ast::Field& field)
{
    Field ir;
    ir.id = field.id;
    ir.name = field.name;
    ir.type = field.type;
    ir.default_value = field.default_value;
    ir.attributes = copy_attributes(field.attrs);
    return ir;
}

Field make_parameter(const ast::Parameter& param)
{
    Field ir;
    ir.id = param.id;
    ir.name = param.name;
    ir.type = param.type;
    ir.default_value = param.default_value;
    ir.attributes = copy_attributes(param.attrs);
    return ir;
}

Struct make_struct(const ast::Struct& node)
{
    Struct ir;
    ir.name = node.name;
    ir.attributes = {};  // structs currently do not carry attributes in AST
    ir.fields.reserve(node.fields.size());
    for (const auto& field : node.fields) {
        ir.fields.push_back(make_field(field));
    }
    return ir;
}

Enum make_enum(const ast::Enum& node)
{
    Enum ir;
    ir.name = node.name;
    ir.attributes = {};  // enums currently do not carry attributes in AST
    ir.values.reserve(node.items.size());
    for (const auto& item : node.items) {
        Enumerator enumerator;
        enumerator.name = item.name;
        enumerator.value = item.value;
        enumerator.attributes = copy_attributes(item.attrs);
        ir.values.push_back(std::move(enumerator));
    }
    return ir;
}

Method make_method(const ast::Method& method)
{
    Method ir;
    ir.name = method.name;
    ir.kind = method.kind;
    ir.attributes = copy_attributes(method.attrs);
    ir.parameters.reserve(method.params.size());
    for (const auto& param : method.params) {
        ir.parameters.push_back(make_parameter(param));
    }

    if (method.result) {
        if (const auto* fields = boost::get<std::vector<ast::Field>>(&*method.result)) {
            ir.result_fields.reserve(fields->size());
            for (const auto& field : *fields) {
                ir.result_fields.push_back(make_field(field));
            }
        } else if (const auto* type = boost::get<ast::Type>(&*method.result)) {
            ir.result_type = *type;
        }
    }

    return ir;
}

Interface make_interface(const ast::Interface& iface)
{
    Interface ir;
    ir.name = iface.name;
    ir.attributes = copy_attributes(iface.attrs);
    ir.methods.reserve(iface.methods.size());
    for (const auto& method : iface.methods) {
        ir.methods.push_back(make_method(method));
    }
    return ir;
}

}  // namespace

CompilationUnit build_internal_representation(const frontend::Program& program)
{
    CompilationUnit unit;

    std::unordered_map<std::string, Module> modules;

    for (const auto& [_, file] : program.files) {
        const auto& module = file.module;
        std::string module_name = module.name.to_string();
        auto [it, inserted] = modules.emplace(module_name, Module{});
        Module& ir_module = it->second;
        if (inserted) {
            ir_module.name = module_name;
            ir_module.namespace_parts = split_module_name(module_name);
        }

        for (const auto& decl : module.decls) {
            if (const auto* s = boost::get<ast::Struct>(&decl)) {
                ir_module.structs.push_back(make_struct(*s));
            } else if (const auto* e = boost::get<ast::Enum>(&decl)) {
                ir_module.enums.push_back(make_enum(*e));
            } else if (const auto* iface = boost::get<ast::Interface>(&decl)) {
                ir_module.interfaces.push_back(make_interface(*iface));
            }
        }
    }

    std::vector<std::string> module_names;
    module_names.reserve(modules.size());
    for (const auto& [name, _] : modules) {
        module_names.push_back(name);
    }
    std::sort(module_names.begin(), module_names.end());

    for (const auto& name : module_names) {
        unit.modules.push_back(std::move(modules[name]));
    }

    return unit;
}

}  // namespace hasten::codegen::ir
