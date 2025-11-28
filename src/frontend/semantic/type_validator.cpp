#include "type_validator.hpp"

namespace hasten::frontend::semantic {

void TypeValidator::validate(const ast::Type& type, const SourceFile& file, const ast::PositionTaggedNode& anchor,
                             const std::string& module_name, const std::string& usage)
{
    struct Visitor : boost::static_visitor<void> {
        Visitor(TypeValidator& self, Context& context, const SourceFile& file,
                const ast::PositionTaggedNode& anchor, const std::string& module_name, const std::string& usage)
            : self(self)
            , context(context)
            , file(file)
            , anchor(anchor)
            , module(module_name)
            , usage(usage)
        {
        }

        TypeValidator& self;
        Context& context;
        const SourceFile& file;
        const ast::PositionTaggedNode& anchor;
        const std::string& module;
        const std::string& usage;

        void operator()(const ast::Primitive&) const {}

        void operator()(const ast::UserType& user) const
        {
            (void)context.resolve_user_type(user, module, file, usage);
        }

        void operator()(const ast::Vector& vec) const
        {
            self.validate(vec.element, file, anchor, module, usage + " (vector element)");
        }

        void operator()(const ast::Map& map) const
        {
            self.validate_map_key(map.key, file, anchor, module, usage);
            self.validate(map.value, file, anchor, module, usage + " (map value)");
        }

        void operator()(const ast::Optional& opt) const
        {
            if (const auto* inner_opt = boost::get<ast::Optional>(&opt.inner)) {
                context.report_error(file, anchor, "Nested optional types are not allowed in " + usage);
                self.validate(*inner_opt, file, anchor, module, usage + " (inner optional)");
            } else {
                self.validate(opt.inner, file, anchor, module, usage + " (optional)");
            }
        }
    };

    boost::apply_visitor(Visitor(*this, context_, file, anchor, module_name, usage), type);
}

void TypeValidator::validate_map_key(const ast::Type& key, const SourceFile& file,
                                     const ast::PositionTaggedNode& anchor, const std::string& module_name,
                                     const std::string& usage)
{
    if (const auto* primitive = boost::get<ast::Primitive>(&key)) {
        (void)primitive;
        return;
    }

    if (const auto* user = boost::get<ast::UserType>(&key)) {
        const auto* info = context_.resolve_user_type(*user, module_name, file, usage + " (map key)");
        if (info && info->kind != DeclKind::Enum) {
            context_.report_error(file, anchor, "Map key in " + usage + " must be a primitive or enum type");
        }
        return;
    }

    context_.report_error(file, anchor, "Map key in " + usage + " must be a primitive or enum type");
}

}  // namespace hasten::frontend::semantic
