#include "idl/config.hpp"

#include <boost/spirit/home/x3.hpp>

namespace hasten::idl::parser::impl
{

BOOST_SPIRIT_INSTANTIATE(LineCommentRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BlockCommentRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(CommentRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(SkipperRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(IdentifierRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(QualifiedIdentifierRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(StringLiteralRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BooleanLiteralRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(IntegerLiteralRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(FloatLiteralRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BytesLiteralRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ConstValueRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(PrimitiveTypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(UserTypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(VectorTypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(MapTypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(OptionalTypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(TypeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(AttributeRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(AttributeListRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(FieldRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ParamRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ResultRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(RetFieldRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(RetFieldsRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ConstDeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(EnumItemRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(EnumDeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(StructDeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(MethodRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(MethodKindRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(InterfaceDeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(DeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ImportRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ModuleDeclRuleType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ModuleRuleType, iterator_type, context_type);

}  // namespace hasten::idl::parser::impl