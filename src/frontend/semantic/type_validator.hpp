#pragma once

#include "semantic_context.hpp"
#include "idl/ast.hpp"

namespace hasten::frontend::semantic {

namespace ast = idl::ast;

class TypeValidator
{
public:
    explicit TypeValidator(Context& context)
        : context_(context)
    {
    }

    void validate(const ast::Type& type, const SourceFile& file, const ast::PositionTaggedNode& anchor,
                  const std::string& module_name, const std::string& usage);

private:
    void validate_map_key(const ast::Type& key, const SourceFile& file, const ast::PositionTaggedNode& anchor,
                          const std::string& module_name, const std::string& usage);

    Context& context_;
};


}  // namespace hasten::frontend::semantic