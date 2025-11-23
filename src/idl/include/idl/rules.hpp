#pragma once

#include "ast.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/support/unused.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

namespace rule
{

// comments and skipper
using LineComment = x3::rule<struct LineCommentRuleClass, std::string>;
using BlockComment = x3::rule<struct BlockCommentRuleClass, std::string>;
using Comment = x3::rule<struct CommentRuleClass, std::string>;
using Skipper = x3::rule<struct SkipperRuleClass, x3::unused_type const>;

// tokens
using Identifier = x3::rule<struct IdentifierRuleClass, std::string>;
using Name = x3::rule<struct NameRuleClass, std::string>;
using QualifiedIdentifier = x3::rule<struct QualifiedIdentifierRuleClass, ast::QualifiedIdentifier>;
using StringLiteral = x3::rule<struct StringLiteralRuleClass, std::string>;
using BooleanLiteral = x3::rule<struct BooleanLiteralRuleClass, bool>;
using IntegerLiteral = x3::rule<struct IntegerLiteralRuleClass, std::int64_t>;
using FloatLiteral = x3::rule<struct FloatLiteralRuleClass, double>;
using BytesLiteral = x3::rule<struct BytesLiteralRuleClass, ast::Bytes>;
using ConstantValue = x3::rule<struct ConstantValueRuleClass, ast::ConstantValue>;

// types
using Type = x3::rule<struct TypeRuleClass, ast::Type>;
using PrimitiveType = x3::rule<struct PrimitiveTypeRuleClass, ast::Primitive>;
using UserType = x3::rule<struct UserTypeRuleClass, ast::UserType>;
using VectorType = x3::rule<struct VectorTypeRuleClass, ast::Vector>;
using MapType = x3::rule<struct MapTypeRuleClass, ast::Map>;
using OptionalType = x3::rule<struct OptionalTypeRuleClass, ast::Optional>;

// attributes
using Attribute = x3::rule<struct AttributeRuleClass, ast::Attribute>;
using AttributeList = x3::rule<struct AttributeListRuleClass, std::vector<ast::Attribute>>;

// fields / params / results
using Field = x3::rule<struct FieldRuleClass, ast::Field>;
using Parameter = x3::rule<struct ParameterRuleClass, ast::Parameter>;
using Result = x3::rule<struct ResultRuleClass, ast::Result>;
using ReturnField = x3::rule<struct ReturnFieldRuleClass, ast::Field>;
using ReturnFields = x3::rule<struct ReturnFieldsRuleClass, std::vector<ast::Field>>;

// declarations
using Constant = x3::rule<struct ConstantRuleClass, ast::ConstantDeclaration>;
using EnumItem = x3::rule<struct EnumItemRuleClass, ast::Enumerator>;
using Enum = x3::rule<struct EnumRuleClass, ast::Enum>;
using Struct = x3::rule<struct StructRuleClass, ast::Struct>;
using Method = x3::rule<struct MethodRuleClass, ast::Method>;
using MethodKind = x3::rule<struct MethodKindRuleClass, ast::MethodKind>;
using Interface = x3::rule<struct InterfaceRuleClass, ast::Interface>;
using Declaration = x3::rule<struct DeclarationRuleClass, ast::Declaration>;

// imports / module / file
using Import = x3::rule<struct ImportRuleClass, ast::Import>;
using Module = x3::rule<struct ModuleRuleClass, ast::Module>;

using ModuleDeclaration = x3::rule<struct ModuleDeclarationRuleClass, ast::QualifiedIdentifier>;

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
