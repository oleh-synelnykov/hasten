#include "validator_impl.hpp"

#include <limits>

namespace hasten::frontend
{

namespace
{
static constexpr std::uint64_t kMaxId = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());

std::string qualified_name(const std::string& module_name, const std::string& decl_name)
{
    if (module_name.empty()) {
        return decl_name;
    }
    return module_name + "." + decl_name;
}
}  // namespace

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
            report_error(file, "Module '" + module_name + "' already defined in " + it->second->path);
        }
        for (const auto& decl : module.decls) {
            if (const auto* s = boost::get<ast::Struct>(&decl)) {
                register_declaration(module_name, s->name, file, DeclKind::Struct);
            } else if (const auto* e = boost::get<ast::Enum>(&decl)) {
                register_declaration(module_name, e->name, file, DeclKind::Enum);
            } else if (const auto* i = boost::get<ast::Interface>(&decl)) {
                register_declaration(module_name, i->name, file, DeclKind::Interface);
            }
        }
    }
}

void ValidateSemanticsImpl::register_declaration(const std::string& module_name, const std::string& name,
                                         const SourceFile& file, DeclKind kind)
{
    std::string fq = qualified_name(module_name, name);
    auto [it, inserted] = _declarations.emplace(fq, DeclInfo{kind, &file});
    if (!inserted) {
        report_error(file, "Declaration '" + fq + "' already defined in " + it->second.file->path);
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
        validate_type(field.type, file, module_name, "field '" + field.name + "' of struct '" + s.name + "'");
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
            validate_type(param.type, file, module_name,
                          "parameter '" + param.name + "' of method '" + method.name + "'");
        }
        if (method.result) {
            if (const auto* result_fields = boost::get<std::vector<ast::Field>>(&*method.result)) {
                check_unique_names(*result_fields, file, "method '" + method.name + "' result", "field");
                check_id_collection(*result_fields, file, "method '" + method.name + "'", "result field");
                for (const auto& field : *result_fields) {
                    validate_type(field.type, file, module_name,
                                  "result field '" + field.name + "' of method '" + method.name + "'");
                }
            } else if (const auto* result_type = boost::get<ast::Type>(&*method.result)) {
                validate_type(*result_type, file, module_name, "result of method '" + method.name + "'");
            }
        }
    }
}

void ValidateSemanticsImpl::validate_type(const ast::Type& type, const SourceFile& file,
                                  const std::string& module_name, const std::string& usage)
{
    struct Visitor : boost::static_visitor<void> {
        Visitor(ValidateSemanticsImpl& self, const SourceFile& file, const std::string& module_name,
                const std::string& usage)
            : self(self)
            , file(file)
            , module_name(module_name)
            , usage(usage)
        {
        }

        ValidateSemanticsImpl& self;
        const SourceFile& file;
        const std::string& module_name;
        const std::string& usage;

        void operator()(const ast::Primitive&) const {}

        void operator()(const ast::UserType& user) const
        {
            self.resolve_user_type(user, module_name, file, usage);
        }

        void operator()(const ast::Vector& vec) const
        {
            self.validate_type(vec.element, file, module_name, usage + " (vector element)");
        }

        void operator()(const ast::Map& map) const
        {
            self.validate_map_key(map.key, file, module_name, usage);
            self.validate_type(map.value, file, module_name, usage + " (map value)");
        }

        void operator()(const ast::Optional& opt) const
        {
            if (const auto* inner_opt = boost::get<ast::Optional>(&opt.inner)) {
                self.report_error(file, "Nested optional types are not allowed in " + usage);
                // continue validating inner to surface additional issues
                self.validate_type(*inner_opt, file, module_name, usage + " (inner optional)");
            } else {
                self.validate_type(opt.inner, file, module_name, usage + " (optional)");
            }
        }
    };

    boost::apply_visitor(Visitor(*this, file, module_name, usage), type);
}

void ValidateSemanticsImpl::validate_map_key(const ast::Type& key, const SourceFile& file,
                                     const std::string& module_name, const std::string& usage)
{
    if (const auto* primitive = boost::get<ast::Primitive>(&key)) {
        (void)primitive;
        return;
    }
    if (const auto* user = boost::get<ast::UserType>(&key)) {
        const auto* info = resolve_user_type(*user, module_name, file, usage + " (map key)");
        if (info && info->kind != DeclKind::Enum) {
            report_error(file, "Map key in " + usage + " must be a primitive or enum type");
        }
        return;
    }
    report_error(file, "Map key in " + usage + " must be a primitive or enum type");
}

const ValidateSemanticsImpl::DeclInfo* ValidateSemanticsImpl::resolve_user_type(const ast::UserType& user_type,
                                                                const std::string& module_name,
                                                                const SourceFile& file,
                                                                const std::string& usage)
{
    std::string name = user_type.name.to_string();
    auto it = _declarations.find(name);
    if (it == _declarations.end() && user_type.name.parts.size() == 1) {
        std::string fq = qualified_name(module_name, name);
        it = _declarations.find(fq);
    }
    if (it == _declarations.end()) {
        report_error(file, "Unknown type '" + name + "' referenced in " + usage);
        return nullptr;
    }
    return &it->second;
}

void ValidateSemanticsImpl::report_error(const SourceFile& file, const std::string& message)
{
    report(Severity::Error, file, message);
}

void ValidateSemanticsImpl::report_note(const SourceFile& file, const std::string& message)
{
    report(Severity::Note, file, message);
}

void ValidateSemanticsImpl::report(Severity severity, const SourceFile& file, const std::string& message)
{
    _sink.report(severity, SourceLocation{file.path, 0, 0}, message);
}

void ValidateSemanticsImpl::check_id_bounds(std::uint64_t id, const SourceFile& file, const std::string& element_kind,
                                    const std::string& owner_label)
{
    if (id == 0) {
        report_error(file,
                     "Invalid " + element_kind + " id '0' in " + owner_label + "; ids must start at 1");
        return;
    }
    if (id > kMaxId) {
        report_error(file, "Invalid " + element_kind + " id '" + std::to_string(id) + "' in " + owner_label +
                               "; maximum allowed value is " + std::to_string(kMaxId));
    }
}

}  // namespace hasten::frontend
