#include "idl/config.hpp"
#include "idl/rules_definition.hpp"
#include "idl/error_handler.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

namespace hasten::idl::parser::rule
{

BOOST_SPIRIT_INSTANTIATE(LineComment, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BlockComment, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Comment, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Skipper, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Skipper, iterator_type, boost::spirit::x3::unused_type);
BOOST_SPIRIT_INSTANTIATE(Identifier, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Name, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(QualifiedIdentifier, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(StringLiteral, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BooleanLiteral, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(IntegerLiteral, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(FloatLiteral, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(BytesLiteral, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ConstantValue, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(PrimitiveType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(UserType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(VectorType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(MapType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(OptionalType, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Type, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Attribute, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(AttributeList, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Field, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Parameter, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Result, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ReturnField, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ReturnFields, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Constant, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(EnumItem, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Enum, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Struct, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Method, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(MethodKind, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Interface, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Declaration, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Import, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(ModuleDeclaration, iterator_type, context_type);
BOOST_SPIRIT_INSTANTIATE(Module, iterator_type, context_type);

#define DEFINE_ANNOTATED_RULE(rule_name) \
    struct rule_name##RuleClass : GenericParsingErrorHandler, x3::annotate_on_success {}

DEFINE_ANNOTATED_RULE(ModuleDeclaration);
DEFINE_ANNOTATED_RULE(Interface);
DEFINE_ANNOTATED_RULE(Module);
DEFINE_ANNOTATED_RULE(Import);
DEFINE_ANNOTATED_RULE(Struct);
DEFINE_ANNOTATED_RULE(Enum);
DEFINE_ANNOTATED_RULE(EnumItem);
DEFINE_ANNOTATED_RULE(Field);
DEFINE_ANNOTATED_RULE(Parameter);
DEFINE_ANNOTATED_RULE(ReturnField);
DEFINE_ANNOTATED_RULE(Method);
DEFINE_ANNOTATED_RULE(UserType);
DEFINE_ANNOTATED_RULE(Attribute);

#undef DEFINE_ANNOTATED_RULE

}  // namespace hasten::idl::parser::rule
