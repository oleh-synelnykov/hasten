#include "validator_impl.hpp"

namespace hasten::frontend
{

void ValidateSemanticsImpl::run()
{
    build_indices();
    for (const auto& [_, file] : _program.files) {
        validate_file(file);
    }
}

void ValidateSemanticsImpl::build_indices()
{
    _module_index.clear();
    _declarations.clear();
    for (const auto& [_, file] : _program.files) {
        const auto& module = file.module;
        std::string module_name = module.name.to_string();
        auto [it, inserted] = _module_index.emplace(module_name, &file);
        if (!inserted) {
            report_error(file, module, "Module '" + module_name + "' already defined in " + it->second->path);
        }
        for (const auto& decl : module.decls) {
            if (const auto* s = boost::get<ast::Struct>(&decl)) {
                register_declaration(module_name, file, *s, DeclKind::Struct);
            } else if (const auto* e = boost::get<ast::Enum>(&decl)) {
                register_declaration(module_name, file, *e, DeclKind::Enum);
            } else if (const auto* i = boost::get<ast::Interface>(&decl)) {
                register_declaration(module_name, file, *i, DeclKind::Interface);
            }
        }
    }
}

void ValidateSemanticsImpl::validate_file(const SourceFile& file)
{
    const auto& module = file.module;
    std::string module_name = module.name.to_string();
    for (const auto& decl : module.decls) {
        if (const auto* s = boost::get<ast::Struct>(&decl)) {
            validate_struct(*s, file, module_name);
        } else if (const auto* e = boost::get<ast::Enum>(&decl)) {
            validate_enum(*e, file, module_name);
        } else if (const auto* i = boost::get<ast::Interface>(&decl)) {
            validate_interface(*i, file, module_name);
        }
    }
}

void ValidateSemanticsImpl::validate_struct(const ast::Struct& s, const SourceFile& file,
                                            const std::string& module_name)
{
    check_unique_names(s.fields, file, "struct '" + s.name + "'", "field");
    check_id_collection(s.fields, file, "struct '" + s.name + "'", "field");
    for (const auto& field : s.fields) {
        validate_type(field.type, file, field, module_name,
                      "field '" + field.name + "' of struct '" + s.name + "'");
    }
}

void ValidateSemanticsImpl::validate_enum(const ast::Enum& e, const SourceFile& file, const std::string&)
{
    check_unique_names(e.items, file, "enum '" + e.name + "'", "enumerator");
}

void ValidateSemanticsImpl::validate_interface(const ast::Interface& iface, const SourceFile& file,
                                               const std::string& module_name)
{
    check_unique_names(iface.methods, file, "interface '" + iface.name + "'", "method");
    for (const auto& method : iface.methods) {
        check_unique_names(method.params, file, "method '" + method.name + "'", "parameter");
        check_id_collection(method.params, file, "method '" + method.name + "'", "parameter");
        for (const auto& param : method.params) {
            validate_type(param.type, file, param, module_name,
                          "parameter '" + param.name + "' of method '" + method.name + "'");
        }
        if (method.result) {
            if (const auto* result_fields = boost::get<std::vector<ast::Field>>(&*method.result)) {
                check_unique_names(*result_fields, file, "method '" + method.name + "' result", "field");
                check_id_collection(*result_fields, file, "method '" + method.name + "'", "result field");
                for (const auto& field : *result_fields) {
                    validate_type(field.type, file, field, module_name,
                                  "result field '" + field.name + "' of method '" + method.name + "'");
                }
            } else if (const auto* result_type = boost::get<ast::Type>(&*method.result)) {
                validate_type(*result_type, file, method, module_name,
                              "result of method '" + method.name + "'");
            }
        }
    }
}

void ValidateSemanticsImpl::validate_type(const ast::Type& type, const SourceFile& file,
                                          const ast::PositionTaggedNode& anchor,
                                          const std::string& module_name, const std::string& usage)
{
    struct Visitor : boost::static_visitor<void> {
        Visitor(ValidateSemanticsImpl& self, const SourceFile& file, const std::string& module_name,
                const std::string& usage, const ast::PositionTaggedNode& anchor)
            : self(self)
            , file(file)
            , anchor(anchor)
            , module_name(module_name)
            , usage(usage)
        {
        }

        ValidateSemanticsImpl& self;
        const SourceFile& file;
        const ast::PositionTaggedNode& anchor;
        const std::string& module_name;
        const std::string& usage;

        void operator()(const ast::Primitive&) const {}

        void operator()(const ast::UserType& user) const
        {
            self.resolve_user_type(user, module_name, file, usage);
        }

        void operator()(const ast::Vector& vec) const
        {
            self.validate_type(vec.element, file, anchor, module_name, usage + " (vector element)");
        }

        void operator()(const ast::Map& map) const
        {
            self.validate_map_key(map.key, file, anchor, module_name, usage);
            self.validate_type(map.value, file, anchor, module_name, usage + " (map value)");
        }

        void operator()(const ast::Optional& opt) const
        {
            if (const auto* inner_opt = boost::get<ast::Optional>(&opt.inner)) {
                self.report_error(file, anchor, "Nested optional types are not allowed in " + usage);
                // continue validating inner to surface additional issues
                self.validate_type(*inner_opt, file, anchor, module_name, usage + " (inner optional)");
            } else {
                self.validate_type(opt.inner, file, anchor, module_name, usage + " (optional)");
            }
        }
    };

    boost::apply_visitor(Visitor(*this, file, module_name, usage, anchor), type);
}

void ValidateSemanticsImpl::validate_map_key(const ast::Type& key, const SourceFile& file,
                                             const ast::PositionTaggedNode& anchor,
                                             const std::string& module_name, const std::string& usage)
{
    if (const auto* primitive = boost::get<ast::Primitive>(&key)) {
        (void)primitive;
        return;
    }
    if (const auto* user = boost::get<ast::UserType>(&key)) {
        const auto* info = resolve_user_type(*user, module_name, file, usage + " (map key)");
        if (info && info->kind != DeclKind::Enum) {
            report_error(file, anchor, "Map key in " + usage + " must be a primitive or enum type");
        }
        return;
    }
    report_error(file, anchor, "Map key in " + usage + " must be a primitive or enum type");
}

const ValidateSemanticsImpl::DeclInfo* ValidateSemanticsImpl::resolve_user_type(
    const ast::UserType& user_type, const std::string& module_name, const SourceFile& file,
    const std::string& usage)
{
    std::string name = user_type.name.to_string();
    auto it = _declarations.find(name);
    if (it == _declarations.end() && user_type.name.parts.size() == 1) {
        std::string fq = qualified_name(module_name, name);
        it = _declarations.find(fq);
    }
    if (it == _declarations.end()) {
        report_error(file, user_type, "Unknown type '" + name + "' referenced in " + usage);
        return nullptr;
    }
    return &it->second;
}

void ValidateSemanticsImpl::report_error(const SourceFile& file, const ast::PositionTaggedNode& node,
                                         const std::string& message)
{
    report(Severity::Error, file, node, message);
}

void ValidateSemanticsImpl::report_note(const SourceFile& file, const ast::PositionTaggedNode& node,
                                        const std::string& message)
{
    report(Severity::Note, file, node, message);
}

void ValidateSemanticsImpl::report(Severity severity, const SourceFile& file,
                                   const ast::PositionTaggedNode& node, const std::string& message)
{
    _sink.report(severity, locate(node, file), message);
}

std::string ValidateSemanticsImpl::qualified_name(const std::string& module_name,
                                                  const std::string& decl_name)
{
    if (module_name.empty()) {
        return decl_name;
    }
    return module_name + "." + decl_name;
}

}  // namespace hasten::frontend
