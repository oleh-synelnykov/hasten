#pragma once

#include "ast.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/unused.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

namespace impl {

// position-tagged wrappers (source locations)
template <typename T>
struct with_pos : T, x3::position_tagged {
    using T::T;
    with_pos() = default;
};

// Expose AST aliases with positions for top nodes where it helps debugging
using pModule = with_pos<ast::Module>;

// clang-format off

// comments and skipper
using LineCommentRuleType = x3::rule<class r_line_comment, std::string>;
using BlockCommentRuleType = x3::rule<class r_block_comment, std::string>;
using CommentRuleType = x3::rule<class r_comment, std::string>;
using SkipperRuleType = x3::rule<class r_skipper, x3::unused_type const>;

// tokens
using IdentifierRuleType = x3::rule<class r_ident, std::string>;
using NameRuleType = x3::rule<class r_name, std::string>;
using QualifiedIdentifierRuleType = x3::rule<class r_qident, ast::QualifiedIdentifier>;
using StringLiteralRuleType = x3::rule<class r_string, std::string>;
using BooleanLiteralRuleType = x3::rule<class r_bool, bool>;
using IntegerLiteralRuleType = x3::rule<class r_int64, std::int64_t>;
using FloatLiteralRuleType = x3::rule<class r_float, double>;
using BytesLiteralRuleType = x3::rule<class r_bytes, ast::Bytes>;
using ConstValueRuleType = x3::rule<class r_const, ast::ConstantValue>;

// types
using TypeRuleType = x3::rule<class r_type, ast::Type>;
using PrimitiveTypeRuleType = x3::rule<class r_prim, ast::Primitive>;
using UserTypeRuleType = x3::rule<class r_user, ast::UserType>;
using VectorTypeRuleType = x3::rule<class r_vec, ast::Vector>;
using MapTypeRuleType = x3::rule<class r_map, ast::Map>;
using OptionalTypeRuleType = x3::rule<class r_opt, ast::Optional>;

// attributes
using AttributeRuleType = x3::rule<class r_attribute, ast::Attribute>;
using AttributeListRuleType = x3::rule<class r_attribute_list, std::vector<ast::Attribute>>;

// fields / params / results
using FieldRuleType = x3::rule<class r_field, ast::Field>;
using ParamRuleType = x3::rule<class r_param, ast::Param>;
using ResultRuleType = x3::rule<class r_ret, ast::Result>;
using RetFieldRuleType = x3::rule<class r_ret_field, ast::Field>;
using RetFieldsRuleType = x3::rule<class r_ret_fields, std::vector<ast::Field>>;

// declarations
using ConstDeclRuleType = x3::rule<class r_const_decl, ast::ConstantDeclaration>;
using EnumItemRuleType = x3::rule<class r_enum_item, ast::Enumerator>;
using EnumDeclRuleType = x3::rule<class r_enum_decl, ast::Enum>;
using StructDeclRuleType = x3::rule<class r_struct_decl, ast::Struct>;
using MethodRuleType = x3::rule<class r_method, ast::Method>;
using MethodKindRuleType = x3::rule<class r_method_kind, ast::MethodKind>;
using InterfaceDeclRuleType = x3::rule<class r_iface_decl, ast::Interface>;
using DeclRuleType = x3::rule<class r_decl, ast::Declaration>;

// imports / module / file
using ImportRuleType = x3::rule<class r_import, ast::Import>;
using ModuleRuleType = x3::rule<class r_module, ast::Module>;

using ModuleDeclRuleType = x3::rule<class r_module_decl, ast::QualifiedIdentifier>;

// grammar hookup
BOOST_SPIRIT_DECLARE(
    LineCommentRuleType,
    BlockCommentRuleType,
    CommentRuleType,
    SkipperRuleType,
    IdentifierRuleType,
    NameRuleType,
    QualifiedIdentifierRuleType,
    StringLiteralRuleType,
    BooleanLiteralRuleType,
    IntegerLiteralRuleType,
    FloatLiteralRuleType,
    BytesLiteralRuleType,
    ConstValueRuleType,
    PrimitiveTypeRuleType,
    UserTypeRuleType,
    VectorTypeRuleType,
    MapTypeRuleType,
    OptionalTypeRuleType,
    TypeRuleType,
    AttributeRuleType,
    AttributeListRuleType,
    FieldRuleType,
    ParamRuleType,
    ResultRuleType,
    RetFieldRuleType,
    RetFieldsRuleType,
    ConstDeclRuleType,
    EnumItemRuleType,
    EnumDeclRuleType,
    StructDeclRuleType,
    MethodRuleType,
    MethodKindRuleType,
    InterfaceDeclRuleType,
    DeclRuleType,
    ImportRuleType,
    ModuleRuleType,
    ModuleDeclRuleType
)

// clang-format on
} // namespace impl

impl::LineCommentRuleType line_comment();
impl::BlockCommentRuleType block_comment();
impl::CommentRuleType comment();
impl::SkipperRuleType skipper();
impl::IdentifierRuleType identifier();
impl::NameRuleType name();
impl::QualifiedIdentifierRuleType qualified_identifier();
impl::StringLiteralRuleType string_literal();
impl::BooleanLiteralRuleType boolean_literal();
impl::IntegerLiteralRuleType integer_literal();
impl::FloatLiteralRuleType float_literal();
impl::BytesLiteralRuleType bytes_literal();
impl::ConstValueRuleType const_value();
impl::PrimitiveTypeRuleType primitive_type();
impl::UserTypeRuleType user_type();
impl::VectorTypeRuleType vector_type();
impl::MapTypeRuleType map_type();
impl::OptionalTypeRuleType optional_type();
impl::TypeRuleType type();
impl::AttributeRuleType attribute();
impl::AttributeListRuleType attribute_list();
impl::FieldRuleType field();
impl::ParamRuleType param();
impl::ResultRuleType result();
impl::RetFieldRuleType ret_field();
impl::RetFieldsRuleType ret_fields();
impl::ConstDeclRuleType const_decl();
impl::EnumItemRuleType enum_item();
impl::EnumDeclRuleType enum_decl();
impl::StructDeclRuleType struct_decl();
impl::MethodRuleType method();
impl::MethodKindRuleType method_kind();
impl::InterfaceDeclRuleType interface_decl();
impl::DeclRuleType decl();
impl::ImportRuleType import();
impl::ModuleRuleType module();
impl::ModuleDeclRuleType module_decl();

}  // namespace hasten::idl::parser
