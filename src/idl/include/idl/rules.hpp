#pragma once

#include "ast.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/unused.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

namespace rule
{

// position-tagged wrappers (source locations)
template <typename T>
struct with_pos : T, x3::position_tagged {
    using T::T;
    with_pos() = default;
};

// Expose AST aliases with positions for top nodes where it helps debugging
using pModule = with_pos<ast::Module>;

// comments and skipper
using LineComment = x3::rule<class r_line_comment, std::string>;
using BlockComment = x3::rule<class r_block_comment, std::string>;
using Comment = x3::rule<class r_comment, std::string>;
using Skipper = x3::rule<class r_skipper, x3::unused_type const>;

// tokens
using Identifier = x3::rule<class r_ident, std::string>;
using Name = x3::rule<class r_name, std::string>;
using QualifiedIdentifier = x3::rule<class r_qident, ast::QualifiedIdentifier>;
using StringLiteral = x3::rule<class r_string, std::string>;
using BooleanLiteral = x3::rule<class r_bool, bool>;
using IntegerLiteral = x3::rule<class r_int64, std::int64_t>;
using FloatLiteral = x3::rule<class r_float, double>;
using BytesLiteral = x3::rule<class r_bytes, ast::Bytes>;
using ConstantValue = x3::rule<class r_const, ast::ConstantValue>;

// types
using Type = x3::rule<class r_type, ast::Type>;
using PrimitiveType = x3::rule<class r_prim, ast::Primitive>;
using UserType = x3::rule<class r_user, ast::UserType>;
using VectorType = x3::rule<class r_vec, ast::Vector>;
using MapType = x3::rule<class r_map, ast::Map>;
using OptionalType = x3::rule<class r_opt, ast::Optional>;

// attributes
using Attribute = x3::rule<class r_attribute, ast::Attribute>;
using AttributeList = x3::rule<class r_attribute_list, std::vector<ast::Attribute>>;

// fields / params / results
using Field = x3::rule<class r_field, ast::Field>;
using Parameter = x3::rule<class r_param, ast::Parameter>;
using Result = x3::rule<class r_ret, ast::Result>;
using ReturnField = x3::rule<class r_ret_field, ast::Field>;
using ReturnFields = x3::rule<class r_ret_fields, std::vector<ast::Field>>;

// declarations
using Constant = x3::rule<class r_const_decl, ast::ConstantDeclaration>;
using EnumItem = x3::rule<class r_enum_item, ast::Enumerator>;
using Enum = x3::rule<class r_enum_decl, ast::Enum>;
using Struct = x3::rule<class r_struct_decl, ast::Struct>;
using Method = x3::rule<class r_method, ast::Method>;
using MethodKind = x3::rule<class r_method_kind, ast::MethodKind>;
using Interface = x3::rule<class r_iface_decl, ast::Interface>;
using Declaration = x3::rule<class r_decl, ast::Declaration>;

// imports / module / file
using Import = x3::rule<class r_import, ast::Import>;
using Module = x3::rule<class r_module, ast::Module>;

using ModuleDeclaration = x3::rule<class r_module_decl, ast::QualifiedIdentifier>;

// clang-format off

// grammar hookup
BOOST_SPIRIT_DECLARE(
    LineComment,
    BlockComment,
    Comment,
    Skipper,
    Identifier,
    Name,
    QualifiedIdentifier,
    StringLiteral,
    BooleanLiteral,
    IntegerLiteral,
    FloatLiteral,
    BytesLiteral,
    ConstantValue,
    PrimitiveType,
    UserType,
    VectorType,
    MapType,
    OptionalType,
    Type,
    Attribute,
    AttributeList,
    Field,
    Parameter,
    Result,
    ReturnField,
    ReturnFields,
    Constant,
    EnumItem,
    Enum,
    Struct,
    Method,
    MethodKind,
    Interface,
    Declaration,
    Import,
    Module,
    ModuleDeclaration
)

// clang-format on
}  // namespace rule

rule::LineComment line_comment();
rule::BlockComment block_comment();
rule::Comment comment();
rule::Skipper skipper();
rule::Identifier identifier();
rule::Name name();
rule::QualifiedIdentifier qualified_identifier();
rule::StringLiteral string_literal();
rule::BooleanLiteral boolean_literal();
rule::IntegerLiteral integer_literal();
rule::FloatLiteral float_literal();
rule::BytesLiteral bytes_literal();
rule::ConstantValue const_value();
rule::PrimitiveType primitive_type();
rule::UserType user_type();
rule::VectorType vector_type();
rule::MapType map_type();
rule::OptionalType optional_type();
rule::Type type();
rule::Attribute attribute();
rule::AttributeList attribute_list();
rule::Field field();
rule::Parameter param();
rule::Result result();
rule::ReturnField ret_field();
rule::ReturnFields ret_fields();
rule::Constant const_decl();
rule::EnumItem enum_item();
rule::Enum enum_decl();
rule::Struct struct_decl();
rule::Method method();
rule::MethodKind method_kind();
rule::Interface interface_decl();
rule::Declaration declaration();
rule::Import import();
rule::Module module();
rule::ModuleDeclaration module_decl();

}  // namespace hasten::idl::parser
